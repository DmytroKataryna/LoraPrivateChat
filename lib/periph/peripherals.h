#pragma once
#include <Arduino.h>

// Бузер, вібромотор і замір напруги акумулятора.
// Звук неблокуючий: signal* лише ставить мелодію в чергу,
// а програє її periphTick() із loop().

void periphBegin();
void periphTick();

void beep(uint16_t freq, uint16_t ms);
void vibrate(uint16_t ms);

// Готові сигнали — щоб не розкидати ноти по коду
void signalStartup();   // старт пристрою
void signalIncoming();  // прийшло повідомлення
void signalDelivered(); // наше підтверджено
void signalFailed();    // вичерпані повтори
void signalKey();       // натиск клавіші
void signalSent();      // повідомлення пішло в ефір
void signalShutdown();  // вимкнення пристрою

float batteryVolts();
uint32_t batteryRawMv(); // напруга на піні до множення, для калібрування
