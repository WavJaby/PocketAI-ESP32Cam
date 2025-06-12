#ifndef __SPI_RAM_H__
#define __SPI_RAM_H__

#include <stdlib.h>

static void *malloc_spi(size_t size) {
    // check if SPIRAM is enabled and allocate on SPIRAM if allocatable
#if (CONFIG_SPIRAM_SUPPORT && (CONFIG_SPIRAM_USE_CAPS_ALLOC || CONFIG_SPIRAM_USE_MALLOC))
    return heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
#else
    // Allocating in internal memory
    return malloc(size);
#endif
}

#endif