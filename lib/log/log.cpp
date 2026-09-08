#include "log.h"
#include <stdarg.h>

static const char *NAMES[] = {"ERROR", "WARN", "INFO", "DEBUG"};
static const uint8_t NAMES_COUNT = sizeof(NAMES) / sizeof(NAMES[0]);

static LogEntry entries[LOG_MAX];
static uint8_t head = 0;
static uint8_t count = 0;

#define LOG_MSG_MAX 96

void logWrite(uint8_t level, const char *fmt, ...)
{
    char msg[LOG_MSG_MAX];

    va_list args;
    va_start(args, fmt);
    // vsnprintf сам обрізає за розміром буфера, тому переповнення
    // неможливе навіть із довгим форматом
    vsnprintf(msg, sizeof(msg), fmt, args);
    va_end(args);

    // Рівень може прийти будь-який: помилка виклику, пошкоджена
    // пам'ять, новий рівень без оновлення таблиці. Вихід за межі
    // масиву тут коштував би падінням у printf.
    const char *name = (level < NAMES_COUNT) ? NAMES[level] : "?";

    // Час від старту: без нього незрозуміло, чи події йшли підряд,
    // чи з паузою в хвилину
    Serial.printf("[%8lu] [%-5s] %s\n", millis(), name, msg);

    // Сюди ж потім стане запис у кільцевий буфер:
    logStore(level, millis(), msg);
}

void logStore(uint8_t level, uint32_t timestamp, const char *msg)
{
    LogEntry &e = entries[head]; // посилання на комірку, без копіювання
    e.level = level;
    e.timestamp = timestamp;
    strncpy(e.text, msg, sizeof(e.text) - 1);
    e.text[sizeof(e.text) - 1] = '\0';

    head = (head + 1) % LOG_MAX;

    if (count < LOG_MAX)
        count++;
}

uint8_t logCount()
{
    return count;
}

const LogEntry &logAt(uint8_t i)
{
    if (i >= count)
        i = count - 1;

    uint8_t index = (head + LOG_MAX - count + i) % LOG_MAX;
    return entries[index];
}

static uint8_t runtimeLevel = LOG_LEVEL;
 
void logSetLevel(uint8_t level) { runtimeLevel = level; }
uint8_t logGetLevel() { return runtimeLevel; }
 
const char *logLevelName(uint8_t level)
{
    return (level < NAMES_COUNT) ? NAMES[level] : "?";
}