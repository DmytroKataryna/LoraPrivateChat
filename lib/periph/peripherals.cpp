#include "peripherals.h"
#include "config.h"

#define BUZZ_CHANNEL 0

// Одна нота мелодії. freq == 0 означає паузу.
struct Note
{
  uint16_t freq;
  uint16_t ms;
};

// Короткі паузи між нотами роблять мелодію виразнішою:
// без них сусідні тони зливаються в один суцільний звук.

// Вхідне повідомлення — висхідне арпеджіо, чути навіть у шумі
static const Note MELODY_INCOMING[] = {
    {988, 70}, {0, 25}, {1319, 70}, {0, 25}, {1568, 160}};

// Доставлено — два швидких тони вгору, ненав'язливо
static const Note MELODY_DELIVERED[] = {
    {1319, 55}, {0, 15}, {1760, 90}};

// Не доставлено — низхідне, нижчим регістром
static const Note MELODY_FAILED[] = {
    {440, 130}, {0, 40}, {330, 240}};

// Старт пристрою
static const Note MELODY_STARTUP[] = {
    {1047, 60}, {0, 20}, {1319, 60}, {0, 20}, {1568, 120}};

// Клік по клавіші — дуже коротко, щоб не набридало
static const Note MELODY_KEY[] = {
    {2200, 12}};

// Вимкнення — низхідне, довше за інші, щоб не сплутати
static const Note MELODY_SHUTDOWN[] = {
    {1568, 90}, {0, 20}, {1319, 90}, {0, 20}, {988, 200}};

// Відправлено — коротко вгору, відрізняється від "доставлено"
static const Note MELODY_SENT[] = {
    {1568, 45}, {0, 15}, {1976, 70}};

static const Note *melody = nullptr;
static uint8_t melodyLen = 0;
static uint8_t melodyIdx = 0;
static uint32_t noteUntil = 0;

static Note singleNote[1];
static uint32_t vibroOff = 0;

static void playMelody(const Note *notes, uint8_t len)
{
  melody = notes;
  melodyLen = len;
  melodyIdx = 0;
  noteUntil = 0; // перша нота почнеться на найближчому tick
}

void periphBegin()
{
  pinMode(PIN_BUZZER, OUTPUT);
  ledcSetup(BUZZ_CHANNEL, 2000, 8);
  ledcAttachPin(PIN_BUZZER, BUZZ_CHANNEL);
  ledcWriteTone(BUZZ_CHANNEL, 0);

#if HAS_VIBRO
  pinMode(PIN_VIBRO, OUTPUT);
  digitalWrite(PIN_VIBRO, LOW);
#endif
}

void beep(uint16_t freq, uint16_t ms)
{
  singleNote[0].freq = freq;
  singleNote[0].ms = ms;
  playMelody(singleNote, 1);
}

void vibrate(uint16_t ms)
{
#if HAS_VIBRO
  digitalWrite(PIN_VIBRO, HIGH);

  // Швидкий набір: якщо мотор ще крутиться, не вкорочуємо імпульс,
  // а відсуваємо вимкнення. Інакше при частих натисках ротор
  // не встигає розігнатись і відгук зникає зовсім.
  uint32_t until = millis() + ms;
  if (until > vibroOff)
  {
    vibroOff = until;
  }
#else
  (void)ms;
#endif
}

void signalStartup()
{
  playMelody(MELODY_STARTUP, sizeof(MELODY_STARTUP) / sizeof(Note));
}

void signalIncoming()
{
  playMelody(MELODY_INCOMING, sizeof(MELODY_INCOMING) / sizeof(Note));
  vibrate(250);
}

void signalDelivered()
{
  playMelody(MELODY_DELIVERED, sizeof(MELODY_DELIVERED) / sizeof(Note));
}

void signalFailed()
{
  playMelody(MELODY_FAILED, sizeof(MELODY_FAILED) / sizeof(Note));
  vibrate(400);
}

void signalKey()
{
  playMelody(MELODY_KEY, sizeof(MELODY_KEY) / sizeof(Note));
#if VIBRO_KEY_MS > 0
  vibrate(VIBRO_KEY_MS);
#endif
}

void signalShutdown()
{
  playMelody(MELODY_SHUTDOWN, sizeof(MELODY_SHUTDOWN) / sizeof(Note));
  vibrate(300);
}

void signalSent()
{
  playMelody(MELODY_SENT, sizeof(MELODY_SENT) / sizeof(Note));
  vibrate(VIBRO_SEND_MS);
}

void periphTick()
{
  uint32_t now = millis();

  if (melody && now >= noteUntil)
  {
    if (melodyIdx >= melodyLen)
    {
      ledcWriteTone(BUZZ_CHANNEL, 0);
      melody = nullptr;
    }
    else
    {
      const Note &n = melody[melodyIdx++];
      ledcWriteTone(BUZZ_CHANNEL, n.freq); // 0 = тиша
      noteUntil = now + n.ms;
    }
  }

#if HAS_VIBRO
  if (vibroOff && now > vibroOff)
  {
    digitalWrite(PIN_VIBRO, LOW);
    vibroOff = 0;
  }
#endif
}

float batteryVolts()
{
  // analogReadMilliVolts враховує заводське калібрування АЦП,
  // зашите у eFuse кожного чіпа, і виправляє нелінійність.
  // Це значно точніше за наївне raw/4095*3.3.
  uint32_t sum = 0;
  const uint8_t N = 16;
  for (uint8_t i = 0; i < N; i++)
  {
    sum += analogReadMilliVolts(PIN_VBAT);
  }

  // /1000 у вольти, x2 — дільник на платі, xCAL — ручна поправка
  return (sum / N) / 1000.0f * 2.0f * VBAT_CAL;
}

uint32_t batteryRawMv()
{
  return analogReadMilliVolts(PIN_VBAT);
}
