#include <stdio.h>
#include "esp_log.h"

#include "timeProbe.h"
#include <stdio.h>
#include "string.h"
#include <esp_attr.h>
#include <esp_random.h>
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


#define WIDTH (240)
#define HEIGHT (240)
#define SIZE (WIDTH*HEIGHT)
/**
 * 原始图像，rgb565彩色图片
 */
EXT_RAM_BSS_ATTR uint16_t image[SIZE] = {0};
/**
 * 将其转为灰度图。
 */
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

int64_t time_single_core;
int64_t time_dual_core;


void calc(int from, int to) {
    for (int i = from; i < to; i++) {
        gray_image[i] = rgb565_to_gray(image[i]);
    }
}


/**
 * make sure this task pin to core 0
 * 这个任务计算前半部分。
 * @param parm
 */
void task_calc_core_0(void *parm) {
    while (1) {
        timeProbe_t timer;
        /////////////////////////////
        timeProbe_start(&timer);
        //告诉另一个核心她也要开始计算了。
        xTaskNotifyGive(taskHandle_core_1);
        //自己也计算。
        calc(0, SIZE / 2);
        //等待另一个核心也计算完成。
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        time_dual_core = timeProbe_stop(&timer);
        ESP_LOGI("dual core calculate", "cost %f ms", (time_dual_core / 1000.0));
        mbedtls_sha256(gray_image, SIZE, sha256_1, 0);
        ESP_LOGI("dual core calculate result sha256", "%0X %0X %0X %0X %0X", sha256_1[0], sha256_1[1], sha256_1[2],
                 sha256_1[3], sha256_1[4]);
        memset(gray_image, 0xaa, SIZE);
        ESP_LOGI("faster", "%f%%",(1.0*time_single_core-time_dual_core)/time_dual_core*100);
        while (1) {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }

}

/**
 * make sure this task pin to core 1
 * 这个任务计算后半部分。
 * @param parm
 */
void task_calc_core_1(void *parm) {
    while (1) {
//        任务启动后，就装死，直到有人唤醒他
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        calc(SIZE / 2, SIZE);
        //算完后告诉主任务自己算完了
        xTaskNotifyGive(taskHandle_core_0);
    }

}

void app_main(void) {
//    填充随机数
    esp_fill_random(image, sizeof(image));

//用于以后计算hash
    mbedtls_sha256_init(&sha256Context);
    timeProbe_t timer;
    timeProbe_start(&timer);
//    完整地一次性单核计算完
    calc(0, SIZE);
    time_single_core = timeProbe_stop(&timer);
    ESP_LOGI("single core calculate", "cost %f ms", (time_single_core / 1000.0));
    mbedtls_sha256(gray_image, SIZE, sha256_0, 0);
    ESP_LOGI("single core calculate result sha256", "%2X %2X %2X %2X %2X", sha256_0[0], sha256_0[1], sha256_0[2],
             sha256_0[3], sha256_0[4]);
    memset(gray_image, 0xaa, SIZE);
////////////////
//  开启两个任务，并且分别绑定到两个核心上。
    xTaskCreatePinnedToCore(task_calc_core_0, "0", 4000, NULL, 1, &taskHandle_core_0, 0);
    xTaskCreatePinnedToCore(task_calc_core_1, "1", 4000, NULL, 1, &taskHandle_core_1, 1);
}
