#include <stdbool.h>

#include <esp_jpg_decode.h>

#include "spi_ram.h"

typedef struct {
    uint16_t width;
    uint16_t height;
    uint8_t *input;
    uint8_t *output;
} ImageData;

// output buffer and image width
static bool _rgb_write(void *arg, uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint8_t *data) {
    ImageData *img = (ImageData *)arg;
    if (!data) {
        if (x == 0 && y == 0) {
            // write start
            img->width = w;
            img->height = h;
            if (!img->output) {
                img->output = (uint8_t *)malloc_spi((w * h * 3));
                if (!img->output) {
                    return false;
                }
            }
        }
        return true;
    }

    size_t jw = img->width * 3;
    size_t t = y * jw;
    size_t b = t + (h * jw);
    size_t l = x * 3;
    uint8_t *out = img->output;
    uint8_t *o;
    size_t iy, ix;

    w = w * 3;

    for (iy = t; iy < b; iy += jw) {
        o = out + iy + l;
        for (ix = 0; ix < w; ix += 3) {
            o[ix] = data[ix];
            o[ix + 1] = data[ix + 1];
            o[ix + 2] = data[ix + 2];
        }
        data += w;
    }
    return true;
}

// input buffer
static unsigned int _jpg_read(void *arg, size_t index, uint8_t *buf, size_t len) {
    ImageData *jpeg = (ImageData *)arg;
    if (buf) {
        memcpy(buf, jpeg->input + index, len);
    }
    return len;
}

bool jpg2bw(uint8_t* data, size_t len, jpg_scale_t scale, ImageData *imgOut) {
    imgOut->input = data;
    imgOut->output = NULL;
    
    if (esp_jpg_decode(len, scale, _jpg_read, _rgb_write, (void *)imgOut) != ESP_OK)
        return false;

    return true;
}

void freeImageData(ImageData *jpeg) {
    free(jpeg->output);
}