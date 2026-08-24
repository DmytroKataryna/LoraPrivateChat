#include "text_input.h"
#include "config.h"

struct KeyLetters
{
  uint8_t count;
  const char *l[6];
};

// Останній варіант у кожному циклі — сама цифра.
// Тобто "2" чотири рази дає "г", а п'ятий раз — "2".
static const KeyLetters LAYOUT_UA[10] = {
    {2, {" ", "0"}},
    {5, {".", ",", "!", "?", "1"}},
    {5, {"а", "б", "в", "г", "2"}},
    {5, {"ґ", "д", "е", "є", "3"}},
    {5, {"ж", "з", "и", "і", "4"}},
    {5, {"ї", "й", "к", "л", "5"}},
    {5, {"м", "н", "о", "п", "6"}},
    {5, {"р", "с", "т", "у", "7"}},
    {5, {"ф", "х", "ц", "ч", "8"}},
    {6, {"ш", "щ", "ь", "ю", "я", "9"}}};

static const KeyLetters LAYOUT_LAT[10] = {
    {2, {" ", "0"}},
    {5, {".", ",", "!", "?", "1"}},
    {4, {"a", "b", "c", "2"}},
    {4, {"d", "e", "f", "3"}},
    {4, {"g", "h", "i", "4"}},
    {4, {"j", "k", "l", "5"}},
    {4, {"m", "n", "o", "6"}},
    {5, {"p", "q", "r", "s", "7"}},
    {4, {"t", "u", "v", "8"}},
    {5, {"w", "x", "y", "z", "9"}}};

static String draft = "";
static bool layoutUa = true;

static char lastDigit = 0;
static uint8_t letterIdx = 0;
static uint8_t lastLetterLen = 0;
static uint32_t lastPressAt = 0;

static void multitapReset()
{
  lastDigit = 0;
  letterIdx = 0;
  lastLetterLen = 0;
}

void inputDigit(char digit)
{
  if (digit < '0' || digit > '9')
  {
    return;
  }

  const KeyLetters *layout = layoutUa ? LAYOUT_UA : LAYOUT_LAT;
  uint8_t d = digit - '0';
  uint32_t now = millis();

  bool sameKey = (digit == lastDigit) && (now - lastPressAt < MULTITAP_MS);
  if (sameKey && lastLetterLen > 0)
  {
    // Той самий клавішний цикл: замінюємо щойно вставлену літеру наступною
    draft.remove(draft.length() - lastLetterLen);
    letterIdx = (letterIdx + 1) % layout[d].count;
  }
  else
  {
    letterIdx = 0;
  }

  const char *letter = layout[d].l[letterIdx];
  if (draft.length() + strlen(letter) <= TXT_MAX)
  {
    draft += letter;
    lastLetterLen = strlen(letter);
  }

  lastDigit = digit;
  lastPressAt = now;
}

void inputBackspace()
{
  int n = draft.length();
  if (n == 0)
  {
    return;
  }

  // Кирилична літера в UTF-8 займає два байти, тому відкочуємось
  // через усі продовжувальні байти виду 10xxxxxx
  int i = n - 1;
  while (i > 0 && ((uint8_t)draft[i] & 0xC0) == 0x80)
  {
    i--;
  }
  draft.remove(i);
  multitapReset();
}

void inputClear()
{
  draft = "";
  multitapReset();
}

void inputToggleLayout()
{
  layoutUa = !layoutUa;
  multitapReset();
}

bool inputLayoutIsUa()
{
  return layoutUa;
}

const String &inputDraft()
{
  return draft;
}
