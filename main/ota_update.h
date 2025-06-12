#include <stdbool.h>
#include <esp_log.h>
#include <pthread.h>
#include <freertos/FreeRTOS.h>
#include <esp_ota_ops.h>

#include "module/http_api_control.h"
#include "module/oled_control.h"

static const char *TAG_OTA = "main:ota";

bool otaUpdating = false;
pthread_t otaUpdateCheckThreadt;

esp_err_t otaHttpEventHandler(esp_http_client_event_t *evt) {
    esp_err_t err = httpEventHandler(evt);
    if (err != ESP_OK) return err;

    // Read progress
    static float lastProgress = -1;
    char text[17];
    switch (evt->event_id) {
    case HTTP_EVENT_ON_DATA:
        size_t contentLen = esp_http_client_get_content_length(evt->client);
        if (contentLen) {
            if (!otaUpdating) {
                otaUpdating = true;
                lastProgress = -1;
                delay(100);
                oledClear();
            }
            HttpResponseData *data;
            esp_http_client_get_user_data(evt->client, (void **)&data);
            float percent = (float)data->len / contentLen * 100;
            if (percent > lastProgress || data->len == contentLen) {
                int n = (int)(percent * 0.8f + 0.5f);
                oledDrawLine(0, n);
                sprintf(text, "%.1f%%", percent);
                oledShowStringOffset(10, 0, text);
                lastProgress = percent + 0.5f;
            }
        }
    default:
        break;
    }
    return ESP_OK;
}

static esp_err_t checkOtaUpdate() {
    // Check ota update
    esp_http_client_config_t config = {
        .host = HTTP_API_HOST,
        .path = "/ota",
        .port = HTTP_API_PORT,
        .method = HTTP_METHOD_GET,
        .event_handler = otaHttpEventHandler,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_err_t result = esp_http_client_perform(client);
    int code = esp_http_client_get_status_code(client);
    // Get user data
    HttpResponseData *data;
    esp_http_client_get_user_data(client, (void **)&data);
    esp_http_client_cleanup(client);
    // Check state
    if (result != ESP_OK || (code != 200 && code != 204)) {
        ESP_LOGE(TAG_OTA, "OTA check request failed");
        goto UPDATE_FAILED;
    }
    if (code == 204) {
        ESP_LOGD(TAG_OTA, "Device is update to date");
        return ESP_OK;
    }

    oledShowString(1, "Updating...");
    ESP_LOGI(TAG_OTA, "Starting OTA Update...");
    /* update handle : set by esp_ota_begin(), must be freed via esp_ota_end() */
    esp_ota_handle_t update_handle = 0;

    const esp_partition_t *configured = esp_ota_get_boot_partition();
    const esp_partition_t *running = esp_ota_get_running_partition();
    if (configured != running)
        goto UPDATE_FAILED;
    ESP_LOGI(TAG_OTA, "Running partition type %d subtype %d (offset 0x%08lx)", configured->type, configured->subtype, configured->address);

    // Get next partition to write
    const esp_partition_t *update_partition = esp_ota_get_next_update_partition(NULL);
    ESP_LOGI(TAG_OTA, "Writing to partition subtype %d at offset 0x%lx", update_partition->subtype, update_partition->address);
    if (!update_partition) {
        ESP_LOGE(TAG_OTA, "update_partition is NULL");
        goto UPDATE_FAILED;
    }

    esp_err_t err = esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &update_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG_OTA, "esp_ota_begin failed, error=%d", err);
        goto UPDATE_FAILED;
    }
    ESP_LOGI(TAG_OTA, "esp_ota_begin success");

    oledShowString(1, "Write OTA...");
    // Write ota update data
    err = esp_ota_write(update_handle, (const void *)data->buff, data->len);
    if (err != ESP_OK) {
        ESP_LOGE(TAG_OTA, "Error: esp_ota_write failed! err=0x%x", err);
        goto UPDATE_FAILED;
    }
    ESP_LOGI(TAG_OTA, "Total Write binary data length : %d", data->len);

    oledShowString(1, "Write OTA end...");
    err = esp_ota_end(update_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG_OTA, "esp_ota_end failed!");
        goto UPDATE_FAILED;
    }

    oledShowString(1, "Set boot part...");
    // Set reboot partition
    err = esp_ota_set_boot_partition(update_partition);
    if (err != ESP_OK) {
        ESP_LOGE(TAG_OTA, "esp_ota_set_boot_partition failed! err=0x%x", err);
        goto UPDATE_FAILED;
    }
    oledShowString(1, "Restarting...   ");
    ESP_LOGI(TAG_OTA, "Restarting system");
    esp_restart();
    return ESP_OK;

UPDATE_FAILED:
    httpResponseDataFree(data);
    return ESP_FAIL;
}

void *otaUpadateCheckThread(void *args) {
    while (1) {
        if (checkOtaUpdate() != ESP_OK) {
            oledShowString(1, "OTA Update Fail");
            esp_restart();
            break;
        }
        delay(500);
    }
    pthread_exit(NULL);
    return NULL;
}

void otaUpadateCheckStart() {
    if (checkOtaUpdate() != ESP_OK) {
        oledShowString(1, "OTA Check Fail");
        esp_restart();
        return;
    }
    pthread_create(&otaUpdateCheckThreadt, NULL, otaUpadateCheckThread, NULL);
    // pthread_join(otaUpadateCheckThread, NULL);
}