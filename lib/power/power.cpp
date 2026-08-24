#include "power.h"
#include "config.h"
#include "ui.h"
#include "link.h"

#include <Preferences.h>
#include <esp_sleep.h>
#include <esp_wifi.h>
#include <esp_bt.h>
#include <driver/gpio.h>
#include <driver/rtc_io.h>

static const gpio_num_t ROW_PINS[4] = {
    (gpio_num_t)KEY_ROW0, (gpio_num_t)KEY_ROW1,
    (gpio_num_t)KEY_ROW2, (gpio_num_t)KEY_ROW3};

// ext0 працює лише з одним піном, тому будити може тільки
// одна лінія стовпців. GPIO 15 — єдиний із чотирьох,
// придатний для цього: 34/36/39 теж RTC, але ext1 на ESP32
// вміє лише ALL_LOW і ANY_HIGH, що з підтяжками вгору не годиться.
static const gpio_num_t WAKE_PIN = (gpio_num_t)KEY_COL0;

static Preferences prefs;
static bool standby = false;
static bool deepEnabled = false;
static uint32_t lastActivity = 0;

void powerBegin()
{
  // Після пробудження піни лишаються замкненими в стані,
  // який ми задали перед сном. Без цього клавіатура не працюватиме.
  gpio_deep_sleep_hold_dis();
  for (uint8_t i = 0; i < 4; i++)
  {
    gpio_hold_dis(ROW_PINS[i]);
  }
  rtc_gpio_deinit(WAKE_PIN);

  // Wi-Fi і Bluetooth нам не потрібні жодного разу.
  // Помилки тут очікувані — модулі й не ініціалізувались.
  esp_wifi_stop();
  esp_wifi_deinit();
  esp_bt_controller_disable();

  prefs.begin("power", false);
  deepEnabled = prefs.getBool("deep", false);

  if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT0)
  {
    Serial.println("wake: keypad");
  }

  standby = false;
  lastActivity = millis();
}

static void enterStandby()
{
  standby = true;
  uiSleep();

  // Радіо навмисно НЕ чіпаємо — воно продовжує слухати ефір
  setCpuFrequencyMhz(STANDBY_CPU_MHZ);

  Serial.println("standby: screen off, radio listening");
}

static void leaveStandby()
{
  setCpuFrequencyMhz(240);
  standby = false;
  uiWake();
  lastActivity = millis();
}

void powerNoteActivity()
{
  if (standby)
  {
    leaveStandby();
  }
  lastActivity = millis();
}

void powerWake()
{
  if (standby)
  {
    leaveStandby();
  }
  lastActivity = millis();
}

bool powerIsStandby() { return standby; }
bool powerDeepSleepEnabled() { return deepEnabled; }

void powerToggleDeepSleep()
{
  deepEnabled = !deepEnabled;
  prefs.putBool("deep", deepEnabled);
}

void powerSleepNow()
{
  Serial.println("deep sleep");
  Serial.flush();

  // Обидва споживачі треба вимкнути явно, інакше сон
  // майже нічого не заощадить: OLED бере десятки міліампер,
  // радіо в режимі прийому близько дванадцяти.
  uiSleep();
  linkSleep();

  // Тримаємо всі рядки притиснутими до нуля, щоб натиск
  // будь-якої клавіші лінії WAKE_PIN замкнув її на землю
  for (uint8_t i = 0; i < 4; i++)
  {
    pinMode(ROW_PINS[i], OUTPUT);
    digitalWrite(ROW_PINS[i], LOW);
    gpio_hold_en(ROW_PINS[i]);
  }
  gpio_deep_sleep_hold_en();

  rtc_gpio_init(WAKE_PIN);
  rtc_gpio_set_direction(WAKE_PIN, RTC_GPIO_MODE_INPUT_ONLY);
  rtc_gpio_pullup_en(WAKE_PIN);
  rtc_gpio_pulldown_dis(WAKE_PIN);

  esp_sleep_enable_ext0_wakeup(WAKE_PIN, 0); // 0 = прокидатись на LOW
  esp_deep_sleep_start();
}

void powerTick()
{
#if ENABLE_SLEEP && HAS_KEYPAD
  uint32_t idle = millis() - lastActivity;

  if (!standby && idle > SCREEN_TIMEOUT_MS)
  {
    enterStandby();
    return;
  }

  // Другий крок тільки якщо дозволено в меню:
  // після нього пристрій перестає чути ефір
  if (standby && deepEnabled && idle > DEEP_SLEEP_TIMEOUT_MS)
  {
    powerSleepNow();
  }
#endif
}
