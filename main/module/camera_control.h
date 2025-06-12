
#include <esp_camera.h>

// ESP32Cam (AiThinker) PIN Map
#define CAM_PIN_PWDN 32
#define CAM_PIN_RESET -1  // software reset will be performed
#define CAM_PIN_XCLK 0
#define CAM_PIN_SIOD 26
#define CAM_PIN_SIOC 27

#define CAM_PIN_D7 35
#define CAM_PIN_D6 34
#define CAM_PIN_D5 39
#define CAM_PIN_D4 36
#define CAM_PIN_D3 21
#define CAM_PIN_D2 19
#define CAM_PIN_D1 18
#define CAM_PIN_D0 5
#define CAM_PIN_VSYNC 25
#define CAM_PIN_HREF 23
#define CAM_PIN_PCLK 22

static const char *TAG_CAM = "main:cam";

#if ESP_CAMERA_SUPPORTED
const camera_config_t camera_config = {
    .pin_pwdn = CAM_PIN_PWDN,
    .pin_reset = CAM_PIN_RESET,
    .pin_xclk = CAM_PIN_XCLK,
    .pin_sccb_sda = CAM_PIN_SIOD,
    .pin_sccb_scl = CAM_PIN_SIOC,

    .pin_d7 = CAM_PIN_D7,
    .pin_d6 = CAM_PIN_D6,
    .pin_d5 = CAM_PIN_D5,
    .pin_d4 = CAM_PIN_D4,
    .pin_d3 = CAM_PIN_D3,
    .pin_d2 = CAM_PIN_D2,
    .pin_d1 = CAM_PIN_D1,
    .pin_d0 = CAM_PIN_D0,
    .pin_vsync = CAM_PIN_VSYNC,
    .pin_href = CAM_PIN_HREF,
    .pin_pclk = CAM_PIN_PCLK,

    .xclk_freq_hz = 24000000,
    .ledc_timer = LEDC_TIMER_0,
    .ledc_channel = LEDC_CHANNEL_0,

    .pixel_format = PIXFORMAT_JPEG,  // YUV422,GRAYSCALE,RGB565,JPEG
    .frame_size = FRAMESIZE_UXGA,

    .jpeg_quality = 1,  // 0-63, for OV series camera sensors, lower number means higher quality
    .fb_count = 2,      // When jpeg mode is used, if fb_count more than one, the driver will work in continuous mode.
    .fb_location = CAMERA_FB_IN_PSRAM,
    .grab_mode = CAMERA_GRAB_LATEST,
};

sensor_t *cameraSensor;

static esp_err_t cameraInit(void) {
    // initialize the camera
    esp_err_t err = esp_camera_init(&camera_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG_CAM, "Camera Init Failed");
        return err;
    }

    cameraSensor = esp_camera_sensor_get();
    if (!cameraSensor)
        return ESP_FAIL;

    delay(100);

    cameraSensor->set_brightness(cameraSensor, 0);      // -2 to 2
    cameraSensor->set_contrast(cameraSensor, 0);        // -2 to 2
    cameraSensor->set_saturation(cameraSensor, 0);      // -2 to 2
    cameraSensor->set_special_effect(cameraSensor, 0);  // 0 to 6 (0 - No Effect, 1 - Negative, 2 - Grayscale, 3 - Red Tint, 4 - Green Tint, 5 - Blue Tint, 6 - Sepia)
    cameraSensor->set_whitebal(cameraSensor, 1);        // 0 = disable , 1 = enable
    cameraSensor->set_awb_gain(cameraSensor, 1);        // 0 = disable , 1 = enable
    cameraSensor->set_wb_mode(cameraSensor, 0);         // 0 to 4 - if awb_gain enabled (0 - Auto, 1 - Sunny, 2 - Cloudy, 3 - Office, 4 - Home)

    cameraSensor->set_exposure_ctrl(cameraSensor, 1);  // 0 = disable , 1 = enable
    cameraSensor->set_aec2(cameraSensor, 1);           // 0 = disable , 1 = enable
    cameraSensor->set_ae_level(cameraSensor, 0);       // -2 to 2
    cameraSensor->set_aec_value(cameraSensor, 600);    // 0 to 1200

    cameraSensor->set_gain_ctrl(cameraSensor, 0);  // 0 = disable , 1 = enable
    cameraSensor->set_agc_gain(cameraSensor, 0);   // 0 to 30

    cameraSensor->set_gainceiling(cameraSensor, (gainceiling_t)0);  // 0 to 6
    cameraSensor->set_bpc(cameraSensor, 0);                         // 0 = disable , 1 = enable
    cameraSensor->set_wpc(cameraSensor, 1);                         // 0 = disable , 1 = enable
    cameraSensor->set_raw_gma(cameraSensor, 1);                     // 0 = disable , 1 = enable
    cameraSensor->set_lenc(cameraSensor, 1);                        // 0 = disable , 1 = enable
    cameraSensor->set_hmirror(cameraSensor, 1);                     // 0 = disable , 1 = enable
    cameraSensor->set_vflip(cameraSensor, 1);                       // 0 = disable , 1 = enable
    cameraSensor->set_dcw(cameraSensor, 1);                         // 0 = disable , 1 = enable
    cameraSensor->set_colorbar(cameraSensor, 0);                    // 0 = disable , 1 = enable

    delay(100);
    return ESP_OK;
}

static void cameraChangeSettings(framesize_t frameSize, int quality) {
    cameraSensor = esp_camera_sensor_get();
    // cameraSensor->set_pixformat(cameraSensor, PIXFORMAT_GRAYSCALE);
    cameraSensor->set_framesize(cameraSensor, frameSize);
    cameraSensor->set_quality(cameraSensor, quality);
    cameraSensor->set_hmirror(cameraSensor, 1);                     // 0 = disable , 1 = enable
    cameraSensor->set_vflip(cameraSensor, 1);                       // 0 = disable , 1 = enable

    // Flush
    camera_fb_t *pic = esp_camera_fb_get();
    if (pic) esp_camera_fb_return(pic);

    delay(300);
    
    pic = esp_camera_fb_get();
    if (pic) esp_camera_fb_return(pic);
}

#endif