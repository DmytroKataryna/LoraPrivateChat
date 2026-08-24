#pragma once
#include <Arduino.h>

// Енергозбереження в два кроки.
//
// ACTIVE   — все увімкнено.
// STANDBY  — екран погашено, процесор на зниженій частоті,
//            радіо СЛУХАЄ. Вхідне повідомлення будить екран,
//            дає звук і вібрацію. Це основний режим очікування.
// DEEP     — глибокий сон, вимкнено все, включно з радіо.
//            Повідомлення не приймаються. Вмикається в меню.
//            Прокидання клавішами * 0 # D (лінія GPIO 15).

void powerBegin();
void powerTick();

void powerNoteActivity(); // натиск клавіші: скидає таймери
void powerWake();         // вхідне повідомлення: підняти екран

bool powerIsStandby();
bool powerDeepSleepEnabled();
void powerToggleDeepSleep();
void powerSleepNow();
