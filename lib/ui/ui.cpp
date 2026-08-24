#include "ui.h"
#include "config.h"
#include "history.h"
#include "text_input.h"
#include "peripherals.h"

#include <Wire.h>
#include <U8g2lib.h>

// Пін скидання не вказуємо: на цій ревізії передача GPIO 16
// вішає шину I2C і плата йде в цикл ресетів по сторожовому таймеру.
static U8G2_SSD1306_128X64_NONAME_F_HW_I2C oled(U8G2_R0, U8X8_PIN_NONE);

// Дрібний шрифт для списків, великий для рядка вводу.
// 5x8 дає 25 символів у рядку замість 21 і звільняє місце
// під третій рядок стрічки.
#define FONT_SMALL u8g2_font_5x8_t_cyrillic
#define FONT_BIG u8g2_font_6x12_t_cyrillic

#define CHAT_ROWS 4
#define HISTORY_ROWS 5
#define MENU_VISIBLE 4
#define SCREEN_W 128

static const char *MENU_TITLES[MENU_COUNT] = {
    "Ping",
    "Дальність",
    "Повний сон",
    "Про пристрій",
    "Стерти історію",
    "Вимкнути"};

static UiScreen screen = UI_CHAT;
static uint8_t scrollPos = 0;
static uint8_t menuPos = 0;
static String status = "готовий";
static String deepLabel = "";
static String signalLine = "";
static uint32_t statusAt = 0;

// Скільки статус перекриває рядок дальності
#define STATUS_HOLD_MS 3000
static bool dirty = true;
static uint32_t lastDraw = 0;

// ===========================================================================
// Допоміжне
// ===========================================================================
// Обрізає рядок по цілих символах UTF-8, поки він не влізе в ширину
static String fitWidth(const String &s, uint8_t maxPx)
{
  String out = s;
  while (out.length() > 0 && oled.getUTF8Width(out.c_str()) > maxPx)
  {
    int i = out.length() - 1;
    while (i > 0 && ((uint8_t)out[i] & 0xC0) == 0x80)
    {
      i--;
    }
    out.remove(i);
  }
  return out;
}

// Ріже рядок на частини, що влазять у ширину.
// Переносимо по символах, а не по словах: повідомлення короткі,
// а слово, довше за екран, інакше загубилося б цілком.
static uint8_t wrapText(const String &s, uint8_t maxPx,
                        String *out, uint8_t maxLines)
{
  uint8_t n = 0;
  int i = 0;
  int len = s.length();

  while (i < len && n < maxLines)
  {
    int start = i;
    String line = "";

    while (i < len)
    {
      uint8_t c = (uint8_t)s[i];
      int cl = 1;
      if ((c & 0xE0) == 0xC0)
      {
        cl = 2;
      }
      else if ((c & 0xF0) == 0xE0)
      {
        cl = 3;
      }
      else if ((c & 0xF8) == 0xF0)
      {
        cl = 4;
      }

      String cand = line + s.substring(i, i + cl);
      if (oled.getUTF8Width(cand.c_str()) > maxPx)
      {
        break;
      }
      line = cand;
      i += cl;
    }

    if (i == start)
    {
      break; // не влазить навіть один символ
    }
    out[n++] = line;
  }
  return n;
}

static void row(int y, const String &s)
{
  oled.drawUTF8(0, y, fitWidth(s, SCREEN_W).c_str());
}

static String entryLine(const HistoryEntry &e)
{
  // Вузлів лише два, тому напрямок однозначно визначає відправника
  // і зберігати його в самому записі не потрібно.
  return String(e.dir == HIST_IN ? PEER_NAME : NODE_NAME) + ": " + e.text;
}

// ===========================================================================
// Екрани
// ===========================================================================
#define DRAFT_PREFIX_W 22

static void drawChat()
{
  String lines[8];
  oled.setFont(FONT_SMALL);

  // --- 1. Сигнал: RSSI, SNR і тривалість пакета в ефірі ---
  bool fresh = (millis() - statusAt) < STATUS_HOLD_MS;
  if (fresh)
  {
    row(8, status);
  }
  else
  {
    row(8, signalLine);
  }

  // --- 2. Останнє повідомлення, до двох рядків ---
  if (historySize() > 0)
  {
    uint8_t n = wrapText(entryLine(historyAt(0)), SCREEN_W, lines, 2);
    int y = 20;
    for (uint8_t i = 0; i < n; i++)
    {
      oled.drawUTF8(0, y, lines[i].c_str());
      y += 10;
    }
  }
  else
  {
    oled.drawUTF8(0, 20, "порожньо");
  }

  // --- 3. Розділювач ---
  oled.drawHLine(0, 33, SCREEN_W);

  // --- 4. Мова і ввід у два рядки ---
  oled.setFont(FONT_BIG);
  oled.drawUTF8(0, 45, inputLayoutIsUa() ? "UA" : "EN");

  uint8_t n = wrapText(inputDraft(), SCREEN_W - DRAFT_PREFIX_W, lines, 8);
  if (n == 0)
  {
    oled.drawUTF8(DRAFT_PREFIX_W, 45, "_");
  }
  else
  {
    // Показуємо хвіст: те місце, де зараз друкуєш
    uint8_t from = n > 2 ? n - 2 : 0;
    int y = 45;
    for (uint8_t i = from; i < n; i++)
    {
      oled.drawUTF8(DRAFT_PREFIX_W, y, lines[i].c_str());
      y += 13;
    }
  }
}

static void drawHistory()
{
  oled.setFont(FONT_SMALL);
  uint8_t total = historySize();

  char top[40];
  snprintf(top, sizeof(top), "Історія  %d/%d",
           total ? scrollPos + 1 : 0, total);
  row(8, top);
  oled.drawHLine(0, 11, SCREEN_W);

  if (total == 0)
  {
    row(32, "порожньо");
    return;
  }

  int y = 21;
  for (uint8_t i = 0; i < HISTORY_ROWS; i++)
  {
    uint8_t idx = scrollPos + i;
    if (idx >= total)
    {
      break;
    }
    row(y, entryLine(historyAt(idx)));
    y += 10;
  }
}

static void drawMenu()
{
  oled.setFont(FONT_SMALL);

  char top[24];
  snprintf(top, sizeof(top), "Меню  %d/%d", menuPos + 1, MENU_COUNT);
  row(8, top);
  oled.drawHLine(0, 11, SCREEN_W);

  // Із заголовком на екран влазить чотири пункти,
  // решту показує вікно прокрутки за курсором
  uint8_t first = 0;
  if (menuPos >= MENU_VISIBLE)
  {
    first = menuPos - MENU_VISIBLE + 1;
  }

  int y = 20;
  for (uint8_t i = first; i < MENU_COUNT && i < first + MENU_VISIBLE; i++)
  {
    String line = (i == menuPos ? "> " : "  ");
    line += MENU_TITLES[i];
    if (i == MENU_DEEP && deepLabel.length())
    {
      line += ": " + deepLabel;
    }
    row(y, line);
    y += 10;
  }

  oled.drawHLine(0, 53, SCREEN_W);
  row(61, status);
}

// ===========================================================================
// Публічне
// ===========================================================================
void uiBegin()
{
  Wire.begin(OLED_SDA, OLED_SCL);
  oled.begin();
  oled.setFont(FONT_SMALL);
}

void uiInvalidate() { dirty = true; }

void uiSleep()
{
  oled.setPowerSave(1);
}

void uiWake()
{
  oled.setPowerSave(0);
  dirty = true;
}

void uiSetStatus(const String &s)
{
  status = s;
  statusAt = millis();
  dirty = true;
}

void uiSetSignal(const String &s)
{
  signalLine = s;
  dirty = true;
}

UiScreen uiScreen() { return screen; }

void uiSetScreen(UiScreen s)
{
  screen = s;
  scrollPos = 0;
  dirty = true;
}

void uiToggleScreen()
{
  // Екран вводу часу в цикл не входить — у нього заходять з меню
  screen = (UiScreen)((screen + 1) % 3);
  scrollPos = 0;
  dirty = true;
}

void uiGoChat()
{
  screen = UI_CHAT;
  scrollPos = 0;
  dirty = true;
}

void uiScrollUp()
{
  if (screen == UI_MENU)
  {
    menuPos = (menuPos + MENU_COUNT - 1) % MENU_COUNT;
  }
  else if (scrollPos > 0)
  {
    scrollPos--;
  }
  dirty = true;
}

void uiScrollDown()
{
  if (screen == UI_MENU)
  {
    menuPos = (menuPos + 1) % MENU_COUNT;
  }
  else
  {
    uint8_t total = historySize();
    if (total > HISTORY_ROWS && scrollPos < total - HISTORY_ROWS)
    {
      scrollPos++;
    }
  }
  dirty = true;
}

void uiResetScroll()
{
  scrollPos = 0;
  dirty = true;
}

uint8_t uiMenuIndex() { return menuPos; }

void uiSetDeepSleepLabel(const String &s)
{
  deepLabel = s;
  dirty = true;
}

void uiFatal(const String &line1, const String &line2)
{
  oled.clearBuffer();
  oled.setFont(FONT_SMALL);
  row(30, line1);
  row(42, line2);
  oled.sendBuffer();
}

void uiTick()
{
  // Перемальовуємо не частіше 10 разів на секунду
  if (!dirty || millis() - lastDraw < 100)
  {
    return;
  }

  oled.clearBuffer();

  if (screen == UI_CHAT)
  {
    drawChat();
  }
  else if (screen == UI_HISTORY)
  {
    drawHistory();
  }
  else
  {
    drawMenu();
  }

  oled.sendBuffer();
  lastDraw = millis();
  dirty = false;
}
