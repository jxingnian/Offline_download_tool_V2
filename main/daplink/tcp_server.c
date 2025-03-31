/**
 * @file tcp_server.c
 * @brief TCP服务器任务处理,实现TCP连接的建立、数据收发和状态管理
 * @version 0.1
 * @date 2020-01-22
 *
 * @copyright Copyright (c) 2020
 *
 */

/* 标准库头文件 */
#include <string.h>
#include <stdint.h>
#include <sys/param.h>

/* 项目相关头文件 */
#include "wifi/wifi_configuration.h"  // WiFi配置
#include "usbip_server.h"       // USBIP服务器
#include "DAP_handle.h"         // DAP处理

/* elaphureLink协议头文件 */
#include "components/elaphureLink/elaphureLink_protocol.h"

/* FreeRTOS相关头文件 */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

/* ESP32系统相关头文件 */
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"

/* lwIP网络协议栈头文件 */
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>

/* 外部变量声明 */
extern TaskHandle_t kDAPTaskHandle;  // DAP任务句柄,用于任务间通信
extern int kRestartDAPHandle;        // DAP重启标志,控制DAP任务的重启

/* 全局变量定义 */
uint8_t kState = ACCEPTING;          // 当前连接状态,初始为等待连接状态
int kSock = -1;                      // 当前活动的Socket描述符

/**
 * @brief TCP服务器主任务函数
 * @details 负责创建TCP服务器,接受客户端连接,处理数据收发
 * @param pvParameters FreeRTOS任务参数(未使用)
 */
void tcp_server_task(void *pvParameters)
{
    uint8_t tcp_rx_buffer[1500];     // TCP数据接收缓冲区
    char addr_str[128];              // IP地址字符串缓冲区
    int addr_family;                 // 地址族(IPv4/IPv6)
    int ip_protocol;                 // IP协议类型
    int on = 1;                      // Socket选项开关值

    while (1) // 主循环,用于服务器重启
    {
        /* IPv4/IPv6协议配置 */
#ifdef CONFIG_EXAMPLE_IPV4
        /* IPv4地址配置 */
        struct sockaddr_in destAddr;
        destAddr.sin_addr.s_addr = htonl(INADDR_ANY);  // 监听所有网卡
        destAddr.sin_family = AF_INET;                 // IPv4协议族
        destAddr.sin_port = htons(PORT);               // 服务器端口
        addr_family = AF_INET;                         
        ip_protocol = IPPROTO_IP;
        inet_ntoa_r(destAddr.sin_addr, addr_str, sizeof(addr_str) - 1);
#else // IPV6
        /* IPv6地址配置 */
        struct sockaddr_in6 destAddr;
        bzero(&destAddr.sin6_addr.un, sizeof(destAddr.sin6_addr.un));
        destAddr.sin6_family = AF_INET6;               // IPv6协议族
        destAddr.sin6_port = htons(PORT);              // 服务器端口
        addr_family = AF_INET6;
        ip_protocol = IPPROTO_IPV6;
        inet6_ntoa_r(destAddr.sin6_addr, addr_str, sizeof(addr_str) - 1);
#endif

        /* 创建监听Socket */
        int listen_sock = socket(addr_family, SOCK_STREAM, ip_protocol);
        if (listen_sock < 0)
        {
            os_printf("Unable to create socket: errno %d\r\n", errno);
            break;
        }
        os_printf("Socket created\r\n");

        /* 设置Socket选项:
         * SO_KEEPALIVE: 启用TCP保活机制
         * TCP_NODELAY: 禁用Nagle算法,降低延迟
         */
        setsockopt(listen_sock, SOL_SOCKET, SO_KEEPALIVE, (void *)&on, sizeof(on));
        setsockopt(listen_sock, IPPROTO_TCP, TCP_NODELAY, (void *)&on, sizeof(on));

        /* 绑定Socket到指定地址和端口 */
        int err = bind(listen_sock, (struct sockaddr *)&destAddr, sizeof(destAddr));
        if (err != 0)
        {
            os_printf("Socket unable to bind: errno %d\r\n", errno);
            break;
        }
        os_printf("Socket binded\r\n");

        /* 开始监听连接请求 
         * 参数1表示最大待处理连接数
         */
        err = listen(listen_sock, 1);
        if (err != 0)
        {
            os_printf("Error occured during listen: errno %d\r\n", errno);
            break;
        }
        os_printf("Socket listening\r\n");

        /* 定义客户端地址结构 */
#ifdef CONFIG_EXAMPLE_IPV6
        struct sockaddr_in6 sourceAddr; // IPv6客户端地址
#else
        struct sockaddr_in sourceAddr;  // IPv4客户端地址
#endif
        uint32_t addrLen = sizeof(sourceAddr);
        
        /* 连接处理循环 */
        while (1)
        {
            /* 接受新的客户端连接 */
            kSock = accept(listen_sock, (struct sockaddr *)&sourceAddr, &addrLen);
            if (kSock < 0)
            {
                os_printf("Unable to accept connection: errno %d\r\n", errno);
                break;
            }

            /* 为新连接设置Socket选项 */
            setsockopt(kSock, SOL_SOCKET, SO_KEEPALIVE, (void *)&on, sizeof(on));
            setsockopt(kSock, IPPROTO_TCP, TCP_NODELAY, (void *)&on, sizeof(on));
            os_printf("Socket accepted\r\n");

            /* 数据接收处理循环 */
            while (1)
            {
                /* 接收数据 */
                int len = recv(kSock, tcp_rx_buffer, sizeof(tcp_rx_buffer), 0);

                /* 接收错误处理 */
                if (len < 0)
                {
                    os_printf("recv failed: errno %d\r\n", errno);
                    break;
                }
                /* 连接关闭处理 */
                else if (len == 0)
                {
                    os_printf("Connection closed\r\n");
                    break;
                }
                /* 正常数据处理 */
                else
                {
                    /* 根据当前状态处理接收到的数据 */
                    switch (kState)
                    {
                    case ACCEPTING:
                        kState = ATTACHING;    // 更新状态为正在连接
                        // fallthrough         // 继续执行ATTACHING的处理
                    case ATTACHING:
                        /* 尝试elaphureLink协议握手 */
                        if (el_handshake_process(kSock, tcp_rx_buffer, len) == 0) {
                            // 握手成功,切换到elaphureLink数据传输阶段
                            kState = EL_DATA_PHASE;
                            kRestartDAPHandle = DELETE_HANDLE;
                            el_process_buffer_malloc();
                            break;
                        }

                        /* 如果不是elaphureLink协议,则按USBIP协议处理 */
                        attach(tcp_rx_buffer, len);
                        break;

                    case EMULATING:
                        /* USBIP协议数据传输阶段 */
                        emulate(tcp_rx_buffer, len);
                        break;

                    case EL_DATA_PHASE:
                        /* elaphureLink协议数据传输阶段 */
                        el_dap_data_process(tcp_rx_buffer, len);
                        break;

                    default:
                        os_printf("unkonw kstate!\r\n");
                    }
                }
            }

            /* 连接清理工作 */
            if (kSock != -1)
            {
                os_printf("Shutting down socket and restarting...\r\n");
                close(kSock);    // 关闭Socket

                /* 重置连接状态 */
                if (kState == EMULATING || kState == EL_DATA_PHASE)
                    kState = ACCEPTING;

                /* 清理DAP相关资源 */
                el_process_buffer_free();

                /* 重启DAP任务 */
                kRestartDAPHandle = RESET_HANDLE;
                if (kDAPTaskHandle)
                    xTaskNotifyGive(kDAPTaskHandle);
            }
        }
    }
    vTaskDelete(NULL);    // 删除当前任务
}