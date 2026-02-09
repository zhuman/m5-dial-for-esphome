#include "esphome/core/log.h"
#include "shys_m5_dial.h"

namespace esphome
{
    namespace shys_m5_dial
    {
        static const char *TAG = "shys_m5_dial";

        /**
         * @brief SETUP
         *
         * Initialisierung
         */
        void ShysM5Dial::setup()
        {
            ShysM5Dial::initDevice();
            ESP_LOGI("log", "%s", "M5 is initialized");
        }

        /**
         * @brief LOOP
         *
         * Standard Loop
         */
        void ShysM5Dial::loop()
        {
            M5.delay(1);
            M5Dial.update();
            esphome::delay(1);
            ShysM5Dial::doLoop();
            esphome::delay(1);
        }

        /**
         * @brief dump_config
         *
         * Output of the current configuration in the log after initialization.
         */
        void ShysM5Dial::dump_config()
        {
            ESP_LOGCONFIG(TAG, "-----------------------------------");
            ESP_LOGCONFIG(TAG, "Shys M5 Dial (build flicker-fix#3)");
            ESP_LOGCONFIG(TAG, "-----------------------------------");
            ESP_LOGCONFIG(TAG, "ui_transitions: %s", this->uiTransitions ? "true" : "false");
            ESP_LOGCONFIG(TAG, "ui_transition_duration_ms: %d", this->uiTransitionDuration);
            ESP_LOGCONFIG(TAG, "ui_animations: %s", this->uiAnimations ? "true" : "false");
            ESP_LOGCONFIG(TAG, "theme bg=%u fg=%u accent=%u text=%u", this->uiTheme.background, this->uiTheme.foreground, this->uiTheme.accent, this->uiTheme.text);
        }

    }
}