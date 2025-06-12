#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <esp_log.h>
#include <esp_system.h>
#include <sys/param.h>
#include <nvs_flash.h>
#include <iot_button.h>

#include "millis.h"
#include "log_util.h"
#include "module/camera_control.h"
#include "module/oled_control.h"

#include "ota_update.h"
#include "module/wifi_control.h"
#include "module/http_api_control.h"

#define LED_PIN 4
#define BTN_OK_PIN 13

#define PREVIEW_FRAMESIZE FRAMESIZE_QQVGA
#define PREVIEW_QUALITY 4
#define TARGET_FRAME_DELAY 1000 / 12

#ifndef __has_attribute
#define __has_attribute(x) 0
#endif
#if __has_attribute(fallthrough)
#define FALLTHROUGH __attribute__((fallthrough))
// Note, there could be more branches here, like using `[[gsl::suppress]]` for MSVC
#else
#define FALLTHROUGH
#endif

static const char *TAG = "main";

typedef enum app_state {
    APP_STATE_PREVIEW,
    APP_STATE_CAPTURE,
    APP_STATE_PREVIOUS_RESULT,
    APP_STATE_RESULT,
    APP_STATE_RESULT_SCROLL,
    APP_STATE_RESULT_RENDER,
    APP_STATE_RESULT_CLEAN,
    APP_STATE_OTA,
} AppState;
AppState appState = APP_STATE_PREVIEW;

// Result variable
ImageData imageResult;
int imageResultByteOffsetX;
uint8_t imageResultZoom;
typedef enum result_control {
    RESULT_CONTROL_RIGHT,
    RESULT_CONTROL_LEFT,
    RESULT_CONTROL_RESET,
} ResultControl;
ResultControl imageResultControl;

static esp_err_t nvsFlashInit() {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_RETURN_ON_ERROR(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }
    return ESP_OK;
}

static void capureImage(ImageData *imageOut) {
    cameraChangeSettings(FRAMESIZE_UXGA, 6);
    // cameraChangeSettings(FRAMESIZE_XGA, 3);
    ESP_LOGI(TAG, "Take picture");
    oledClear();
    oledShowString(0, "Capture image");
    delay(500);

    camera_fb_t *pic = esp_camera_fb_get();
    // Reset preview camera setting
    cameraChangeSettings(PREVIEW_FRAMESIZE, PREVIEW_QUALITY);
    if (!pic) {
        ESP_LOGE(TAG, "Failed to read image");
        oledShowString(0, "Failed capture");
        goto FAILED;
    }
    // size_t imageSize = pic->len, imageWidth = pic->width, imageHeight = pic->height;
    // ESP_LOGI(TAG, "Image size: %zux%zu (%zu bytes)", imageWidth, imageHeight, imageSize);

    // Show image
    oledClearLine(0);
    oledUpdateImage(pic->buf, pic->len, true, JPG_SCALE_8X);

    // Send image
    oledShowString(0, "Sending image...");
    // UUID
    char processId[37];
    esp_err_t result = httpSendImageProcess(pic->buf, pic->len, processId, sizeof(processId) - 1);
    esp_camera_fb_return(pic);
    if (result != ESP_OK) {
        oledShowString(0, "Send image fail");
        goto FAILED;
    }
    ESP_LOGI(TAG, "Id: %s", processId);

    // Wait image processing
    oledShowString(1, "Processing...   ");
    result = httpWaitImageProcess(pic->buf, processId);
    if (result != ESP_OK) {
        oledShowString(0, "Processing fail ");
        goto FAILED;
    }

    // Wait problem solving
    oledShowString(2, "Solving...      ");
    result = httpWaitImageProcess(pic->buf, processId);
    if (result != ESP_OK) {
        oledShowString(0, "Solving fail   ");
        goto FAILED;
    }

    // Get final result
    oledShowString(3, "Analyzing...    ");
    result = httpGetProcessResult(pic->buf, processId, imageOut);
    if (result != ESP_OK) {
        oledShowString(0, "Analyzing fail  ");
        goto FAILED;
    }

    // Set render result state
    imageResultByteOffsetX = 0;
    imageResultZoom = 0;
    imageResultControl = 0;
    appState = APP_STATE_RESULT_RENDER;
    return;

FAILED:
    appState = APP_STATE_PREVIEW;
    delay(500);  // For showing error
    return;
}

static inline uint8_t zoomImg2x(uint8_t *imgRowOff, int imgByteWidth, int xByteOff) {
    uint8_t pix0 = imgRowOff[xByteOff];
    uint8_t pix1 = (xByteOff + 1 < imgByteWidth) ? imgRowOff[xByteOff + 1] : 0;
    return ((pix0 & 0b10000000) << 0) | ((pix0 & 0b01000000) << 1) |
           ((pix0 & 0b00100000) << 1) | ((pix0 & 0b00010000) << 2) |
           ((pix0 & 0b00001000) << 2) | ((pix0 & 0b00000100) << 3) |
           ((pix0 & 0b00000010) << 3) | ((pix0 & 0b00000001) << 4) |

           ((pix1 & 0b10000000) >> 4) | ((pix1 & 0b01000000) >> 3) |
           ((pix1 & 0b00100000) >> 3) | ((pix1 & 0b00010000) >> 2) |
           ((pix1 & 0b00001000) >> 2) | ((pix1 & 0b00000100) >> 1) |
           ((pix1 & 0b00000010) >> 1) | ((pix1 & 0b00000001) >> 0);
}

static inline uint8_t zoomImg4x(uint8_t *imgRowOff, int imgByteWidth, int xByteOff) {
    uint8_t pix0 = imgRowOff[xByteOff];
    uint8_t pix1 = (xByteOff + 1 < imgByteWidth) ? imgRowOff[xByteOff + 1] : 0;
    uint8_t pix2 = (xByteOff + 2 < imgByteWidth) ? imgRowOff[xByteOff + 2] : 0;
    uint8_t pix3 = (xByteOff + 3 < imgByteWidth) ? imgRowOff[xByteOff + 3] : 0;
    return ((pix0 & 0b10000000) << 0) | ((pix0 & 0b01000000) << 1) | ((pix0 & 0b00100000) << 2) | ((pix0 & 0b00010000) << 3) |
           ((pix0 & 0b00001000) << 3) | ((pix0 & 0b00000100) << 4) | ((pix0 & 0b00000010) << 5) | ((pix0 & 0b00000001) << 6) |

           ((pix1 & 0b10000000) >> 2) | ((pix1 & 0b01000000) >> 1) | ((pix1 & 0b00100000) << 0) | ((pix1 & 0b00010000) << 1) |
           ((pix1 & 0b00001000) << 1) | ((pix1 & 0b00000100) << 2) | ((pix1 & 0b00000010) << 3) | ((pix1 & 0b00000001) << 4) |

           ((pix2 & 0b10000000) >> 4) | ((pix2 & 0b01000000) >> 3) | ((pix2 & 0b00100000) >> 2) | ((pix2 & 0b00010000) >> 1) |
           ((pix2 & 0b00001000) >> 1) | ((pix2 & 0b00000100) >> 0) | ((pix2 & 0b00000010) << 1) | ((pix2 & 0b00000001) << 2) |

           ((pix3 & 0b10000000) >> 6) | ((pix3 & 0b01000000) >> 5) | ((pix3 & 0b00100000) >> 4) | ((pix3 & 0b00010000) >> 3) |
           ((pix3 & 0b00001000) >> 3) | ((pix3 & 0b00000100) >> 2) | ((pix3 & 0b00000010) >> 1) | ((pix3 & 0b00000001) >> 0);
}

static void renderResultImage(ImageData *image) {
    uint8_t frame[BITMAP_ROW_BYTE_COUNT * 64];
    uint8_t *resultImg = image->output + 4;
    int imgByteWidth = image->width >> 3;

    if (imageResultZoom == 1) {
        // int w = MIN(imgByteWidth, BITMAP_ROW_BYTE_COUNT);
        // for (size_t i = 0; i < 64; i++) {
        //     memcpy(frame + i * BITMAP_ROW_BYTE_COUNT, resultImg + imageResultByteOffsetX + i * imgByteWidth, w);
        // }

        // Limit image scroll offset
        if (imgByteWidth < BITMAP_ROW_BYTE_COUNT * 2 || imageResultByteOffsetX < 0)
            imageResultByteOffsetX = 0;
        else if (imageResultByteOffsetX + BITMAP_ROW_BYTE_COUNT * 2 > imgByteWidth)
            imageResultByteOffsetX = imgByteWidth - BITMAP_ROW_BYTE_COUNT * 2;

        for (size_t i = 0; i < 64; i++) {
            uint8_t *off = frame + i * BITMAP_ROW_BYTE_COUNT;
            uint8_t *imgOff = resultImg + (i * 2 * imgByteWidth) + imageResultByteOffsetX;
            int w = MIN(imgByteWidth, BITMAP_ROW_BYTE_COUNT * 2);
            for (size_t j = 0; j < w; j += 2) {
                off[j >> 1] = zoomImg2x(imgOff, imgByteWidth, j) |
                              zoomImg2x(imgOff + imgByteWidth, imgByteWidth, j);
            }
        }
    } else {
        if (imgByteWidth < BITMAP_ROW_BYTE_COUNT * 4 || imageResultByteOffsetX < 0)
            imageResultByteOffsetX = 0;
        else if (imageResultByteOffsetX + BITMAP_ROW_BYTE_COUNT * 4 > imgByteWidth)
            imageResultByteOffsetX = imgByteWidth - BITMAP_ROW_BYTE_COUNT * 4;

        memset(frame, 0, BITMAP_ROW_BYTE_COUNT * 64);
        for (size_t i = 0; i < 32; i++) {
            uint8_t *off = frame + (16 + i) * BITMAP_ROW_BYTE_COUNT;
            uint8_t *imgOff = resultImg + (i * 4 * imgByteWidth) + imageResultByteOffsetX;
            int w = MIN(imgByteWidth, BITMAP_ROW_BYTE_COUNT * 4);
            for (size_t j = 0; j < w; j += 4) {
                off[j >> 2] = zoomImg4x(imgOff, imgByteWidth, j);
            }
        }
    }
    oledShowBitmap(frame);

    // Mode display
    switch (imageResultControl) {
    case RESULT_CONTROL_RIGHT:
        oledShowString(0, ">");
        break;
    case RESULT_CONTROL_LEFT:
        oledShowString(0, "<");
        break;
    case RESULT_CONTROL_RESET:
        oledShowString(0, "-");
        break;
    }
}

void capureImagePreview() {
    // Capture preview image
    camera_fb_t *pic = esp_camera_fb_get();
    if (!pic) {
        ESP_LOGE(TAG, "Failed to read preview image");
        oledShowString(0, "Fail read image.");
        delay(100);
        return;
    }

    // size_t imageSize = pic->len,imageWidth = pic->width,imageHeight = pic->height;
    // ESP_LOGI(TAG, "Image size: %zux%zu (%zu bytes)", imageWidth, imageHeight, imageSize);

    oledUpdateImage(pic->buf, pic->len, false, JPG_SCALE_2X);
    esp_camera_fb_return(pic);
}

void btnOkClick() {
    if (appState == APP_STATE_PREVIEW) {
        appState = APP_STATE_CAPTURE;
    } else if (appState == APP_STATE_RESULT) {
        if (imageResultControl == RESULT_CONTROL_RESET)
            imageResultControl = 0;
        else
            imageResultControl++;
        appState = APP_STATE_RESULT_RENDER;
    }
}

void btnOkLongPress(void *button_handle, void *usr_data) {
    if (appState != APP_STATE_RESULT && appState != APP_STATE_RESULT_SCROLL)
        return;

    if (usr_data) {
        appState = APP_STATE_RESULT_SCROLL;
    } else
        appState = APP_STATE_RESULT;
}

void btnOkDoubleClick() {
    if (appState == APP_STATE_PREVIEW) {
        appState = APP_STATE_PREVIOUS_RESULT;
    }
    // Result control
    else if (appState == APP_STATE_RESULT) {
        if (imageResultByteOffsetX == 0 && imageResultControl == RESULT_CONTROL_RESET)
            // Clean result
            appState = APP_STATE_RESULT_CLEAN;
        else {
            // Change zoom
            if (++imageResultZoom == 2)
                imageResultZoom = 0;
            appState = APP_STATE_RESULT_RENDER;
        }
    }
}

button_config_t btnOkConfig = {
    .type = BUTTON_TYPE_GPIO,
    .long_press_time = CONFIG_BUTTON_LONG_PRESS_TIME_MS,
    .short_press_time = CONFIG_BUTTON_SHORT_PRESS_TIME_MS,
    .gpio_button_config = {
        .gpio_num = BTN_OK_PIN,
        .active_level = 0,
        .enable_power_save = true,
    },
};

void app_main(void) {
    #if (CONFIG_SPIRAM_SUPPORT && (CONFIG_SPIRAM_USE_CAPS_ALLOC || CONFIG_SPIRAM_USE_MALLOC))
        ESP_LOGI(TAG, "SPIRAM is enabled");
    #endif
    init_oled_panel();
    oledShowString(0, "Booting...");

    oledShowString(1, "Init NVS...");
    if (ESP_OK != nvsFlashInit()) {
        oledShowString(1, "Failed NVS   ");
        return;
    }

    oledShowString(1, "Init WiFi...  ");
    if (ESP_OK != wifi_connect()) {
        oledShowString(1, "Failed WiFi  ");
        return;
    }
    otaUpadateCheckStart();

    oledShowString(1, "GPIO init...");
    gpio_set_direction(LED_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(LED_PIN, 0);

    button_handle_t btnOk = iot_button_create(&btnOkConfig);
    if (!btnOk) {
        oledShowString(1, "Failed button");
        return;
    }
    iot_button_register_cb(btnOk, BUTTON_SINGLE_CLICK, btnOkClick, NULL);
    iot_button_register_cb(btnOk, BUTTON_LONG_PRESS_START, btnOkLongPress, (void *)true);
    iot_button_register_cb(btnOk, BUTTON_LONG_PRESS_UP, btnOkLongPress, (void *)false);
    iot_button_register_cb(btnOk, BUTTON_DOUBLE_CLICK, btnOkDoubleClick, NULL);

    oledShowString(1, "Init camera...");
    if (ESP_OK != cameraInit()) {
        oledShowString(1, "Failed camera");
        delay(1000);
        esp_restart();
        return;
    }
    cameraChangeSettings(PREVIEW_FRAMESIZE, PREVIEW_QUALITY);

    oledClear();
    // ESP_LOGI(TAG_CAM, "Total heap:%zu bytes", heap_caps_get_total_size(MALLOC_CAP_8BIT));
    // ESP_LOGI(TAG_CAM, "Free heap: %zu bytes", heap_caps_get_free_size(MALLOC_CAP_8BIT));

    uint64_t time = millis();
    while (!otaUpdating) {
        // Clean result after close
        if (appState == APP_STATE_RESULT_CLEAN) {
            appState = APP_STATE_PREVIEW;
            free(imageResult.output);
        }
        // App state switch
        switch (appState) {
        case APP_STATE_PREVIEW:
            capureImagePreview();
            break;
        case APP_STATE_CAPTURE:
            capureImage(&imageResult);
            break;
        case APP_STATE_PREVIOUS_RESULT:
            capureImage(&imageResult);
            break;
        case APP_STATE_RESULT_RENDER:
            renderResultImage(&imageResult);
            appState = APP_STATE_RESULT;
            break;
        case APP_STATE_RESULT_SCROLL:
            int scrollSpeed = imageResultZoom == 1 ? 2 : 4;

            switch (imageResultControl) {
            case RESULT_CONTROL_RIGHT:
                imageResultByteOffsetX += scrollSpeed;
                break;
            case RESULT_CONTROL_LEFT:
                imageResultByteOffsetX -= scrollSpeed;
                break;
            case RESULT_CONTROL_RESET:
                imageResultByteOffsetX = 0;
                imageResultControl = 0;
                appState = APP_STATE_RESULT;
                break;
            }
            renderResultImage(&imageResult);
            break;
        default:
            break;
        }

        uint64_t now = millis();
        uint64_t pass = now - time; 
        time = now;
        ESP_LOGD(TAG, "Loop: %llu ms", pass);

        // char cache[6];
        // sprintf(cache, "%llu", pass);
        // ssd1306_display_text(&oled, 10, 0, cache, false);

        if (TARGET_FRAME_DELAY > time)
            delay(TARGET_FRAME_DELAY - time);
    }
    esp_camera_deinit();
}
