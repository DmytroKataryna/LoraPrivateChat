#pragma once
#include <Arduino.h>

// Радіоканал: пакет, шифрування, підтвердження, повтори, ефірний бюджет.
// Назовні віддає лише текст і події — про SX1276 решта коду не знає.

typedef void (*LinkTextHandler)(const String &text);
typedef void (*LinkVoidHandler)();
typedef void (*LinkPongHandler)(uint32_t rttMs, float localRssi, float remoteRssi);

bool linkBegin();
void linkTick();
void linkSleep(); // приспати SX1276 перед глибоким сном

// false, якщо канал зайнятий очікуванням відповіді або ефірним бюджетом
bool linkSend(const String &text);
bool linkPing();

bool linkBusy();     // чекає ACK на повідомлення
bool linkPinging();  // чекає PONG
uint8_t linkRetries();
float linkRssi();
float linkSnr();
uint32_t linkAirtimeMs(); // скільки останній пакет тривав у ефірі

// Скільки ще мілісекунд треба мовчати за нормою duty cycle
uint32_t linkAirWaitMs();

void linkOnMessage(LinkTextHandler h);   // прийшло повідомлення
void linkOnDelivered(LinkTextHandler h); // наше підтверджено
void linkOnFailed(LinkTextHandler h);    // вичерпані повтори
void linkOnPong(LinkPongHandler h);      // відповідь на ping
void linkOnPingLost(LinkVoidHandler h);  // ping без відповіді
