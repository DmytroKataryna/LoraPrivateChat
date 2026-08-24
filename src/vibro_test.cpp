#include <Arduino.h>
#include "config.h"

// Діагностика вібромотора. Нічого, крім одного піна, не чіпає.
// pio run -e vibrotest -t upload -t monitor
//
// Модуль має драйвер на борту, тому GPIO керує лише транзистором.
// Живлення модуля бери з батареї або 5V: стартова напруга
// коінового мотора близько 3.7 В, від 3.3 В він може не рушити.

#define VIBRO_CHANNEL 1 // канал 0 зайнятий бузером

static void banner(const char *s)
{
  Serial.println();
  Serial.print(">>> ");
  Serial.println(s);
  Serial.flush();
}

void setup()
{
  Serial.begin(115200);
  delay(500);
  Serial.printf("\n=== VIBRO TEST, pin %d ===\n", PIN_VIBRO);

  pinMode(PIN_VIBRO, OUTPUT);
  digitalWrite(PIN_VIBRO, LOW);
}

void loop()
{
  // --- 1. Просте вмикання. Так має працювати звичайний модуль ---
  banner("1: HIGH на 1.5 с");
  ledcDetachPin(PIN_VIBRO);
  pinMode(PIN_VIBRO, OUTPUT);
  digitalWrite(PIN_VIBRO, HIGH);
  delay(1500);
  digitalWrite(PIN_VIBRO, LOW);
  delay(1000);

  // --- 2. Інверсна логіка: деякі драйвери вмикаються нулем ---
  banner("2: LOW на 1.5 с (інверсна логіка)");
  digitalWrite(PIN_VIBRO, LOW);
  delay(1500);
  digitalWrite(PIN_VIBRO, HIGH);
  delay(300);
  digitalWrite(PIN_VIBRO, LOW);
  delay(1000);

  // --- 3. Короткі імпульси, як реальне сповіщення ---
  banner("3: три імпульси по 250 мс");
  for (uint8_t i = 0; i < 3; i++)
  {
    digitalWrite(PIN_VIBRO, HIGH);
    delay(250);
    digitalWrite(PIN_VIBRO, LOW);
    delay(200);
  }
  delay(1000);

  // --- 4. ШІМ: підбір мінімальної потужності ---
  // Мотор має інерцію, тому на малій скважності не рушить із місця.
  // Тут видно, з якої саме він оживає — корисно, щоб потім
  // вібрувати тихіше й економити батарею.
  banner("4: ШІМ, скважність 10..100%");
  ledcSetup(VIBRO_CHANNEL, 200, 8);
  ledcAttachPin(PIN_VIBRO, VIBRO_CHANNEL);
  for (uint8_t pct = 10; pct <= 100; pct += 10)
  {
    Serial.printf("    %u%%\n", pct);
    Serial.flush();
    ledcWrite(VIBRO_CHANNEL, (255 * pct) / 100);
    delay(600);
  }
  ledcWrite(VIBRO_CHANNEL, 0);
  ledcDetachPin(PIN_VIBRO);
  pinMode(PIN_VIBRO, OUTPUT);
  digitalWrite(PIN_VIBRO, LOW);

  // --- 5. Пошук мінімальної відчутної тривалості ---
  // Мотор має інерцію: короткий імпульс не встигає розкрутити ротор.
  // Запам'ятай перше значення, яке реально відчувається рукою,
  // і постав його у VIBRO_KEY_MS.
  banner("5: тривалість 40..300 мс");
  pinMode(PIN_VIBRO, OUTPUT);
  for (uint16_t ms = 40; ms <= 300; ms += 20)
  {
    Serial.printf("    %u мс\n", ms);
    Serial.flush();
    digitalWrite(PIN_VIBRO, HIGH);
    delay(ms);
    digitalWrite(PIN_VIBRO, LOW);
    delay(900); // пауза, щоб імпульси не зливалися
  }

  banner("цикл завершено, повтор через 3 с");
  delay(3000);
}