#include <Arduino.h>
#include "config.h"

// Діагностика бузера. Перебирає всі способи його розворушити.
// pio run -e buzztest -t upload -t monitor

#define BUZZ_CHANNEL 0

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
  Serial.printf("\n=== BUZZER TEST, pin %d ===\n", PIN_BUZZER);
}

void loop()
{
  // --- 1. Просто високий рівень. Так гудить АКТИВНИЙ бузер ---
  banner("1: digitalWrite HIGH (активний бузер)");
  ledcDetachPin(PIN_BUZZER);
  pinMode(PIN_BUZZER, OUTPUT);
  digitalWrite(PIN_BUZZER, HIGH);
  delay(1500);
  digitalWrite(PIN_BUZZER, LOW);
  delay(700);

  // --- 2. Низький рівень. Так гудить бузер з ІНВЕРСНОЮ логікою ---
  banner("2: digitalWrite LOW (інверсна логіка)");
  digitalWrite(PIN_BUZZER, LOW);
  delay(1500);
  digitalWrite(PIN_BUZZER, HIGH);
  delay(700);

  // --- 3. Ручний меандр без ledc. Так гудить ПАСИВНИЙ бузер ---
  banner("3: ручний меандр 2 кГц (пасивний бузер)");
  digitalWrite(PIN_BUZZER, LOW);
  for (uint32_t i = 0; i < 3000; i++)
  {
    digitalWrite(PIN_BUZZER, HIGH);
    delayMicroseconds(250);
    digitalWrite(PIN_BUZZER, LOW);
    delayMicroseconds(250);
  }
  delay(700);

  // --- 4. Апаратний ШІМ через ledc, розгортка по частоті ---
  banner("4: ledcWriteTone, розгортка 400..3000 Гц");
  ledcSetup(BUZZ_CHANNEL, 2000, 8);
  ledcAttachPin(PIN_BUZZER, BUZZ_CHANNEL);
  for (uint16_t f = 400; f <= 3000; f += 200)
  {
    Serial.printf("    %u Hz\n", f);
    Serial.flush();
    ledcWriteTone(BUZZ_CHANNEL, f);
    delay(220);
  }
  ledcWriteTone(BUZZ_CHANNEL, 0);

  banner("цикл завершено, повтор через 3 с");
  delay(3000);
}
