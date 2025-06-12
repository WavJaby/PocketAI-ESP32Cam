
#include <esp_err.h>

#if defined NDEBUG || defined CONFIG_COMPILER_OPTIMIZATION_ASSERTIONS_SILENT

#define ESP_RETURN_ON_ERROR(x)   \
    do {                         \
        esp_err_t err_rc_ = x;   \
        if (err_rc_ != ESP_OK) { \
            return err_rc_;      \
        }                        \
    } while (0)

#else
#define ESP_RETURN_ON_ERROR(x)                                   \
    do {                                                         \
        esp_err_t err_rc_ = (x);                                 \
        if (err_rc_ != ESP_OK) {                                 \
            _esp_error_check_failed(err_rc_, __FILE__, __LINE__, \
                                    __ASSERT_FUNC, #x);          \
            return err_rc_;                                      \
        }                                                        \
    } while (0)
#endif