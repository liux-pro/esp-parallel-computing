#include <stdio.h>
#include "esp_log.h"

#include "timeProbe.h"
#include <stdio.h>
#include "string.h"
#include <esp_attr.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "mbedtls/sha256.h"

// Convert 16-bit RGB to 8-bit grayscale
uint8_t rgb565_to_gray(uint16_t color) {
    uint8_t red = (color >> 11) & 0x1F;
    uint8_t green = (color >> 5) & 0x3F;
    uint8_t blue = color & 0x1F;
    return (uint8_t) ((red * 299 + green * 587 + blue * 114) / 1000);
}

void rgb565_to_gray_p(const uint16_t *color, uint8_t *gray) {
    uint8_t red = (*color >> 11) & 0x1F;
    uint8_t green = (*color >> 5) & 0x3F;
    uint8_t blue = *color & 0x1F;
    *gray = ((red * 299 + green * 587 + blue * 114) / 1000);
}

#define WIDTH (240)
#define HEIGHT (240)
#define SIZE (WIDTH*HEIGHT)
EXT_RAM_BSS_ATTR uint16_t image[SIZE] = {0};
 uint8_t gray_image[SIZE];


mbedtls_sha256_context sha256Context;
/**
 * 用普通方法计算结果，并计算sha256
 */
uint8_t sha256_0[32] = {0};
/**
 * 用双核的方法计算结果，并计算sha256
 */
uint8_t sha256_1[32] = {0};

TaskHandle_t taskHandle_core_0;
TaskHandle_t taskHandle_core_1;

// 91ms   888*888



void calc(int from, int to) {
    for (int i = from; i < to; i++) {
        gray_image[i] = rgb565_to_gray(image[i]);
    }
}

void calc_fast(int from, int to) {
    for (int i = from; i < to; i++) {
        rgb565_to_gray_p(image + i, gray_image + i);
    }
}

/**
 * make sure this task pin to core 0
 * @param parm
 */
void task_calc_core_0(void *parm) {
    while (1) {
        timeProbe_t timer;
        /////////////////////////////
        timeProbe_start(&timer);
        xTaskNotifyGive(taskHandle_core_1);
        calc(0, SIZE / 2);
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        ESP_LOGI("dual core calculate", "cost %f ms", (timeProbe_stop(&timer) / 1000.0));
        mbedtls_sha256(gray_image, SIZE, sha256_1, 0);
        ESP_LOGI("dual core calculate result sha256", "%0X %0X %0X %0X %0X", sha256_1[0], sha256_1[1], sha256_1[2],
                 sha256_1[3], sha256_1[4]);
        memset(gray_image, 0xaa, SIZE);
    }

}

/**
 * make sure this task pin to core 1
 * @param parm
 */
void task_calc_core_1(void *parm) {
    while (1) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        ESP_LOGI("11111111111111111", "222222222");
        calc(SIZE / 2, SIZE);
        xTaskNotifyGive(taskHandle_core_0);
    }

}

void app_main(void) {
    for (int i = 0; i < SIZE; ++i) {
        image[i] = rand();
    }

    timeProbe_t timer;
    mbedtls_sha256_init(&sha256Context);
    timeProbe_start(&timer);
    calc(0, SIZE);
    ESP_LOGI("single core calculate", "cost %f ms", (timeProbe_stop(&timer) / 1000.0));
    mbedtls_sha256(gray_image, SIZE, sha256_0, 0);
    ESP_LOGI("single core calculate result sha256", "%2X %2X %2X %2X %2X", sha256_0[0], sha256_0[1], sha256_0[2],
             sha256_0[3], sha256_0[4]);
    memset(gray_image, 0xaa, SIZE);
////////////////
    xTaskCreatePinnedToCore(task_calc_core_0, "0", 4000, NULL, 1, &taskHandle_core_0, 0);
    xTaskCreatePinnedToCore(task_calc_core_1, "1", 4000, NULL, 1, &taskHandle_core_1, 1);
}
