#pragma once
#include <Arduino.h>

// Усе, що стосується OLED. Модуль сам бере чернетку з text_input
// та стрічку з history, тому назовні потрібен лише статус і навігація.

enum UiScreen : uint8_t
{
  UI_CHAT = 0,
  UI_HISTORY = 1,
  UI_MENU = 2
};

// Пункти меню — дії виконує main.cpp за індексом
enum UiMenuItem : uint8_t
{
  MENU_PING = 0,
  MENU_RANGE = 1,
  MENU_DEEP = 2,
  MENU_INFO = 3,
  MENU_CLEAR = 4,
  MENU_POWEROFF = 5,
  MENU_COUNT = 6
};

void uiBegin();
void uiTick();
void uiSleep(); // погасити панель
void uiWake();  // підняти панель

void uiInvalidate();
void uiSetStatus(const String &s);

// Рядок дальності: RSSI і SNR останнього пакета.
// Ставить main, щоб ui не залежав від link.
void uiSetSignal(const String &s);

UiScreen uiScreen();
void uiSetScreen(UiScreen s);
void uiToggleScreen();
void uiGoChat();

void uiScrollUp();
void uiScrollDown();
void uiResetScroll();

uint8_t uiMenuIndex();

// Підпис стану біля пункту "Повний сон". Ставить main —
// так ui не залежить від power, і залежність між
// бібліотеками лишається односторонньою.
void uiSetDeepSleepLabel(const String &s);

// Показати фатальну помилку й лишитись на екрані
void uiFatal(const String &line1, const String &line2);
