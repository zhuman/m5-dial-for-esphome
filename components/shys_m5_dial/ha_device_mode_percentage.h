#pragma once
#include "text_marquee.h"

namespace esphome
{
    namespace shys_m5_dial
    {
        class HaDeviceModePercentage: public esphome::shys_m5_dial::HaDeviceMode {
            protected:
                std::string label = "Percentage";
                std::string unit = "%";
                unsigned short* icon = nullptr;

                bool barActive = true;
                bool use_custom_value = false;
                std::string custom_value = "";

                TextMarquee device_name_marquee_{};

                bool animations_enabled_ = true;
                float display_value_ = 0.0f;
                bool display_value_init_ = false;
                bool anim_active_ = false;
                uint32_t anim_start_ms_ = 0;
                uint32_t anim_duration_ms_ = 200;
                float anim_from_ = 0.0f;
                float anim_to_ = 0.0f;


                void showPercentageMenu(M5DialDisplay& display){
                    LovyanGFX* gfx = display.getGfx();

                    uint16_t height = gfx->height();
                    uint16_t width  = gfx->width();

                    const Theme& theme = display.theme();
                    uint16_t bg = theme.background;
                    uint16_t accent = theme.accent;
                    uint16_t accent2 = theme.foreground;
                    uint16_t textColor = theme.text;

                    float disp_val = getAnimatedValue();
                    float valOnArc = (getMaxValue()==0?240:((float)240 / (this->getMaxValue() - this->getMinValue())) * (disp_val - this->getMinValue())) + 150;

                    gfx->setTextColor(textColor);
                    gfx->setTextDatum(middle_center);

                    gfx->startWrite();                      // Secure SPI bus

                    display.clear(bg);

                    if(this->isBarActive()){
                        // Round %-Bar
                        gfx->fillArc(width / 2,
                                    height / 2,
                                    115,
                                    100,
                                    150,
                                    valOnArc,
                                    accent
                                    );

                        gfx->fillArc(width / 2,
                                    height / 2,
                                    115,
                                    100,
                                    valOnArc,
                                    390,
                                    accent2
                                    );
                    } else {
                        gfx->fillArc(width / 2,
                                    height / 2,
                                    115,
                                    100,
                                    150,
                                    390,
                                    bg
                                    );
                    }

                    // Percent
                    display.setFontsize(1.7);
                    gfx->drawString(use_custom_value ? custom_value.c_str() : (String((int)round(disp_val)) + this->unit.c_str()).c_str(),
                                    width / 2,
                                    height / 2 - 70);

                    // Mode
                    display.setFontsize(1);
                    gfx->drawString(this->label.c_str(),
                                    width / 2,
                                    height / 2 - 40);  

                    // Icon
                    if(this->icon != nullptr){
                        display.drawBitmapTransparent(this->icon, width/2-35, height/2-30, 70, 70, 0xFFFF);
                    }

                    // Device Name (marquee)
                    display.setFontsize(1);
                    device_name_marquee_.setViewport(width / 2);
                    device_name_marquee_.setText(this->device.getName());
                    bool changed = device_name_marquee_.update(gfx, esphome::millis());
                    device_name_marquee_.draw(gfx, width / 2, height / 2 + 90, textColor);
 

                    gfx->endWrite();                      // Release SPI bus

                    // If marquee is active, keep refreshing
                    this->displayRefreshNeeded = changed && device_name_marquee_.isActive();
                }

                float getAnimatedValue(){
                    if (!display_value_init_){
                        display_value_ = this->getValue();
                        display_value_init_ = true;
                        return display_value_;
                    }

                    if (animations_enabled_ && getValue() != anim_to_)
                    {
                        animateTo(this->getValue());
                    }

                    if (!anim_active_)
                    {
                        display_value_ = this->getValue();
                        return display_value_;
                    }
                    uint32_t now = esphome::millis();
                    float t = (float)(now - anim_start_ms_) / (float)anim_duration_ms_;
                    if (t >= 1.0f){
                        anim_active_ = false;
                        display_value_ = anim_to_;
                        return display_value_;
                    }
                    // safety hard stop
                    if (now - anim_start_ms_ > (anim_duration_ms_ * 2)){
                        anim_active_ = false;
                        display_value_ = anim_to_;
                        return display_value_;
                    }
                    float eased = 1.0f - (1.0f - t) * (1.0f - t);
                    display_value_ = anim_from_ + (anim_to_ - anim_from_) * eased;
                    return display_value_;
                }

                void animateTo(float target){
                    float from = display_value_init_ ? display_value_ : (float)getValue();
                    anim_from_ = from;
                    anim_to_ = target;
                    anim_start_ms_ = esphome::millis();
                    anim_active_ = (round(from) != round(target));
                    display_value_init_ = true;
                }

            public:
                HaDeviceModePercentage(HaDevice& device) : HaDeviceMode(device){}

                ~HaDeviceModePercentage() {
                    delete[] icon;
                }

                void setLabel(const std::string& newLabel){
                    this->label = newLabel;
                }

                void setCustomValue(const std::string& newVal){
                    this->custom_value = newVal;
                }

                void useCustomValue(bool activate){
                    this->use_custom_value = activate;
                }

                void setUnit(const std::string& newUnit){
                    this->unit = newUnit;
                }

                void setIcon(const unsigned short* newIcon, size_t size) {
                    delete[] this->icon;
                    this->icon = new unsigned short[size];
                    std::copy(newIcon, newIcon + size, icon);
                }

                void setAnimationsEnabled(bool enabled) override {
                    animations_enabled_ = enabled;
                    if (!enabled) {
                        anim_active_ = false;
                        display_value_ = this->getValue();
                        display_value_init_ = true;
                        this->displayRefreshNeeded = true;
                    }
                }

                bool isDisplayRefreshNeeded() override {
                    return (animations_enabled_ && anim_active_) || this->displayRefreshNeeded || device_name_marquee_.isActive();
                }

                void activateBar(bool activate){
                    this->barActive = activate;
                }

                bool isBarActive(){
                    return this->barActive;
                }

                void refreshDisplay(M5DialDisplay& display, bool init) override {
                    //ESP_LOGD("DISPLAY", "refresh display: percentage mode val=%d disp=%.2f anim=%d from=%.2f to=%.2f", this->getValue(), display_value_, anim_active_, anim_from_, anim_to_);
                    showPercentageMenu(display);
                    this->displayRefreshNeeded = false;
                }
                
                bool onTouch(M5DialDisplay& display, uint16_t x, uint16_t y) override {
                    return this->defaultOnTouch(display, x, y);        
                }

                bool onRotary(M5DialDisplay& display, const char * direction) override {
                    return this->defaultOnRotary(display, direction);
                }
        };
    }
}