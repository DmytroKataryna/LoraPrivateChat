#include "keypad.h"
#include "config.h"

static const uint8_t ROWS[4] = {KEY_ROW0, KEY_ROW1, KEY_ROW2, KEY_ROW3};
static const uint8_t COLS[4] = {KEY_COL0, KEY_COL1, KEY_COL2, KEY_COL3};

// Роз'єм клавіатури підключений дзеркально: лінії, які код сканує
// як рядки, фізично є стовпцями, і порядок в обох групах зворотний.
// Матриця симетрична, тому переробляти монтаж не треба —
// достатньо розвернути таблицю символів.
static const char KEYMAP[4][4] = {
    {'D', 'C', 'B', 'A'},
    {'#', '9', '6', '3'},
    {'0', '8', '5', '2'},
    {'*', '7', '4', '1'}};

static char rawPrev = 0;
static char stableKey = 0;
static uint32_t lastChange = 0;

// Піни 34..39 фізично не мають внутрішніх підтяжок — тільки INPUT.
// Решта стовпців обходяться внутрішньою.
static void setColMode(uint8_t pin)
{
  if (pin >= 34)
  {
    pinMode(pin, INPUT);
  }
  else
  {
    pinMode(pin, INPUT_PULLUP);
  }
}

void keypadBegin()
{
  // Рядки в спокої — високий імпеданс. Це рятує від короткого
  // замикання, якщо натиснути дві клавіші в одному стовпці.
  for (uint8_t i = 0; i < 4; i++)
  {
    pinMode(ROWS[i], INPUT);
  }
  for (uint8_t i = 0; i < 4; i++)
  {
    setColMode(COLS[i]);
  }
}

static char scanRaw()
{
  for (uint8_t r = 0; r < 4; r++)
  {
    pinMode(ROWS[r], OUTPUT);
    digitalWrite(ROWS[r], LOW);
    delayMicroseconds(10); // час на розряд паразитної ємності

    for (uint8_t c = 0; c < 4; c++)
    {
      if (digitalRead(COLS[c]) == LOW)
      {
        pinMode(ROWS[r], INPUT);
        return KEYMAP[r][c];
      }
    }

    pinMode(ROWS[r], INPUT);
  }
  return 0;
}

char keypadPoll()
{
  char raw = scanRaw();
  uint32_t now = millis();

  // Сигнал змінився — почати відлік стабільності заново
  if (raw != rawPrev)
  {
    rawPrev = raw;
    lastChange = now;
    return 0;
  }

  if (now - lastChange < KEY_DEBOUNCE_MS)
  {
    return 0;
  }

  if (raw == stableKey)
  {
    return 0;
  }

  stableKey = raw;
  return stableKey; // 0 при відпусканні, символ при натиску
}

char keypadHeld()
{
  return stableKey;
}
