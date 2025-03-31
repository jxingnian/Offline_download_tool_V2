/*
 * @Author: XingNian j_xingnian@163.com
 * @Date: 2024-11-10 11:00:47
 * @LastEditors: jxingnian j_xingnian@163.com
 * @LastEditTime: 2024-12-23 09:08:48
 * @FilePath: \wireless-esp8266-dapc:\XingNian\GeRen\4Gdownload\usb-dap\main\wifi_configuration.h
 * @Description: WiFi配置头文件,包含WiFi连接、mDNS、静态IP、OTA等功能的配置
 * 
 * Copyright (c) 2024 by ${git_name_email}, All Rights Reserved. 
 */

#ifndef __WIFI_CONFIGURATION__
#define __WIFI_CONFIGURATION__
#include "sdkconfig.h" 
/* WiFi连接配置 */
static struct {
    const char *ssid;      // WiFi名称
    const char *password;  // WiFi密码
} wifi_list[] __attribute__((unused)) = {
    {.ssid = CONFIG_WIFI_SSID, .password = CONFIG_WIFI_PASSWORD},
    // 按以下格式添加你的WiFi:
    // {.ssid = "your ssid", .password = "your password"},
};

// WiFi配置列表大小
#define WIFI_LIST_SIZE (sizeof(wifi_list) / sizeof(wifi_list[0]))

/* mDNS配置 */
#define USE_MDNS       1   // 是否启用mDNS服务,1:启用,0:禁用
// 使用"dap.local"访问设备
#define MDNS_HOSTNAME "dap"        // mDNS主机名
#define MDNS_INSTANCE "DAP mDNS"   // mDNS实例名称

/* 静态IP配置 */
#define USE_STATIC_IP 1    // 是否使用静态IP,1:使用,0:不使用
// 如果不想指定IP配置,可以忽略以下项目
#define DAP_IP_ADDRESS 192, 168, 137, 123  // 设备IP地址
#define DAP_IP_GATEWAY 192, 168, 137, 1    // 网关地址
#define DAP_IP_NETMASK 255, 255, 255, 0    // 子网掩码

#define PORT                3240  // 服务端口号
#define CONFIG_EXAMPLE_IPV4 1     // 使用IPv4
#define MTU_SIZE            1500  // 最大传输单元大小



/* printf函数声明和包装 */
extern int printf(const char *, ...);
// printf函数包装,用于系统日志输出
inline int os_printf(const char *__restrict __fmt, ...)  {
    return printf(__fmt, __builtin_va_arg_pack());
}

#endif