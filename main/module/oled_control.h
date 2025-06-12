#ifndef __OLED_CONTROL__
#define __OLED_CONTROL__

#include <math.h>

#include <ssd1306.h>

#include "millis.h"
#include "image_lib.h"

#define OLED_WIDTH 128
#define OLED_HEIGHT 64
#define BITMAP_ROW_BYTE_COUNT (OLED_WIDTH >> 3)

#define setIfLess(target, cache, value) \
    if ((cache = value) < target) target = cache

static const char *TAG_OLED = "main:oled";

uint8_t *oledBitmap;
SSD1306_t oled;
static void init_oled_panel(void) {
#if CONFIG_I2C_INTERFACE
    // ESP_LOGI(TAG_OLED, "INTERFACE i2c");
    // ESP_LOGI(TAG_OLED, "CONFIG_SDA_GPIO=%d", CONFIG_SDA_GPIO);
    // ESP_LOGI(TAG_OLED, "CONFIG_SCL_GPIO=%d", CONFIG_SCL_GPIO);
    // ESP_LOGI(TAG_OLED, "CONFIG_RESET_GPIO=%d", CONFIG_RESET_GPIO);
    i2c_master_init(&oled, CONFIG_SDA_GPIO, CONFIG_SCL_GPIO, CONFIG_RESET_GPIO);
#endif  // CONFIG_I2C_INTERFACE

#if CONFIG_SPI_INTERFACE
    // ESP_LOGI(TAG_OLED, "INTERFACE SPI");
    // ESP_LOGI(TAG_OLED, "CONFIG_MOSI_GPIO=%d", CONFIG_MOSI_GPIO);
    // ESP_LOGI(TAG_OLED, "CONFIG_SCLK_GPIO=%d", CONFIG_SCLK_GPIO);
    // ESP_LOGI(TAG_OLED, "CONFIG_CS_GPIO=%d", CONFIG_CS_GPIO);
    // ESP_LOGI(TAG_OLED, "CONFIG_DC_GPIO=%d", CONFIG_DC_GPIO);
    // ESP_LOGI(TAG_OLED, "CONFIG_RESET_GPIO=%d", CONFIG_RESET_GPIO);
    spi_master_init(&dev, CONFIG_MOSI_GPIO, CONFIG_SCLK_GPIO, CONFIG_CS_GPIO, CONFIG_DC_GPIO, CONFIG_RESET_GPIO);
#endif  // CONFIG_SPI_INTERFACE

    // ESP_LOGI(TAG_OLED, "Panel size: 128x64");
    ssd1306_init(&oled, 128, 64);

    ssd1306_contrast(&oled, 0xE0);
    ssd1306_clear_screen(&oled, false);

    oledBitmap = malloc((OLED_WIDTH / 8) * OLED_HEIGHT);
}

static inline uint8_t RGB24_to_BW8(ImageData *bmp, uint16_t x, uint16_t y) {
    if (y >= bmp->height) y = bmp->height - 1;
    uint32_t index = (y * bmp->width + x) * 3;
    return (uint8_t)(0.299f * bmp->output[index] +
                     0.587f * bmp->output[index + 1] +
                     0.114f * bmp->output[index + 2]);
}

void oledUpdateImage(uint8_t *data, size_t len, bool forceCalculateLight, const jpg_scale_t scale) {
    ImageData imageData;
    if (!jpg2bw(data, len, scale, &imageData))
        return;

    static int threshold, step;
    static uint64_t time;

    // Recalculate light
    uint64_t now = millis();
    if (now - time > 300 || forceCalculateLight) {
        time = now;

#define W_SCAN 26
#define H_SCAN 20
        float wScanScale = (float)imageData.width / W_SCAN;
        float hScanScale = (float)imageData.height / H_SCAN;

        float avg = 0;
        int thresholdMin = 255;
        int thresholdMax = 0;
        for (int i = 0; i < H_SCAN; i++) {
            for (int j = 0; j < W_SCAN; j++) {
                uint8_t level = RGB24_to_BW8(&imageData, j * wScanScale, i * hScanScale);
                if (level < thresholdMin) thresholdMin = level;
                if (level > thresholdMax) thresholdMax = level;
                avg += level;
            }
        }
        int center = thresholdMax * 0.4f +
                     thresholdMin * 0.3f +
                     avg / (H_SCAN * W_SCAN) * 0.3f;
        int gap = (thresholdMax - thresholdMin) * 0.15f;
        threshold = center - gap;
        step = center + gap;
        ESP_LOGD(TAG_OLED, "%d, %d", threshold, step);
    }

    float widthScale = (float)imageData.width / OLED_WIDTH;
    float heightScale = (float)imageData.height / OLED_HEIGHT;
    for (uint16_t i = 0; i < OLED_HEIGHT; i++) {
        uint8_t *row = oledBitmap + i * BITMAP_ROW_BYTE_COUNT;
        uint16_t yOff = i * heightScale;
        for (uint16_t j = 0; j < BITMAP_ROW_BYTE_COUNT; j++) {
            uint16_t jb = (j << 3);

            uint8_t cache;
            uint8_t p0 = RGB24_to_BW8(&imageData, (jb + 0) * widthScale, yOff);
            setIfLess(p0, cache, RGB24_to_BW8(&imageData, (jb + 1) * widthScale, yOff));
            setIfLess(p0, cache, RGB24_to_BW8(&imageData, (jb + 0) * widthScale, yOff + 1));
            setIfLess(p0, cache, RGB24_to_BW8(&imageData, (jb + 1) * widthScale, yOff + 1));

            uint8_t p1 = RGB24_to_BW8(&imageData, (jb + 2) * widthScale, yOff);
            setIfLess(p1, cache, RGB24_to_BW8(&imageData, (jb + 3) * widthScale, yOff));
            setIfLess(p1, cache, RGB24_to_BW8(&imageData, (jb + 2) * widthScale, yOff + 1));
            setIfLess(p1, cache, RGB24_to_BW8(&imageData, (jb + 3) * widthScale, yOff + 1));

            uint8_t p2 = RGB24_to_BW8(&imageData, (jb + 4) * widthScale, yOff);
            setIfLess(p2, cache, RGB24_to_BW8(&imageData, (jb + 5) * widthScale, yOff));
            setIfLess(p2, cache, RGB24_to_BW8(&imageData, (jb + 4) * widthScale, yOff + 1));
            setIfLess(p2, cache, RGB24_to_BW8(&imageData, (jb + 5) * widthScale, yOff + 1));

            uint8_t p3 = RGB24_to_BW8(&imageData, (jb + 6) * widthScale, yOff);
            setIfLess(p3, cache, RGB24_to_BW8(&imageData, (jb + 7) * widthScale, yOff));
            setIfLess(p3, cache, RGB24_to_BW8(&imageData, (jb + 6) * widthScale, yOff + 1));
            setIfLess(p3, cache, RGB24_to_BW8(&imageData, (jb + 7) * widthScale, yOff + 1));

            row[j] = (p0 < threshold ? 0 : (p0 < step ? (i % 2 ? 0b1000000 : 0b10000000) : 0b11000000)) |
                     (p1 < threshold ? 0 : (p1 < step ? (i % 2 ? 0b10000 : 0b100000) : 0b110000)) |
                     (p2 < threshold ? 0 : (p2 < step ? (i % 2 ? 0b100 : 0b1000) : 0b1100)) |
                     (p3 < threshold ? 0 : (p3 < step ? (i % 2 ? 0b1 : 0b10) : 0b11));
        }
    }

    freeImageData(&imageData);

    // fmt2bmp
    ssd1306_bitmaps(&oled, 0, 0, oledBitmap, 128, 64, false);
    return;
}

static inline void oledShowBitmap(uint8_t *bitmap) {
    ssd1306_bitmaps(&oled, 0, 0, bitmap, 128, 64, false);
}

static inline void oledShowString(int line, char *str) {
    ssd1306_display_text(&oled, 0, line, str, false);
}

static inline void oledShowStringOffset(int offset, int line, char *str) {
    ssd1306_display_text(&oled, offset, line, str, false);
}

static inline void oledClearLine(int line) {
    ssd1306_clear_line(&oled, line, false);
}

static inline void oledDrawLine(int line, int length) {
    int y = (line << 3) + 4;
    _ssd1306_line(&oled, 0, y, length, y, false);
    ssd1306_show_buffer_page(&oled, line, 0);
}

static inline void oledClear() {
    ssd1306_clear_screen(&oled, false);
}

#endif