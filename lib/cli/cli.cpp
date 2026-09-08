#include "cli.h"
#include "version.h"
#include "log.h"

#include <ctype.h>
#include <string.h>

#define CLI_BUF_MAX 32

static char buf[CLI_BUF_MAX];
static uint8_t len = 0;

// ---------------------------------------------------------------------------
// Обробники команд
// ---------------------------------------------------------------------------
static void cmdHelp()
{
    Serial.println();
    Serial.println("version      дані про збірку");
    Serial.println("log          останні записи логу");
    Serial.println("log 0..3     рівень: 0=error 1=warn 2=info 3=debug");
    Serial.println("stats        лічильники радіо");
    Serial.println("help         цей список");
    Serial.println();
}

static void cmdVersion()
{
    versionPrint();
}

static void cmdLogDump()
{
    uint8_t n = logCount();
    if (n == 0)
    {
        Serial.println("log empty");
        return;
    }

    Serial.println();
    for (uint8_t i = 0; i < n; i++)
    {
        // logAt(0) — найстаріший, тому виводиться хронологічно
        const LogEntry &e = logAt(i);
        Serial.printf("[%8lu] [%-5s] %s\n",
                      e.timestamp, logLevelName(e.level), e.text);
    }
    Serial.printf("-- %d records --\n\n", n);
}

static void cmdLog(const char *arg)
{
    if (arg == nullptr || arg[0] == '\0')
    {
        cmdLogDump();
        return;
    }

    if (arg[0] < '0' || arg[0] > '3' || arg[1] != '\0')
    {
        Serial.println("usage: log [0..3]");
        return;
    }

    uint8_t want = arg[0] - '0';

    // Підняти рівень вище за зібраний неможливо: макроси вирізали
    // ті виклики ще при компіляції, у прошивці їх просто немає.
    if (want > LOG_LEVEL)
    {
        Serial.printf("built with level %d, cannot raise to %d\n",
                      (int)LOG_LEVEL, want);
        return;
    }

    logSetLevel(want);
    Serial.printf("log level = %d (%s)\n", want, logLevelName(want));
}

static void cmdStats()
{
    Serial.println();
    Serial.printf("uptime : %lu s\n", millis() / 1000);
    Serial.printf("heap   : %u\n", ESP.getFreeHeap());
    Serial.printf("log    : %d/%d\n", logCount(), LOG_MAX);
    Serial.println();
}

// ---------------------------------------------------------------------------
// Розбір рядка
// ---------------------------------------------------------------------------
static void execute(char *line)
{
    // Порожній рядок ігноруємо: при \r\n інакше кожна команда
    // виконувалася б двічі
    if (line[0] == '\0')
    {
        return;
    }

    // Хтось набере VERSION — приводимо до нижнього регістру
    for (char *p = line; *p; p++)
    {
        *p = (char)tolower((unsigned char)*p);
    }

    // Ділимо на команду й аргумент по першому пробілу.
    // Копіювання не потрібне: ставимо термінатор на місце пробілу,
    // і один буфер стає двома рядками.
    char *arg = strchr(line, ' ');
    if (arg)
    {
        *arg = '\0';
        arg++;
    }

    if (strcmp(line, "version") == 0)
    {
        cmdVersion();
    }
    else if (strcmp(line, "log") == 0)
    {
        cmdLog(arg);
    }
    else if (strcmp(line, "stats") == 0)
    {
        cmdStats();
    }
    else if (strcmp(line, "help") == 0 || strcmp(line, "?") == 0)
    {
        cmdHelp();
    }
    else
    {
        // Мовчати не можна: користувач вирішить, що пристрій завис
        Serial.printf("unknown command '%s', try 'help'\n", line);
    }
}

// ---------------------------------------------------------------------------
// Публічне
// ---------------------------------------------------------------------------
void cliBegin()
{
    len = 0;
    Serial.println("type 'help' for commands");
}

void cliTick()
{
    // Читаємо по одному символу за прохід і одразу повертаємо
    // керування. Serial.readStringUntil() блокував би на секунду,
    // і за цей час приймач пропустив би пакети.
    while (Serial.available())
    {
        char c = Serial.read();

        // Різні термінали шлють \n, \r або обидва — вважаємо
        // кінцем рядка будь-який
        if (c == '\n' || c == '\r')
        {
            buf[len] = '\0'; // strcmp шукає термінатор
            execute(buf);
            len = 0;
            continue;
        }

        if (len < CLI_BUF_MAX - 1)
        {
            buf[len++] = c;
        }
        // Символи понад розмір буфера відкидаємо мовчки:
        // краще обрізана команда, ніж затерта пам'ять
    }
}