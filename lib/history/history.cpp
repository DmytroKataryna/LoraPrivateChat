#include "history.h"
#include <Preferences.h>

#define NVS_NAMESPACE "msglog"
#define NVS_KEY "ring"
#define BLOB_VERSION 3

// Не пишемо у флеш на кожне повідомлення: чекаємо паузу.
// Флеш має обмежений ресурс перезапису, а під час активної
// переписки записів було б багато й підряд.
#define FLUSH_DELAY_MS 3000

static Preferences prefs;
static HistoryBlob blob;
static bool dirty = false;
static uint32_t dirtyAt = 0;

static void resetBlob()
{
  memset(&blob, 0, sizeof(blob));
  blob.version = BLOB_VERSION;
}

void historyBegin()
{
  prefs.begin(NVS_NAMESPACE, false);

  size_t stored = prefs.getBytesLength(NVS_KEY);
  if (stored == sizeof(HistoryBlob))
  {
    prefs.getBytes(NVS_KEY, &blob, sizeof(blob));
    // Якщо колись зміниш структуру — старі дані просто відкинуться
    if (blob.version != BLOB_VERSION || blob.count > HISTORY_MAX ||
        blob.head >= HISTORY_MAX)
    {
      resetBlob();
    }
  }
  else
  {
    resetBlob();
  }
}

void historyAdd(uint8_t dir, const String &text)
{
  HistoryEntry &e = blob.items[blob.head];
  e.dir = dir;

  size_t n = text.length();
  if (n > HISTORY_TEXT_MAX)
  {
    n = HISTORY_TEXT_MAX;
    // Не ріжемо кириличну літеру навпіл: відкочуємось
    // через продовжувальні байти 10xxxxxx
    while (n > 0 && ((uint8_t)text[n] & 0xC0) == 0x80)
    {
      n--;
    }
  }
  memcpy(e.text, text.c_str(), n);
  e.text[n] = '\0';

  blob.head = (blob.head + 1) % HISTORY_MAX;
  if (blob.count < HISTORY_MAX)
  {
    blob.count++;
  }

  dirty = true;
  dirtyAt = millis();
}

uint8_t historySize()
{
  return blob.count;
}

const HistoryEntry &historyAt(uint8_t i)
{
  int idx = (int)blob.head - 1 - (int)i;
  while (idx < 0)
  {
    idx += HISTORY_MAX;
  }
  return blob.items[idx];
}

void historyClear()
{
  resetBlob();
  dirty = true;
  historyFlush();
}

void historyFlush()
{
  if (!dirty)
  {
    return;
  }
  prefs.putBytes(NVS_KEY, &blob, sizeof(blob));
  dirty = false;
}

void historyTick()
{
  if (dirty && millis() - dirtyAt > FLUSH_DELAY_MS)
  {
    historyFlush();
  }
}
