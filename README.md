# RC-VideoWall-ESP32

Line 143 of ".pio/libdeps/esp32dev/RGB Matrix Panel/RGBmatrixpanel.cpp" should be changed to reflect the pins used for the display.

For example, for my setup:

static const uint8_t defaultrgbpins[] = {25, 26, 27, 21, 22, 13};
