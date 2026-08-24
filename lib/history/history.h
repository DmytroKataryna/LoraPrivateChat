#pragma once
#include <Arduino.h>

// Історія листування в NVS — вбудованій флеш-пам'яті ESP32.
// SD-карта для цього не потрібна: двадцять повідомлень
// займають менше трьох кілобайт.

#define HISTORY_MAX 20
#define HISTORY_TEXT_MAX 120

enum : uint8_t
{
  HIST_IN = 0,  // отримане
  HIST_OUT = 1  // відправлене й підтверджене
};

#pragma pack(push, 1)
struct HistoryEntry
{
  uint8_t dir;
  char text[HISTORY_TEXT_MAX + 1];
};

struct HistoryBlob
{
  uint8_t version;
  uint8_t count;
  uint8_t head; // куди ляже наступний запис
  HistoryEntry items[HISTORY_MAX];
};
#pragma pack(pop)

void historyBegin();
void historyAdd(uint8_t dir, const String &text);
uint8_t historySize();

// Індекс 0 — найновіше повідомлення
const HistoryEntry &historyAt(uint8_t i);

void historyClear();

// Запис відкладений: викликати в loop(), реальний putBytes
// станеться через кілька секунд після останньої зміни
void historyTick();
void historyFlush();
