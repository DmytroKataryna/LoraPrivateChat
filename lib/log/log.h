#pragma once
#include <Arduino.h>

#define LOG_ERROR 0
#define LOG_WARN  1
#define LOG_INFO  2
#define LOG_DEBUG 3

#ifndef LOG_LEVEL
#define LOG_LEVEL LOG_INFO
#endif

void logWrite(uint8_t level, const char *fmt, ...);

void logStore(uint8_t level, uint32_t timestamp, const char *msg);

#if LOG_LEVEL >= LOG_INFO
  #define LOG_I(fmt, ...) logWrite(LOG_INFO, fmt, ##__VA_ARGS__)
#else
  #define LOG_I(fmt, ...) do {} while (0)
#endif

#if LOG_LEVEL >= LOG_WARN
  #define LOG_W(fmt, ...) logWrite(LOG_WARN, fmt, ##__VA_ARGS__)
#else
  #define LOG_W(fmt, ...) do {} while (0)
#endif      

#if LOG_LEVEL >= LOG_ERROR
  #define LOG_E(fmt, ...) logWrite(LOG_ERROR, fmt, ##__VA_ARGS__)
#else               
  #define LOG_E(fmt, ...) do {} while (0)                           
#endif

#if LOG_LEVEL >= LOG_DEBUG
  #define LOG_D(fmt, ...) logWrite(LOG_DEBUG, fmt, ##__VA_ARGS__)
#else
  #define LOG_D(fmt, ...) do {} while (0)
#endif


#define LOG_MAX 20
#define LOG_TEXT_MAX 120

struct LogEntry
{
  uint8_t level;
  uint32_t timestamp;
  char text[LOG_TEXT_MAX + 1];
};

uint8_t logCount();
const LogEntry &logAt(uint8_t i);


const char *logLevelName(uint8_t level);
 
void logSetLevel(uint8_t level); // рантаймовий фільтр
uint8_t logGetLevel();