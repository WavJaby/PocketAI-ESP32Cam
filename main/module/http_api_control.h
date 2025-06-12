#ifndef __HTTP_API_CONTROL__
#define __HTTP_API_CONTROL__

#include <stdint.h>
#include <string.h>
#include <esp_log.h>
#include <esp_http_client.h>

#define HTTP_API_HOST "140.116.246.59"
#define HTTP_API_PORT 25569

// local pc
// #undef HTTP_API_HOST
// #define HTTP_API_HOST "192.168.137.1"

static const char *TAG_HTTP = "main:http";

typedef struct {
    size_t len;
    uint8_t *buff;
} HttpResponseData;

esp_err_t httpEventHandler(esp_http_client_event_t *evt) {
    switch (evt->event_id) {
    case HTTP_EVENT_ERROR:
        ESP_LOGE(TAG_HTTP, "HTTP_EVENT_ERROR");
        break;
    case HTTP_EVENT_ON_HEADER:
        ESP_LOGD(TAG_HTTP, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
        break;
    case HTTP_EVENT_ON_CONNECTED:
        ESP_LOGD(TAG_HTTP, "HTTP_EVENT_ON_CONNECTED");
        break;
    case HTTP_EVENT_ON_DATA:
        ESP_LOGD(TAG_HTTP, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
        if (esp_http_client_is_chunked_response(evt->client)) {
            ESP_LOGE(TAG_HTTP, "Chunk data not support");
            return ESP_FAIL;
        }

        // Http response start, create response data buffer
        HttpResponseData *response = evt->user_data;
        if (!response && evt->data_len) {
            int content_len = esp_http_client_get_content_length(evt->client);
            response = malloc(sizeof(HttpResponseData));
            if (response == NULL) {
                ESP_LOGE(TAG_HTTP, "Failed to allocate HttpResponseData");
                return ESP_FAIL;
            }
            response->buff = (uint8_t *)malloc(content_len + 1);
            response->len = 0;
            if (response->buff == NULL) {
                ESP_LOGE(TAG_HTTP, "Failed to allocate HttpResponseData buffer");
                return ESP_FAIL;
            }

            response->buff[content_len] = 0;
            // Set user data output
            esp_http_client_set_user_data(evt->client, response);
        }

        // Append received buffer
        if (evt->data && evt->data_len) {
            memcpy(response->buff + response->len, evt->data, evt->data_len);
            response->len += evt->data_len;
        }
        break;
    case HTTP_EVENT_DISCONNECTED:
        ESP_LOGD(TAG_HTTP, "HTTP_EVENT_DISCONNECTED");
        break;
    default:
        break;
    }

    return ESP_OK;
}

void httpResponseDataFree(HttpResponseData *data) {
    if (!data) return;
    free(data->buff);
    free(data);
}

esp_err_t httpSendImageProcess(uint8_t *imageBuff, size_t imageSize,
                               char *processId, int len) {
    // POST image request
    esp_http_client_config_t config = {
        .host = HTTP_API_HOST,
        .path = "/img",
        .port = HTTP_API_PORT,
        .method = HTTP_METHOD_POST,
        .timeout_ms = 30000,
        .event_handler = httpEventHandler,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    // esp_http_client_set_header(client, "Connection", "Keep-Alive");
    esp_http_client_set_header(client, "Content-Type", "image/jpeg");
    esp_http_client_set_post_field(client, (char *)imageBuff, imageSize);
    esp_err_t result = esp_http_client_perform(client);
    HttpResponseData *responseData;
    esp_http_client_get_user_data(client, (void **)&responseData);
    if (result != ESP_OK) {
        ESP_LOGE(TAG_HTTP, "HTTP POST request failed");
        httpResponseDataFree(responseData);
        esp_http_client_cleanup(client);
        return ESP_FAIL;
    }
    esp_http_client_cleanup(client);

    // Copy process id
    strncpy(processId, (char *)responseData->buff, len);
    processId[len] = '\0';

    // Dont free buff, string need to use later
    httpResponseDataFree(responseData);
    return ESP_OK;
}

esp_err_t httpWaitImageProcess(uint8_t *imageBuff, char *processId) {
    // POST image request
    esp_http_client_config_t config = {
        .host = HTTP_API_HOST,
        .path = "/img",
        .port = HTTP_API_PORT,
        .method = HTTP_METHOD_GET,
        .timeout_ms = 120000,
        .event_handler = httpEventHandler,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    // Wait result
    client = esp_http_client_init(&config);
    esp_http_client_set_method(client, HTTP_METHOD_GET);
    esp_http_client_set_header(client, "id", processId);

    esp_err_t result = esp_http_client_perform(client);
    int code = esp_http_client_get_status_code(client);
    if (result != ESP_OK || code == 400 || code == 500) {
        ESP_LOGE(TAG_HTTP, "HTTP POST request failed");
        esp_http_client_cleanup(client);
        return ESP_FAIL;
    }
    // Free client data
    esp_http_client_cleanup(client);
    return ESP_OK;
}

esp_err_t httpGetProcessResult(uint8_t *imageBuff, char *processId, ImageData *imageOut) {
    // POST image request
    esp_http_client_config_t config = {
        .host = HTTP_API_HOST,
        .path = "/img",
        .port = HTTP_API_PORT,
        .method = HTTP_METHOD_GET,
        .timeout_ms = 120000,
        .event_handler = httpEventHandler,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    // Wait result
    client = esp_http_client_init(&config);
    esp_http_client_set_method(client, HTTP_METHOD_GET);
    esp_http_client_set_header(client, "id", processId);

    esp_err_t result = esp_http_client_perform(client);
    HttpResponseData *imageResult;
    esp_http_client_get_user_data(client, (void **)&imageResult);
    int code = esp_http_client_get_status_code(client);
    if (result != ESP_OK || code == 400 || code == 500) {
        ESP_LOGE(TAG_HTTP, "HTTP POST request failed");
        httpResponseDataFree(imageResult);
        esp_http_client_cleanup(client);
        return ESP_FAIL;
    }
    // Free client data
    esp_http_client_cleanup(client);

    // Get response data info
    int imgWidth = (imageResult->buff[0] << 24) | (imageResult->buff[1] << 16) | (imageResult->buff[2] << 8) | (imageResult->buff[3]);
    // In pixel
    imageOut->width = imgWidth;
    imageOut->height = imageResult->len * 8 / imgWidth;
    imageOut->output = imageResult->buff;

    // Dont free buff, image need to use later
    free(imageResult);
    return ESP_OK;
}

#endif