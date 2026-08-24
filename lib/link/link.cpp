#include "link.h"
#include "config.h"

#include <SPI.h>
#include <RadioLib.h>
#include <mbedtls/aes.h>
#include <esp_random.h>

// ===========================================================================
// Формат пакета
// ===========================================================================
#define PKT_MAGIC 0xA5
#define PKT_VERSION 2

enum : uint8_t
{
  TYPE_MSG = 0,
  TYPE_ACK = 1,
  TYPE_PING = 2,
  TYPE_PONG = 3
};

#pragma pack(push, 1)
struct Header
{
  uint8_t magic;
  uint8_t version;
  uint8_t src;
  uint8_t dst;
  uint8_t type;
  uint16_t session; // випадковий при кожному старті
  uint16_t seq;
};
#pragma pack(pop)

// Заголовок лишається відкритим — по ньому фільтруємо чужі пакети
// й відповідаємо ACK. Шифрується тільки корисне навантаження.
// Перед шифруванням до нього додається дві байти контрольної суми:
// якщо ключ не збігається, розшифроване не пройде перевірку.
#define CRC_LEN 2

// ===========================================================================
// Стан
// ===========================================================================
static SX1276 radio = new Module(LORA_CS, LORA_DIO0, LORA_RST, LORA_DIO1);

static volatile bool irqFlag = false;

static uint16_t sessionId = 0;
static uint16_t txSeq = 1;

static bool waitingAck = false;
static uint16_t pendingSeq = 0;
static String pendingText = "";
static uint8_t retries = 0;
static uint32_t ackDeadline = 0;

static bool waitingPong = false;
static uint16_t pingSeq = 0;
static uint32_t pingSentAt = 0;
static uint32_t pingDeadline = 0;

static uint16_t lastRxSeq = 0xFFFF;
static float rssi = 0;
static float snr = 0;
static uint32_t airtimeMs = 0;

static uint32_t nextTxAllowed = 0;

static LinkTextHandler onMessage = nullptr;
static LinkTextHandler onDelivered = nullptr;
static LinkTextHandler onFailed = nullptr;
static LinkPongHandler onPong = nullptr;
static LinkVoidHandler onPingLost = nullptr;

static const uint8_t cryptoKey[16] = CRYPTO_KEY;

// ===========================================================================
// Шифрування: AES-128 у режимі CTR
// ===========================================================================
// CTR перетворює блоковий шифр на потоковий, тому довжина
// не змінюється й не треба доповнювати дані до розміру блока.
static void cryptPayload(const Header &h, uint8_t *data, size_t len)
{
  if (len == 0)
  {
    return;
  }

  // Лічильник має бути унікальним для кожного пакета під одним ключем.
  // session випадковий при старті, тому seq після перезавантаження
  // не повторює старі значення.
  uint8_t nonce[16] = {0};
  nonce[0] = (uint8_t)(h.session & 0xFF);
  nonce[1] = (uint8_t)(h.session >> 8);
  nonce[2] = (uint8_t)(h.seq & 0xFF);
  nonce[3] = (uint8_t)(h.seq >> 8);
  nonce[4] = h.src;
  nonce[5] = h.dst;
  nonce[6] = h.type;

  uint8_t stream[16] = {0};
  size_t off = 0;

  mbedtls_aes_context ctx;
  mbedtls_aes_init(&ctx);
  mbedtls_aes_setkey_enc(&ctx, cryptoKey, 128);
  mbedtls_aes_crypt_ctr(&ctx, len, &off, nonce, stream, data, data);
  mbedtls_aes_free(&ctx);
}

static uint16_t checksum(const uint8_t *d, size_t len)
{
  uint16_t sum = 0x1D0F;
  for (size_t i = 0; i < len; i++)
  {
    sum = (uint16_t)(sum * 31 + d[i]);
  }
  return sum;
}

// ===========================================================================
// Внутрішнє
// ===========================================================================
static void IRAM_ATTR onRadioEvent()
{
  irqFlag = true; // в ISR нічого важчого робити не можна
}

static bool sendRaw(uint8_t type, uint16_t seq, const String &text)
{
  if (millis() < nextTxAllowed)
  {
    return false;
  }

  Header h;
  h.magic = PKT_MAGIC;
  h.version = PKT_VERSION;
  h.src = NODE_ID;
  h.dst = PEER_ID;
  h.type = type;
  h.session = sessionId;
  h.seq = seq;

  uint8_t buf[sizeof(Header) + CRC_LEN + TXT_MAX];
  memcpy(buf, &h, sizeof(h));

  size_t len = text.length();
  if (len > TXT_MAX)
  {
    len = TXT_MAX;
  }

  uint8_t *payload = buf + sizeof(Header);
  uint16_t crc = checksum((const uint8_t *)text.c_str(), len);
  payload[0] = (uint8_t)(crc & 0xFF);
  payload[1] = (uint8_t)(crc >> 8);
  memcpy(payload + CRC_LEN, text.c_str(), len);

  cryptPayload(h, payload, CRC_LEN + len);

  uint32_t t0 = millis();
  int state = radio.transmit(buf, sizeof(Header) + CRC_LEN + len);
  uint32_t airtime = millis() - t0;

  airtimeMs = airtime;
  nextTxAllowed = millis() + airtime * (100 / DUTY_CYCLE_PERCENT);

  // Радіо напівдуплексне: поки передаємо — не чуємо.
  // Без цього рядка приймач оглухне назавжди.
  radio.startReceive();

  if (state != RADIOLIB_ERR_NONE)
  {
    Serial.printf("TX failed, code %d\n", state);
    return false;
  }
  return true;
}

static void handleIncoming()
{
  uint8_t buf[256];
  size_t len = radio.getPacketLength();
  int state = radio.readData(buf, len);

  radio.startReceive();

  if (state != RADIOLIB_ERR_NONE || len < sizeof(Header) + CRC_LEN)
  {
    return;
  }

  Header h;
  memcpy(&h, buf, sizeof(h));

  // Чужі й биті пакети мовчки ігноруємо
  if (h.magic != PKT_MAGIC || h.version != PKT_VERSION || h.dst != NODE_ID)
  {
    return;
  }

  rssi = radio.getRSSI();
  snr = radio.getSNR();

  // Тривалість пакета в ефірі: скільки радіо реально працювало.
  // Залежить від SF, смуги та довжини — корисно бачити при
  // підборі параметрів і при розрахунку ефірного бюджету.
  airtimeMs = radio.getTimeOnAir(len) / 1000;

  uint8_t *payload = buf + sizeof(Header);
  size_t plen = len - sizeof(Header);
  cryptPayload(h, payload, plen);

  size_t textLen = plen - CRC_LEN;
  uint16_t got = (uint16_t)payload[0] | ((uint16_t)payload[1] << 8);
  if (got != checksum(payload + CRC_LEN, textLen))
  {
    Serial.println("bad key or corrupted payload");
    return;
  }

  String text = "";
  for (size_t i = 0; i < textLen; i++)
  {
    text += (char)payload[CRC_LEN + i];
  }

  switch (h.type)
  {
  case TYPE_ACK:
    if (waitingAck && h.seq == pendingSeq)
    {
      waitingAck = false;
      if (onDelivered)
      {
        onDelivered(pendingText);
      }
    }
    return;

  case TYPE_PING:
    // Відповідаємо своїм рівнем сигналу, щоб інший бік бачив обидва
    sendRaw(TYPE_PONG, h.seq, String(rssi, 0));
    return;

  case TYPE_PONG:
    if (waitingPong && h.seq == pingSeq)
    {
      waitingPong = false;
      if (onPong)
      {
        onPong(millis() - pingSentAt, rssi, text.toFloat());
      }
    }
    return;

  default: // TYPE_MSG
    break;
  }

  // Підтверджуємо завжди: попередній ACK міг загубитись
  sendRaw(TYPE_ACK, h.seq, "");

  if (h.seq == lastRxSeq)
  {
    return; // дублікат, наверх не віддаємо
  }
  lastRxSeq = h.seq;

  if (onMessage)
  {
    onMessage(text);
  }
}

// ===========================================================================
// Публічне
// ===========================================================================
bool linkBegin()
{
  sessionId = (uint16_t)(esp_random() & 0xFFFF);

  // Дефолтні SPI-піни ESP32 не збігаються з розводкою LilyGO,
  // тому шину ініціалізуємо явно ДО radio.begin()
  SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_CS);

  int state = radio.begin(LORA_FREQ, LORA_BW, LORA_SF, LORA_CR,
                          LORA_SYNC, LORA_POWER, LORA_PREAMBLE);
  if (state != RADIOLIB_ERR_NONE)
  {
    Serial.printf("SX1276 init failed, code %d\n", state);
    return false;
  }

  // Неперервний прийом через переривання на DIO0.
  // Блокуючий receive() губить пакети, які прилітають
  // у проміжки між приймальними вікнами.
  radio.setPacketReceivedAction(onRadioEvent);
  radio.startReceive();
  return true;
}

bool linkSend(const String &text)
{
  if (text.length() == 0 || waitingAck)
  {
    return false;
  }

  pendingText = text;
  pendingSeq = txSeq++;
  retries = 0;

  if (!sendRaw(TYPE_MSG, pendingSeq, pendingText))
  {
    return false;
  }

  waitingAck = true;
  ackDeadline = millis() + ACK_TIMEOUT_MS;
  return true;
}

bool linkPing()
{
  if (waitingPong)
  {
    return false;
  }

  pingSeq = txSeq++;
  if (!sendRaw(TYPE_PING, pingSeq, ""))
  {
    return false;
  }

  waitingPong = true;
  pingSentAt = millis();
  pingDeadline = pingSentAt + PING_TIMEOUT_MS;
  return true;
}

void linkTick()
{
  if (irqFlag)
  {
    irqFlag = false;
    handleIncoming();
  }

  if (waitingPong && millis() > pingDeadline)
  {
    waitingPong = false;
    if (onPingLost)
    {
      onPingLost();
    }
  }

  if (!waitingAck || millis() <= ackDeadline)
  {
    return;
  }

  if (retries >= MAX_RETRIES)
  {
    waitingAck = false;
    if (onFailed)
    {
      onFailed(pendingText);
    }
    return;
  }

  if (sendRaw(TYPE_MSG, pendingSeq, pendingText))
  {
    retries++;
    ackDeadline = millis() + ACK_TIMEOUT_MS;
  }
  else
  {
    // Ефір ще в паузі — не спалюємо спробу даремно
    ackDeadline = nextTxAllowed + ACK_TIMEOUT_MS;
  }
}

void linkSleep()
{
  radio.sleep();
}

bool linkBusy() { return waitingAck; }
bool linkPinging() { return waitingPong; }
uint8_t linkRetries() { return retries; }
float linkRssi() { return rssi; }
float linkSnr() { return snr; }
uint32_t linkAirtimeMs() { return airtimeMs; }

uint32_t linkAirWaitMs()
{
  uint32_t now = millis();
  return now < nextTxAllowed ? nextTxAllowed - now : 0;
}

void linkOnMessage(LinkTextHandler h) { onMessage = h; }
void linkOnDelivered(LinkTextHandler h) { onDelivered = h; }
void linkOnFailed(LinkTextHandler h) { onFailed = h; }
void linkOnPong(LinkPongHandler h) { onPong = h; }
void linkOnPingLost(LinkVoidHandler h) { onPingLost = h; }
