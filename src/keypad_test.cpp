#include <Arduino.h>
#include "config.h"
#include "keypad.h"

// Діагностика клавіатури. Нічого, крім матриці, не чіпає.
// Заливати через окреме середовище: pio run -e keytest -t upload

static const uint8_t COLS_T[4] = {KEY_COL0, KEY_COL1, KEY_COL2, KEY_COL3};
static const char *COL_NAMES[4] = {"IO15", "IO34", "S_VP", "S_VN"};

void setup()
{
  Serial.begin(115200);
  delay(500);

  Serial.println();
  Serial.println("=== KEYPAD TEST ===");
  Serial.println("У спокої всі стовпці мають читатись як 1.");
  Serial.println("Якщо десь 0 або значення стрибає — немає підтяжки.");
  Serial.println();

  keypadBegin();
}

void loop()
{
  char k = keypadPoll();
  if (k)
  {
    Serial.printf(">>> KEY: %c\n", k);
  }

  // Раз на секунду — стан стовпців у спокої
  static uint32_t next = 0;
  if (millis() > next)
  {
    next = millis() + 1000;

    Serial.print("idle:");
    for (uint8_t i = 0; i < 4; i++)
    {
      Serial.printf("  %s=%d", COL_NAMES[i], digitalRead(COLS_T[i]));
    }
    Serial.println();
  }

  delay(5);
}
