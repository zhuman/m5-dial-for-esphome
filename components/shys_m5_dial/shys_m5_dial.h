#pragma once
#include "esphome.h"
#include "esp_log.h"

#include "globals.h"
#include "ha_api.h"
#include "ha_device.h"
#include "ha_device_light.h"
#include "ha_device_climate.h"
#include "ha_device_cover.h"
#include "ha_device_switch.h"
#include "ha_device_fan.h"
#include "ha_device_mediaplayer.h"
#include "ha_device_lock.h"
#include "ha_device_number.h"
#include "ha_device_timer.h"

#include "M5Dial.h"

#define MAX_DEVICE_COUNT 50

namespace esphome
{
  namespace shys_m5_dial
  {
    class ShysM5Dial : public Component, public esphome::api::CustomAPIDevice
    {
    protected:
      int timeToScreenOff = 30000;
      int longPressMs = 1200;
      int rotaryStepWidth = 10;
      uint16_t displayRefeshPause = 16;

      int apiSendDelay = 1000; // Delay after value change (to avoid sending every value while turning the dial)
      int apiSendLock = 3000;  // Wait time between individual API calls

      bool uiTransitions = false;
      int uiTransitionDuration = 300;
      bool uiAnimations = true;
      Theme uiTheme{};

      // -------------------------------

      HaDevice *devices[MAX_DEVICE_COUNT];
      int deviceCount = 0;

      int currentDevice = 0;

      int lastDisplayDevice = -1;
      float lastDisplayValue = -1;
      int lastModeIndex = -1;

      unsigned long lastRotaryEvent = 0;
      unsigned long lastReceiveEvent = 0;
      unsigned long lastDisplayRefresh = 0;

      int lastLoop = 0;

      bool enableRFID = true;
      bool enableEncoder = true;

      M5DialDisplay *m5DialDisplay = new M5DialDisplay();
      M5DialRfid *m5DialRfid = new M5DialRfid();
      M5DialRotary *m5DialRotary = new M5DialRotary();
      M5DialTouch *m5DialTouch = new M5DialTouch();
      M5DialEEPROM *m5DialEEPROM = new M5DialEEPROM();

      esphome::time::RealTimeClock *local_time;

      bool startsWith(const char *pre, const char *str)
      {
        return strncmp(pre, str, strlen(pre)) == 0;
      }

      static uint16_t rgbTo565(uint32_t rgb)
      {
        uint8_t r = (rgb >> 16) & 0xFF;
        uint8_t g = (rgb >> 8) & 0xFF;
        uint8_t b = (rgb) & 0xFF;
        return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
      }

      int getCurrentValue()
      {
        return devices[currentDevice]->getValue();
      }

      bool isDisplayRefreshNeeded()
      {
        bool anim_refresh = devices[currentDevice]->isDisplayRefreshNeeded();
        bool value_changed = (getCurrentValue() != lastDisplayValue);
        bool device_changed = (currentDevice != lastDisplayDevice);
        bool mode_changed = (devices[currentDevice]->getCurrentModeIndex() != lastModeIndex);
        if (anim_refresh || value_changed || device_changed || mode_changed)
        {
          return esphome::millis() - lastDisplayRefresh > displayRefeshPause;
        }
        return false;
      }

      void refreshDisplay(bool forceRefresh)
      {
        bool deviceChanged = lastDisplayDevice != currentDevice;
        bool modeChanged = devices[currentDevice]->getCurrentModeIndex() != lastModeIndex;
        bool shouldRefresh = forceRefresh || isDisplayRefreshNeeded() || deviceChanged || modeChanged;
        // ESP_LOGD("DISPLAY", "refresh? f=%d needs=%d val=%d last=%.1f deviceChanged=%d modeChanged=%d", forceRefresh, devices[currentDevice]->isDisplayRefreshNeeded(), getCurrentValue(), lastDisplayValue, deviceChanged, modeChanged);
        if (!shouldRefresh)
          return;

        M5Dial.Display.startWrite();

        if (uiTransitions && (deviceChanged || modeChanged) && m5DialDisplay->transitionsEnabled() && m5DialDisplay->transitionsSupported())
        {
          if (m5DialDisplay->isTransitionActive())
          {
            m5DialDisplay->tickTransition();
          }
          else
          {
            // Render prev frame
            if (lastDisplayDevice >= 0 && lastDisplayDevice < deviceCount)
            {
              m5DialDisplay->usePrevTarget();
              devices[lastDisplayDevice]->refreshDisplay(*m5DialDisplay, true);
            }
            // Render next frame
            m5DialDisplay->useNextTarget();
            devices[currentDevice]->refreshDisplay(*m5DialDisplay, true);
            m5DialDisplay->useDeviceTarget();
            m5DialDisplay->startTransition(uiTransitionDuration);
          }
        }
        else
        {
          devices[currentDevice]->refreshDisplay(*m5DialDisplay, deviceChanged || modeChanged);

          m5DialDisplay->updateDoubleBufferedScreen();
        }

        M5Dial.Display.endWrite();

        lastDisplayDevice = currentDevice;
        lastModeIndex = devices[currentDevice]->getCurrentModeIndex();
        lastDisplayValue = getCurrentValue();
        lastDisplayRefresh = esphome::millis();
      }

      void nextDevice()
      {
        if (currentDevice >= deviceCount - 1)
        {
          currentDevice = 0;
        }
        else
        {
          currentDevice++;
        }
      }

      void previousDevice()
      {
        if (currentDevice >= 1)
        {
          currentDevice--;
        }
        else
        {
          currentDevice = deviceCount - 1;
        }
      }

      /**
       *
       */
      void addDevice(HaDevice *device)
      {
        if (device != nullptr)
        {
          if (this->deviceCount >= MAX_DEVICE_COUNT - 1)
          {
            ESP_LOGE("DEVICE", "EXCEED DEVICE COUNT MAXIMUM: %s can not be added!", device->getName().c_str());
            return;
          }

          ESP_LOGD("DEVICE", "New Device: %s", device->getName().c_str());

          devices[deviceCount] = device;

          devices[deviceCount]->setApiSendDelay(this->apiSendDelay);
          devices[deviceCount]->setApiSendLock(this->apiSendLock);
          devices[deviceCount]->setRotaryStepWidth(this->rotaryStepWidth);

          devices[deviceCount]->init();
          devices[deviceCount]->setAnimationsEnabled(this->uiAnimations);

          deviceCount++;
          ESP_LOGD("DEVICE", "Device added");
        }
      }

      int getDeviceIdByEntityId(std::string entityId)
      {
        for (int i = 0; i < deviceCount; i++)
        {
          HaDevice *device = devices[i];
          if (strcmp(device->getEntityId().c_str(), entityId.c_str()) == 0)
          {
            return i;
          }
        }
        return -1;
      }

      void setLockDevice(std::string entityId, bool lock)
      {
        int deviceIndex = this->getDeviceIdByEntityId(entityId);

        if (deviceIndex >= 0)
        {
          devices[deviceIndex]->setLocked(lock);
          ESP_LOGI("SERVICE", "Entity %s %s", entityId.c_str(), lock ? "locked" : "unlocked");
          return;
        }

        ESP_LOGW("SERVICE", "Entity-ID %s not found for %s", entityId.c_str(), lock ? "lock" : "unlock");
      }

    public:
      void dump_config() override;
      void setup() override;
      void loop() override;

      ShysM5Dial() : Component() {}

      void setScreenOffTime(int value)
      {
        ESP_LOGI("DEVICE", "setScreenOffTime %i", value);
        this->timeToScreenOff = value;
        m5DialDisplay->setTimeToScreenOff(value);
      }

      void setLongPressDuration(int value)
      {
        ESP_LOGI("DEVICE", "setLongPressDuration %i", value);
        this->longPressMs = value;
        m5DialRotary->setLongPressDuration(value);
      }

      void setApiSendDelay(int delayInMs)
      {
        ESP_LOGI("DEVICE", "setApiSendDelay %i", delayInMs);
        this->apiSendDelay = delayInMs;
      }

      void setApiSendLock(int delayInMs)
      {
        ESP_LOGI("DEVICE", "setApiSendLock %i", delayInMs);
        this->apiSendLock = delayInMs;
      }

      void setRotaryStepWidth(int value)
      {
        ESP_LOGI("DEVICE", "setRotaryStepWidth %i", value);
        this->rotaryStepWidth = value;
      }

      void setFontName(std::string value)
      {
        ESP_LOGI("DEVICE", "setFontName %s", value);
        m5DialDisplay->setFontName(value);
      }

      void setFontFactor(int value)
      {
        ESP_LOGI("DEVICE", "setFontFactor %i", value);
        m5DialDisplay->setFontFactor(value);
      }

      void setDisplayRotation(int value)
      {
        ESP_LOGI("DEVICE", "setDisplayRotation %i", value);
        m5DialDisplay->setRotation(value);
      }

      void setScreensaver(std::string value)
      {
        ESP_LOGI("DEVICE", "setScreensaver %s", value);

        if (strcmp(value.c_str(), "clock") == 0)
        {
          m5DialDisplay->setScreensaver(new ScreensaverClock());
        }
        else
        {
          m5DialDisplay->setScreensaver(nullptr);
        }
      }

      void setUiTransitions(bool enabled)
      {
        this->uiTransitions = enabled;
        if (m5DialDisplay)
          m5DialDisplay->setTransitionsEnabled(enabled);
      }
      void setUiTransitionDuration(int ms)
      {
        this->uiTransitionDuration = ms;
        if (m5DialDisplay)
          m5DialDisplay->setTransitionDuration(ms);
      }
      void setUiAnimations(bool enabled)
      {
        this->uiAnimations = enabled;
      }
      void setUiTheme(uint32_t bg, uint32_t fg, uint32_t accent, uint32_t text)
      {
        this->uiTheme.background = rgbTo565(bg);
        this->uiTheme.foreground = rgbTo565(fg);
        this->uiTheme.accent = rgbTo565(accent);
        this->uiTheme.text = rgbTo565(text);
        if (m5DialDisplay)
          m5DialDisplay->setTheme(this->uiTheme);
      }

      void setTimeComponent(esphome::time::RealTimeClock *clock)
      {
        this->local_time = clock;
      }

      /**
       *
       */
      void addLight(const std::string &entity_id, const std::string &name, const std::string &modes)
      {
        HaDeviceLight *light = new HaDeviceLight(entity_id, name, modes);
        addDevice(light);
      }

      /**
       *
       */
      void addClimate(const std::string &entity_id, const std::string &name, const std::string &modes)
      {
        HaDeviceClimate *climate = new HaDeviceClimate(entity_id, name, modes);
        addDevice(climate);
      }

      /**
       *
       */
      void addCover(const std::string &entity_id, const std::string &name, const std::string &modes)
      {
        HaDeviceCover *climate = new HaDeviceCover(entity_id, name, modes);
        addDevice(climate);
      }

      /**
       *
       */
      void addSwitch(const std::string &entity_id, const std::string &name, const std::string &modes)
      {
        HaDeviceSwitch *switchDevice = new HaDeviceSwitch(entity_id, name, modes);
        addDevice(switchDevice);
      }

      /**
       *
       */
      void addFan(const std::string &entity_id, const std::string &name, const std::string &modes)
      {
        HaDeviceFan *fan = new HaDeviceFan(entity_id, name, modes);
        addDevice(fan);
      }

      /**
       *
       */
      void addMediaPlayer(const std::string &entity_id, const std::string &name, const std::string &modes)
      {
        HaDeviceMediaPlayer *mediaPlayer = new HaDeviceMediaPlayer(entity_id, name, modes);
        addDevice(mediaPlayer);
      }

      /**
       *
       */
      void addLock(const std::string &entity_id, const std::string &name, const std::string &modes)
      {
        HaDeviceLock *lock = new HaDeviceLock(entity_id, name, modes);
        addDevice(lock);
      }

      /**
       *
       */
      void addNumber(const std::string &entity_id, const std::string &name, const std::string &modes)
      {
        HaDeviceNumber *number = new HaDeviceNumber(entity_id, name, modes);
        addDevice(number);
      }

      /**
       *
       */
      void addTimer(const std::string &entity_id, const std::string &name, const std::string &modes)
      {
        HaDeviceTimer *timer = new HaDeviceTimer(entity_id, name, modes);
        timer->setTimeComponent(this->local_time);
        addDevice(timer);
      }

      /**
       *
       */
      void initDevice()
      {
        using std::placeholders::_1;
        using std::placeholders::_2;

        ESP_LOGI("DEVICE", "Initialisierung...");

        auto cfg = M5.config();
        M5Dial.begin(cfg, enableEncoder, enableRFID);

        ESP_LOGI("DEVICE", "Register Callbacks...");
        m5DialRotary->on_rotary_left(std::bind(&esphome::shys_m5_dial::ShysM5Dial::turnRotaryLeft, this));
        m5DialRotary->on_rotary_right(std::bind(&esphome::shys_m5_dial::ShysM5Dial::turnRotaryRight, this));
        m5DialRotary->on_short_button_press(std::bind(&esphome::shys_m5_dial::ShysM5Dial::shortButtonPress, this));
        m5DialRotary->on_long_button_press(std::bind(&esphome::shys_m5_dial::ShysM5Dial::longButtonPress, this));

        m5DialTouch->on_touch(std::bind(&esphome::shys_m5_dial::ShysM5Dial::touchInput, this, _1, _2));
        m5DialTouch->on_swipe(std::bind(&esphome::shys_m5_dial::ShysM5Dial::touchSwipe, this, _1));

        m5DialDisplay->on_display_refresh(std::bind(&esphome::shys_m5_dial::ShysM5Dial::refreshDisplay, this, _1));
        m5DialDisplay->init();
        m5DialDisplay->setTransitionsEnabled(uiTransitions);
        m5DialDisplay->setTransitionDuration(uiTransitionDuration);
        m5DialDisplay->setTheme(uiTheme);

        this->registerServices();
      }

      /**
       *
       */
      void doLoop()
      {
        if (api::global_api_server->is_connected())
        {

          ESP_LOGD("LOOP", "Rotary");
          m5DialRotary->handleRotary();

          ESP_LOGD("LOOP", "Button");
          if (m5DialRotary->handleButtonPress())
          {
            m5DialDisplay->resetLastEventTimer();
          }

          ESP_LOGD("LOOP", "Touch");
          m5DialTouch->handleTouch();
          m5DialDisplay->validateTimeout();

          ESP_LOGD("LOOP", "Update HA Value");
          devices[currentDevice]->updateHomeAssistantValue();

          devices[currentDevice]->doOnLoop();

          ESP_LOGD("LOOP", "Refresh Display");
          this->refreshDisplay(false);

          lastLoop = 1;
        }
        else if (network::is_connected())
        {
          if (lastLoop != 2)
          {
            ESP_LOGD("HA_API", "API is not connected");
            m5DialDisplay->showDisconnected();
          }
          esphome::delay(10);
          lastLoop = 2;
        }
        else
        {
          if (lastLoop != 3)
          {
            ESP_LOGD("wifi", "Network is not connected");
            m5DialDisplay->showOffline();
          }
          esphome::delay(10);
          lastLoop = 3;
        }
      }

      /**
       *
       */
      void registerServices()
      {
        register_service(&ShysM5Dial::selectDevice, "select_device", {"entity_id"});
        register_service(&ShysM5Dial::lockDevice, "lock_device", {"entity_id"});
        register_service(&ShysM5Dial::unlockDevice, "unlock_device", {"entity_id"});
      }

      /**
       *
       */
      void lockDevice(std::string entityId)
      {
        this->setLockDevice(entityId, true);
      }

      /**
       *
       */
      void unlockDevice(std::string entityId)
      {
        this->setLockDevice(entityId, false);
      }

      /**
       *
       */
      void selectDevice(std::string entityId)
      {
        int deviceIndex = this->getDeviceIdByEntityId(entityId);

        if (deviceIndex >= 0)
        {
          this->currentDevice = deviceIndex;
          this->m5DialDisplay->resetLastEventTimer();

          ESP_LOGI("SERVICE", "Entity %s selected", entityId.c_str());
          return;
        }
        ESP_LOGW("SERVICE", "Entity-ID %s not found", entityId.c_str());
      }

      /**
       *
       */
      void turnRotaryLeft()
      {
        m5DialDisplay->resetLastEventTimer();
        M5Dial.Speaker.tone(5000, 20);

        if (m5DialDisplay->isDisplayOn() && !m5DialDisplay->isScreensaverRunning())
        {
          devices[currentDevice]->doOnRotary(*m5DialDisplay, ROTARY_LEFT);
        }

        lastRotaryEvent = esphome::millis();
      }

      /**
       *
       */
      void turnRotaryRight()
      {
        m5DialDisplay->resetLastEventTimer();
        M5Dial.Speaker.tone(5000, 20);

        if (m5DialDisplay->isDisplayOn() && !m5DialDisplay->isScreensaverRunning())
        {
          devices[currentDevice]->doOnRotary(*m5DialDisplay, ROTARY_RIGHT);
        }

        lastRotaryEvent = esphome::millis();
      }

      /**
       *
       */
      void shortButtonPress()
      {
        m5DialDisplay->resetLastEventTimer();
        M5Dial.Speaker.tone(4000, 20);

        if (m5DialDisplay->isDisplayOn() && !m5DialDisplay->isScreensaverRunning())
        {
          devices[currentDevice]->doOnButton(*m5DialDisplay, BUTTON_SHORT);
        }
      }

      /**
       *
       */
      void longButtonPress()
      {
        if (m5DialDisplay->isDisplayOn() && !m5DialDisplay->isScreensaverRunning())
        {
          m5DialDisplay->resetLastEventTimer();
        }
      }

      /**
       *
       */
      void touchInput(uint16_t x, uint16_t y)
      {
        m5DialDisplay->resetLastEventTimer();

        if (m5DialDisplay->isDisplayOn() && !m5DialDisplay->isScreensaverRunning())
        {
          devices[currentDevice]->doOnTouch(*m5DialDisplay, x, y);
        }
      }

      /**
       *
       */
      void touchSwipe(const char *direction)
      {
        ESP_LOGD("TOUCH", "touchSwipe direction: %s", direction);
        m5DialDisplay->resetLastEventTimer();

        if (m5DialDisplay->isDisplayOn() && !m5DialDisplay->isScreensaverRunning())
        {
          if (!devices[currentDevice]->doOnSwipe(*m5DialDisplay, direction))
          {

            if (strcmp(direction, TOUCH_SWIPE_LEFT) == 0)
            {
              this->previousDevice();
            }
            else if (strcmp(direction, TOUCH_SWIPE_RIGHT) == 0)
            {
              this->nextDevice();
            }
            else if (strcmp(direction, TOUCH_SWIPE_UP) == 0)
            {
              devices[currentDevice]->previousMode();
            }
            else if (strcmp(direction, TOUCH_SWIPE_DOWN) == 0)
            {
              devices[currentDevice]->nextMode();
            }
          }
        }
      }
    };
  }
}