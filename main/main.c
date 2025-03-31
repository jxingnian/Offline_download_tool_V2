/*
 * @Author: XingNian j_xingnian@163.com
 * @Date: 2024-11-09 19:46:59
 * @LastEditors: XingNian j_xingnian@163.com
 * @LastEditTime: 2024-11-11 18:52:58
 * @FilePath: \usb-dap\main\main.c
 * @Description: 
 * 
 * Copyright (c) 2024 by ${git_name_email}, All Rights Reserved. 
 */
/*
 * SPDX-FileCopyrightText: 2010-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <stdio.h>
#include <inttypes.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_chip_info.h"
#include "esp_flash.h"

#include "nvs_flash.h"
#include "wifi/wifi_handle.h"
#include "tusb_config.h"

extern void DAP_Setup(void);
extern void tcp_server_task(void *pvParameters);
extern void DAP_Thread(void *pvParameters);

TaskHandle_t kDAPTaskHandle = NULL;

/**
 * @brief 主程序入口函数
 * @note 程序启动
 */
void app_main(void)
{
    bool ret = false;

    /* 打印欢迎信息 */
    printf("Hello DAP_LINK!\n");

    /* 获取芯片信息结构体 */
    esp_chip_info_t chip_info;
    uint32_t flash_size;
    esp_chip_info(&chip_info);
    
    /* 打印芯片基本信息 */
    printf("This is %s chip with %d CPU core(s), WiFi%s%s, ",
           CONFIG_IDF_TARGET,
           chip_info.cores,
           (chip_info.features & CHIP_FEATURE_BT) ? "/BT" : "",
           (chip_info.features & CHIP_FEATURE_BLE) ? "/BLE" : "");

    /* 打印芯片版本信息 */
    unsigned major_rev = chip_info.revision / 100;
    unsigned minor_rev = chip_info.revision % 100;
    printf("silicon revision v%d.%d, ", major_rev, minor_rev);
    
    /* 获取并打印Flash大小 */
    if(esp_flash_get_size(NULL, &flash_size) != ESP_OK) {
        printf("Get flash size failed");
        return;
    }

    printf("%" PRIu32 "MB %s flash\n", flash_size / (uint32_t)(1024 * 1024),
           (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");

    /* 打印最小剩余堆内存大小 */
    printf("Minimum free heap size: %" PRIu32 " bytes\n", esp_get_minimum_free_heap_size());

    ESP_ERROR_CHECK(nvs_flash_init());
    wifi_init();    
    DAP_Setup();
    xTaskCreate(tcp_server_task, "tcp_server", 4096, NULL, 14, NULL);
    xTaskCreate(DAP_Thread, "DAP_Task", 2048, NULL, 10, &kDAPTaskHandle);
}
