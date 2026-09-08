#include <Arduino.h>

#include "config.h"
#include "keypad.h"
#include "history.h"
#include "text_input.h"
#include "peripherals.h"
#include "link.h"
#include "ui.h"
#include "power.h"
#include "version.h"
#include "cli.h"

// ===========================================================================
// Тест дальності
// ===========================================================================
static bool rangeActive = false;
static uint8_t rangeSent = 0;
static uint8_t rangeOk = 0;
static float rangeBest = -999;
static float rangeWorst = 999;
static uint32_t rangeNextAt = 0;

static void rangeStart()
{
  rangeActive = true;
  rangeSent = 0;
  rangeOk = 0;
  rangeBest = -999;
  rangeWorst = 999;
  rangeNextAt = 0;
  uiSetStatus("тест 0/" + String(RANGE_TEST_COUNT));
}

static void rangeFinish()
{
  rangeActive = false;
  String s = String(rangeOk) + "/" + String(rangeSent);
  if (rangeOk > 0)
  {
    s += "  " + String(rangeBest, 0) + ".." + String(rangeWorst, 0) + "dBm";
  }
  uiSetStatus(s);
  rangeOk > 0 ? signalDelivered() : signalFailed();
}

// ===========================================================================
// Реакція на події радіо
// ===========================================================================
// RSSI, SNR і тривалість пакета в ефірі — однаково для всіх подій
static String signalInfo()
{
  return String(linkRssi(), 0) + "dBm  " + String(linkSnr(), 0) +
         "dB  " + String(linkAirtimeMs()) + "ms";
}

static void onIncoming(const String &text)
{
  historyAdd(HIST_IN, text);
  powerWake(); // піднімаємо екран, далі звук і вібрація
  uiSetSignal(signalInfo());
  uiSetStatus("нове повідомлення");
  signalIncoming();

  if (uiScreen() == UI_HISTORY)
  {
    uiResetScroll();
  }
}

static void onDelivered(const String &text)
{
  // У стрічку потрапляє лише підтверджене
  historyAdd(HIST_OUT, text);
  uiSetSignal(signalInfo()); // ACK теж прилетів по радіо
  uiSetStatus("доставлено");
  signalDelivered();
}

static void onFailed(const String &text)
{
  (void)text;
  uiSetStatus("не доставлено");
  signalFailed();
}

static void onPong(uint32_t rtt, float local, float remote)
{
  if (rangeActive)
  {
    rangeOk++;
    if (local > rangeBest)
    {
      rangeBest = local;
    }
    if (local < rangeWorst)
    {
      rangeWorst = local;
    }
    uiSetSignal(String(local, 0) + "/" + String(remote, 0) + "dBm");
    uiSetStatus("тест " + String(rangeOk) + "/" + String(rangeSent));
    return;
  }

  // "тут / там" — рівень сигналу з обох боків одразу
  uiSetSignal(String(local, 0) + "/" + String(remote, 0) + "dBm  " +
              String(rtt) + "ms");
  uiSetStatus("ping ok");
  signalDelivered();
}

static void onPingLost()
{
  if (rangeActive)
  {
    uiSetStatus("тест " + String(rangeOk) + "/" + String(rangeSent) + "  --");
    return;
  }
  uiSetStatus("немає відповіді");
  signalFailed();
}

// ===========================================================================
// Реакція на клавіші
// ===========================================================================
// Програмне вимкнення. Регулятор плати лишається під живленням,
// тому це не повне знеструмлення: у глибокому сні плата бере
// одиниці міліампер. Для довгого зберігання все одно
// краще фізичний перемикач або вийняти акумулятор.
static void powerOff()
{
  uiSetStatus("вимкнення");
  uiInvalidate();
  uiTick();
  signalShutdown();

  // Даємо мелодії й вібрації доіграти до сну
  uint32_t until = millis() + 600;
  while (millis() < until)
  {
    periphTick();
    delay(10);
  }

  historyFlush(); // не втратити останні повідомлення
  powerSleepNow();
}

static void trySend()
{
  if (inputDraft().length() == 0)
  {
    return;
  }
  if (linkBusy())
  {
    uiSetStatus("зачекайте ACK");
    return;
  }

  if (linkSend(inputDraft()))
  {
    inputClear();
    uiSetStatus("відправлено");
    signalSent();
  }
  else
  {
    // Чернетку навмисно не стираємо: набране не має пропадати
    uiSetStatus("ефір зайнятий " + String(linkAirWaitMs() / 1000 + 1) + "с");
  }
  uiInvalidate();
}

static void keyInChat(char k)
{
  if (k >= '0' && k <= '9')
  {
    inputDigit(k);
  }
  else if (k == '*')
  {
    inputBackspace();
  }
  else if (k == '#')
  {
    trySend();
  }
  else if (k == 'A')
  {
    inputToggleLayout();
  }
  else if (k == 'B')
  {
    inputClear();
  }
}

static void keyInHistory(char k)
{
  if (k == '2')
  {
    uiScrollUp();
  }
  else if (k == '8')
  {
    uiScrollDown();
  }
  else if (k == '*')
  {
    uiGoChat();
  }
}

static void menuActivate()
{
  switch (uiMenuIndex())
  {
  case MENU_PING:
    if (linkPing())
    {
      uiSetStatus("ping...");
    }
    else
    {
      uiSetStatus("ефір зайнятий " + String(linkAirWaitMs() / 1000 + 1) + "с");
    }
    break;

  case MENU_RANGE:
    rangeStart();
    break;

  case MENU_DEEP:
    powerToggleDeepSleep();
    uiSetDeepSleepLabel(powerDeepSleepEnabled() ? "увімк" : "вимк");
    uiSetStatus(powerDeepSleepEnabled()
                    ? "радіо засне через 30хв"
                    : "радіо слухає завжди");
    break;

  case MENU_INFO:
    // На екрані місця мало, тому лише hash і напруга —
    // решту видно в Serial через versionPrint()
    uiSetStatus(versionShort() + "  " + String(batteryVolts(), 2) + "V");
    break;

  case MENU_CLEAR:
    historyClear();
    uiSetStatus("історію очищено");
    break;

  case MENU_POWEROFF:
    powerOff();
    break;
  }
}

static void keyInMenu(char k)
{
  if (k == '2')
  {
    uiScrollUp();
  }
  else if (k == '8')
  {
    uiScrollDown();
  }
  else if (k == '#')
  {
    menuActivate();
  }
  else if (k == '*')
  {
    rangeActive = false;
    uiGoChat();
  }
}

static void handleKey(char k)
{
  if (k == 'C')
  {
    uiToggleScreen();
    return;
  }

  switch (uiScreen())
  {
  case UI_CHAT:
    keyInChat(k);
    break;
  case UI_HISTORY:
    keyInHistory(k);
    break;
  case UI_MENU:
    keyInMenu(k);
    break;
  }
  uiInvalidate();
}

// ===========================================================================
// setup / loop
// ===========================================================================
void setup()
{
  Serial.begin(115200);
  delay(300);

  powerBegin();
  periphBegin();
  keypadBegin();
  historyBegin();
  uiBegin();

  linkOnMessage(onIncoming);
  linkOnDelivered(onDelivered);
  linkOnFailed(onFailed);
  linkOnPong(onPong);
  linkOnPingLost(onPingLost);

  if (!linkBegin())
  {
    uiFatal("Радіо не стартувало", "див. Serial");
    while (true)
    {
      delay(100);
    }
  }

  uiSetDeepSleepLabel(powerDeepSleepEnabled() ? "увімк" : "вимк");
  uiSetSignal("SF" + String(LORA_SF) + "  " + String(LORA_FREQ, 1) + "MHz  " + String(LORA_POWER) + "dBm");

  versionPrint();
  Serial.printf("history  : %d\n", historySize());
  Serial.printf("battery: %.3f V (на піні %lu mV, CAL %.3f)\n", batteryVolts(), batteryRawMv(), (double)VBAT_CAL);

  signalStartup();
  uiInvalidate();
  cliBegin();
}

void loop()
{
#if HAS_KEYPAD
  char k = keypadPoll();
  if (k)
  {
    signalKey();
    powerNoteActivity();
    handleKey(k);
  }

  // Довге утримання D — вимкнення з будь-якого екрана
  {
    static char holdKey = 0;
    static uint32_t holdSince = 0;

    char cur = keypadHeld();
    if (cur != holdKey)
    {
      holdKey = cur;
      holdSince = millis();
    }
    else if (holdKey == 'D' && holdSince &&
             millis() - holdSince > POWER_HOLD_MS)
    {
      holdSince = 0; // щоб не спрацювало двічі
      powerOff();
    }
  }
#else
  (void)handleKey; // клавіатура ще не припаяна
#endif

  // Тест дальності: серія ping з паузою, статистика в кінці
  if (rangeActive && !linkPinging() && millis() > rangeNextAt)
  {
    if (rangeSent >= RANGE_TEST_COUNT)
    {
      rangeFinish();
    }
    else if (linkPing())
    {
      rangeSent++;
      rangeNextAt = millis() + RANGE_TEST_GAP_MS;
      uiInvalidate();
    }
  }

  linkTick();
  periphTick();
  powerTick();
  historyTick();
  uiTick();
  cliTick();
}
