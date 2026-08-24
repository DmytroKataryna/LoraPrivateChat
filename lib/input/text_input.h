#pragma once
#include <Arduino.h>

// Ввід тексту мультитапом, як на кнопковому телефоні.
// Модуль володіє чернеткою і нічого не знає ні про радіо, ні про екран.

void inputDigit(char digit);   // '0'..'9'
void inputBackspace();
void inputClear();
void inputToggleLayout();

bool inputLayoutIsUa();
const String &inputDraft();
