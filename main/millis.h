#ifndef __MILLIS_H__
#define __MILLIS_H__

#include <sys/time.h>
#include <freertos/FreeRTOS.h>

#define delay(time) vTaskDelay((time) / portTICK_PERIOD_MS)

static inline uint64_t millis() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (tv.tv_sec * 1000LL + (tv.tv_usec / 1000LL));
}

#endif