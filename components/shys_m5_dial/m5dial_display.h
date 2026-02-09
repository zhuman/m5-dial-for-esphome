#pragma once
#include "M5Dial.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include "default_font_16px.h"
#include "screensaver.h"
#include <M5GFX.h>
#include <vector>

struct Theme {
    uint16_t background = YELLOW;
    uint16_t foreground = WHITE;
    uint16_t accent = ORANGE;
    uint16_t text = BLACK;
    uint16_t muted = DARKGREY;
};

#define FF_DEFAULT &default_font_16px


/**
 * M5Dial Display
 *--------------------------
 * Display driver: GC9A01
 * Resolution: 240x240
 * Touch driver: FT3267
 */
namespace esphome
{
    namespace shys_m5_dial
    {
        class M5DialDisplay {
            protected:
                Theme theme_{};
                uint16_t backgroundColor = YELLOW;

                LovyanGFX* gfx = &M5Dial.Display;
                lgfx::LGFX_Sprite sprite_prev_{&M5Dial.Display};
                lgfx::LGFX_Sprite sprite_next_or_current_{&M5Dial.Display};
                bool transitions_enabled_ = false;
                bool sprites_ready_ = false;
                bool sprites_failed_ = false;
                bool transition_active_ = false;
                bool double_buffering_ = true;
                uint32_t transition_start_ms_ = 0;
                uint32_t transition_duration_ms_ = 300;
                std::vector<uint16_t> blend_line_{};

                enum class Target { DEVICE, PREV, NEXT } target_ = Target::DEVICE;

                int timeToScreenOff = 30000;
                int timeToDimScreen = 20000;
                int dimmedBrightness = 2;
                int normalBrightness = 100;
                unsigned long lastEvent = 0;
                uint16_t lastMode = -1;

                std::string fontName = "default";  //"FreeMono12pt7b";
                float fontFactor = 1;

                int displayRotation = 2;

                Screensaver* screensaver = nullptr;
                bool screensaverRunning = false;

                std::function<void(bool)> display_refresh_action;

            public:
                M5DialDisplay() {
                }

                void init(){
                    M5Dial.Display.setRotation(displayRotation);
                    
                    ensureSprites();
                }

                void on_display_refresh(std::function<void(bool)> callback){
                    ESP_LOGD("DEVICE", "register on_swipe Callback");
                    this->display_refresh_action = callback;
                }

                void setTimeToScreenOff(int value){
                    this->timeToScreenOff = value;
                }

                void setRotation(int value){
                    this->displayRotation = value;
                }

                void resetLastEventTimer(){
                    lastEvent = esphome::millis();
                }

                uint16_t getHeight(){
                    return M5Dial.Display.height();
                }
                uint16_t getWidth(){
                    return M5Dial.Display.width();
                }

                LovyanGFX* getGfx() {
                    switch (target_) {
                        case Target::PREV:
                            return sprites_ready_ ? &sprite_prev_ : gfx;
                        case Target::NEXT:
                            return sprites_ready_ ? &sprite_next_or_current_ : gfx;
                        case Target::DEVICE:
                        default:
                            return (double_buffering_ && sprites_ready_) ? &sprite_next_or_current_ : gfx;
                    }
                }

                void setFontName(std::string name){
                    this->fontName = name;
                }

                void setTheme(const Theme& t){
                    this->theme_ = t;
                    this->backgroundColor = t.background;
                }
                const Theme& theme() const { return this->theme_; }

                void setTransitionsEnabled(bool enabled){ this->transitions_enabled_ = enabled; }
                bool transitionsEnabled() const { return this->transitions_enabled_; }
                void setTransitionDuration(uint32_t ms){ this->transition_duration_ms_ = ms; }
                bool transitionsSupported(){
                    if (sprites_failed_) return false;
                    bool ok = ensureSprites();
                    if (!ok) {
                        ESP_LOGW("DISPLAY", "Transitions disabled: sprite allocation failed");
                        transitions_enabled_ = false;
                        sprites_failed_ = true;
                    }
                    return ok;
                }

                void usePrevTarget(){ this->target_ = Target::PREV; }
                void useNextTarget(){ this->target_ = Target::NEXT; }
                void useDeviceTarget(){ this->target_ = Target::DEVICE; }

            protected:
                bool ensureSprites(){
                    ESP_LOGD("DISPLAY", "ensureSprites called ready=%d failed=%d", sprites_ready_, sprites_failed_);
                    if (sprites_ready_) return true;
                    if (sprites_failed_) return false;

                    if (transitions_enabled_)
                    {
                        sprite_prev_.setColorDepth(16);        
                        if (!sprite_prev_.createSprite(getWidth(), getHeight())) {
                            ESP_LOGW("DISPLAY", "createSprite prev failed %dx%d", getWidth(), getHeight());
                            sprites_failed_ = true;
                            return false;
                        }
                    }
                    if (transitions_enabled_ || double_buffering_)
                    {
                        sprite_next_or_current_.setColorDepth(16);
                        if (!sprite_next_or_current_.createSprite(getWidth(), getHeight())) {
                            ESP_LOGW("DISPLAY", "createSprite next failed %dx%d", getWidth(), getHeight());
                            sprites_failed_ = true;
                            return false;
                        }
                    }
                    blend_line_.resize(getWidth());
                    sprites_ready_ = true;
                    return true;
                }

                static inline uint16_t blend565(uint16_t c0, uint16_t c1, float t){
                    uint32_t r0 = (c0 >> 11) & 0x1F;
                    uint32_t g0 = (c0 >> 5) & 0x3F;
                    uint32_t b0 = (c0) & 0x1F;
                    uint32_t r1 = (c1 >> 11) & 0x1F;
                    uint32_t g1 = (c1 >> 5) & 0x3F;
                    uint32_t b1 = (c1) & 0x1F;
                    uint32_t r = r0 + (int)((r1 - r0) * t);
                    uint32_t g = g0 + (int)((g1 - g0) * t);
                    uint32_t b = b0 + (int)((b1 - b0) * t);
                    return (r << 11) | (g << 5) | (b);
                }

            public:
                bool startTransition(uint32_t duration_ms){
                    if (!transitions_enabled_) return false;
                    if (!ensureSprites()) { transitions_enabled_ = false; return false; }
                    transition_active_ = true;
                    transition_start_ms_ = esphome::millis();
                    transition_duration_ms_ = duration_ms;
                    return true;
                }

                bool tickTransition(){
                    if (!transition_active_ || !sprites_ready_) return false;
                    uint32_t now = esphome::millis();
                    float t = (float)(now - transition_start_ms_) / (float)transition_duration_ms_;
                    if (t >= 1.0f){
                        transition_active_ = false;
                        sprite_next_or_current_.pushSprite(0,0);
                        return false;
                    }
                    int w = sprite_prev_.width();
                    int h = sprite_prev_.height();
                    uint16_t* buf_prev = (uint16_t*)sprite_prev_.getBuffer();
                    uint16_t* buf_next = (uint16_t*)sprite_next_or_current_.getBuffer();
                    for(int y=0; y<h; y++){
                        uint16_t* line_prev = buf_prev + y*w;
                        uint16_t* line_next = buf_next + y*w;
                        for(int x=0; x<w; x++){
                            blend_line_[x] = blend565(line_prev[x], line_next[x], t);
                        }
                        gfx->pushImage(0, y, w, 1, blend_line_.data());
                    }
                    return true;
                }

                bool updateDoubleBufferedScreen(){
                    if (!double_buffering_) return false;
                    if (!sprites_ready_) return false;
                    sprite_next_or_current_.pushSprite(0,0);
                    return true;
                }

                bool isTransitionActive() const { return transition_active_; }

                void setFontFactor(float factor){
                    this->fontFactor = factor;
                }

                bool isDisplayOn(){
                    return M5Dial.Display.getBrightness() > 0;
                }

                void setBackgroundColor(uint16_t color){
                    this->backgroundColor = color;
                    this->theme_.background = color;
                }

                uint16_t getBackgroundColor(){
                    return this->theme_.background;
                }

                void setScreensaver(Screensaver* saver){
                    this->screensaver = saver;
                }

                bool isScreensaverActive(){
                    return this->screensaver != nullptr;
                }
                
                bool isScreensaverRunning(){
                    return screensaverRunning;
                }
                
                void resetScreensaverRunning(){
                    screensaverRunning = false;
                }

                void validateTimeout(){
                    int msSinceLastEvent = esphome::millis() - lastEvent;

                    if (msSinceLastEvent > timeToScreenOff ) {
                        if(this->isScreensaverActive()){
                            bool forceRefresh = !screensaverRunning;
                            screensaver->show(*this, forceRefresh);
                            
                            screensaverRunning = true;
                        } else {
                            if(M5Dial.Display.getBrightness()>0){
                                M5Dial.Display.setBrightness(0);
                                ESP_LOGI("DISPLAY", "Sleep after %d ms", timeToScreenOff);
                            }
                        } 
                    } else {
                        if(screensaverRunning){
                            this->resetScreensaverRunning();
                            this->display_refresh_action(true);
                        }

                        int brightness = timeToDimScreen < timeToDimScreen ? normalBrightness :
                            ((msSinceLastEvent - timeToDimScreen) * (dimmedBrightness - normalBrightness) / (timeToScreenOff - timeToDimScreen) + normalBrightness);

                        if ( M5Dial.Display.getBrightness() != brightness ) {
                            M5Dial.Display.setBrightness(brightness);
                        }
                    }
                }


                void showOffline(){
                    uint16_t height = this->getHeight();
                    uint16_t width  = this->getWidth();

                    gfx->setTextColor(LIGHTGREY);
                    gfx->setTextDatum(middle_center);

                    this->setFontByName(this->fontName);

                    gfx->startWrite();                      // Secure SPI bus
                    this->clear(DARKGREY);

                    this->setFontsize(2);
                    gfx->drawString("OFFLINE",
                                    width / 2,
                                    height / 2);

                    gfx->endWrite();                      // Release SPI bus
                    this->resetScreensaverRunning();
                }

                void showDisconnected(){
                    uint16_t height = this->getHeight();
                    uint16_t width  = this->getWidth();

                    gfx->setTextColor(WHITE);
                    gfx->setTextDatum(middle_center);

                    this->setFontByName(this->fontName);

                    gfx->startWrite();                      // Secure SPI bus
                    this->clear(BLUE);
                    
                    this->setFontsize(1);
                    gfx->drawString("DISCONNECTED",
                                    width / 2,
                                    height / 2);

                    gfx->endWrite();                      // Release SPI bus
                    this->resetScreensaverRunning();
                }

                void showUnknown(){
                    uint16_t height = this->getHeight();
                    uint16_t width  = this->getWidth();

                    gfx->setTextColor(MAROON);
                    gfx->setTextDatum(middle_center);

                    this->setFontByName(this->fontName);

                    gfx->startWrite();                      // Secure SPI bus
                    
                    this->clear(ORANGE);
                    
                    this->setFontsize(2);

                    gfx->drawString("UNKNOWN",
                                    width / 2,
                                    height / 2);

                    gfx->endWrite();                      // Release SPI bus
                    this->resetScreensaverRunning();
                }

                float getDegByCoord(uint16_t x, uint16_t y){
                    float mx = M5Dial.Display.width()/2;
                    float my = M5Dial.Display.height()/2;

                    float angle = atan2(y - my, x - mx) * 180.0 / M_PI;
                    //angle = 360 - fmod((angle + 360.0 - 90), 360.0);
                    angle = fmod((angle + 360.0 - 90), 360.0);
                    return angle;
                }

                float getRadiusFromCoord(float touchX, float touchY) {
                    float dx = touchX - (getWidth() / 2.0f);
                    float dy = touchY - (getHeight() / 2.0f);
                    float radius = sqrt(dx * dx + dy * dy);

                    return radius;
                }

                coord getColorCoord(float radius, float degree){
                    coord result;
                    result.x = radius * sin(degree*M_PI/180) + (gfx->width()/2);
                    result.y = radius * cos(degree*M_PI/180) + (gfx->height()/2);
                    return result;
                }

                void drawColorCircleLine(float degree, float r1, float r2, uint32_t color) {
                    uint16_t step = 1;
                    coord c1 = getColorCoord(r1, degree);
                    coord c2 = getColorCoord(r2, degree-step);
                    coord c3 = getColorCoord(r2, degree+step);

                    getGfx()->fillTriangle(c1.x, c1.y, c2.x, c2.y, c3.x, c3.y, color);

                    c1 = getColorCoord(r1, degree);
                    c2 = getColorCoord(r1, degree-step-step);
                    c3 = getColorCoord(r2, degree-step);
                    getGfx()->fillTriangle(c1.x, c1.y, c2.x, c2.y, c3.x, c3.y, color);
                }

                void setFontsize(float size) {
                    getGfx()->setTextSize(size * this->fontFactor);
                }

                int getRowHeight(float fontSize){
                    return (int)this->fontFactor * fontSize;
                }

                void setFontByName(const std::string& name) {
                    if (FONT_MAP.find(name) != FONT_MAP.end()) {
                        this->setFontName(name);
                    } else {
                        this->setFontName("default");
                        ESP_LOGE("DISPLAY", "Font '%s' not found, using default font: 'default'", name.c_str());
                    }

                    if(strcmp(name.c_str(), "default")==0){
                        getGfx()->setFont(FF_DEFAULT);
                    } else {
                        getGfx()->setFont(FONT_MAP[this->fontName]);
                    }
                }

                void drawBitmap(const uint8_t* bmp, int size, uint8_t x, uint8_t y, uint8_t width, uint8_t height){
                    getGfx()->drawJpg(bmp, size, x, y, width, height, 0, 0);
                }

                void drawBitmapTransparent(const uint16_t* bmp, uint8_t x, uint8_t y, uint8_t width, uint8_t height, uint32_t transparentColor){
                    getGfx()->pushImage(x, y, width, height, bmp, transparentColor);
                }

                void clear(uint16_t bgColor){
                    getGfx()->fillRect(0, 0, getWidth(), getHeight(), bgColor);
                }

                void clear(){
                    this->clear(this->backgroundColor);
                }
        };
    }
}