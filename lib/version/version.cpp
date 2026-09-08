#include "version.h"
#include "config.h"
#include "log.h"

String versionShort()
{
    return String(BUILD_COMMIT);
}

void versionPrint()
{
    Serial.println();
    LOG_I("firmware : %s", FIRMWARE_NAME);
    LOG_I("build    : %s", BUILD_COMMIT);
    LOG_I("branch   : %s", BUILD_BRANCH);
    LOG_I("date     : %s", BUILD_DATE);
    LOG_I("protocol : %d", 2);
    LOG_I("node     : %d -> %d", NODE_ID, PEER_ID);
    LOG_I("radio    : SF%d BW%.0f %.1fMHz %ddBm",
          LORA_SF, LORA_BW, LORA_FREQ, LORA_POWER);

    // Вільна купа — найкорисніше число тут. Якщо вона повільно
    // тане від запуску до запуску, десь витік пам'яті.
    LOG_I("uptime   : %lu s", millis() / 1000);
    LOG_I("heap     : %u", ESP.getFreeHeap());
    Serial.println();
}
