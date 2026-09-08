#pragma once
#include <Arduino.h>

// Дані про збірку підставляє version.py перед компіляцією.
// Значення нижче — запобіжник на випадок, коли скрипт не відпрацював
// або модуль зібрали в чужому проєкті без нього.

#ifndef BUILD_COMMIT
#define BUILD_COMMIT "unknown"
#endif

#ifndef BUILD_BRANCH
#define BUILD_BRANCH "unknown"
#endif

#ifndef BUILD_DATE
#define BUILD_DATE "unknown"
#endif

#define FIRMWARE_NAME "lora-messenger"

// Короткий рядок для екрана: тільки hash
String versionShort();

// Повний блок у Serial: збірка, вузол, час роботи, вільна пам'ять
void versionPrint();
