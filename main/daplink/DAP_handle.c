/**
 * @file DAP_handle.c
 * @brief Handle DAP packets and transaction push
 * @version 0.5
 * @change: 2020.02.04 first version
 *          2020.11.11 support WinUSB mode
 *          2021.02.17 support SWO
 *          2021.10.03 try to handle unlink behavior
 *
 * @copyright Copyright (c) 2021
 *
 */

#include <stdint.h>
#include <string.h>

#include "usbip_server.h"
#include "DAP_handle.h"
#include "dap_configuration.h"
#include "wifi/wifi_configuration.h"

#include "components/USBIP/usb_descriptor.h"
#include "components/DAP/Include/DAP.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/ringbuf.h"
#include "freertos/semphr.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>

#if ((USE_MDNS == 1) || (USE_OTA == 1))
    #define DAP_BUFFER_NUM 10
#else
    #define DAP_BUFFER_NUM 20
#endif

#if (USE_WINUSB == 1)
typedef struct
{
    uint32_t length;
    uint8_t buf[DAP_PACKET_SIZE];
} DapPacket_t;
#else
typedef struct
{
    uint8_t buf[DAP_PACKET_SIZE];
} DapPacket_t;
#endif

#define DAP_HANDLE_SIZE (sizeof(DapPacket_t))


extern int kSock;
extern TaskHandle_t kDAPTaskHandle;

int kRestartDAPHandle = NO_SIGNAL;


static DapPacket_t DAPDataProcessed;
static int dap_respond = 0;

// SWO Trace
static uint8_t *swo_data_to_send = NULL;
static uint32_t swo_data_num;

// DAP handle
static RingbufHandle_t dap_dataIN_handle = NULL;
static RingbufHandle_t dap_dataOUT_handle = NULL;
static SemaphoreHandle_t data_response_mux = NULL;


void malloc_dap_ringbuf() {
    if (data_response_mux && xSemaphoreTake(data_response_mux, portMAX_DELAY) == pdTRUE)
    {
        if (dap_dataIN_handle == NULL) {
            dap_dataIN_handle = xRingbufferCreate(DAP_HANDLE_SIZE * DAP_BUFFER_NUM, RINGBUF_TYPE_BYTEBUF);
        }
        if (dap_dataOUT_handle == NULL) {
            dap_dataOUT_handle = xRingbufferCreate(DAP_HANDLE_SIZE * DAP_BUFFER_NUM, RINGBUF_TYPE_BYTEBUF);
        }

        xSemaphoreGive(data_response_mux);
    }
}

void free_dap_ringbuf() {
    if (data_response_mux && xSemaphoreTake(data_response_mux, portMAX_DELAY) == pdTRUE) {
        if (dap_dataIN_handle) {
            vRingbufferDelete(dap_dataIN_handle);
        }
        if (dap_dataOUT_handle) {
            vRingbufferDelete(dap_dataOUT_handle);
        }

        dap_dataIN_handle = dap_dataOUT_handle = NULL;
        xSemaphoreGive(data_response_mux);
    }

}


void handle_dap_data_request(usbip_stage2_header *header, uint32_t length)
{
    uint8_t *data_in = (uint8_t *)header;
    data_in = &(data_in[sizeof(usbip_stage2_header)]);
    // Point to the beginning of the URB packet

#if (USE_WINUSB == 1)
    send_stage2_submit(header, 0, 0);

    // always send constant size buf -> cuz we don't care about the IN packet size
    // and to unify the style, we set aside the length of the section
    xRingbufferSend(dap_dataIN_handle, data_in - sizeof(uint32_t), DAP_HANDLE_SIZE, portMAX_DELAY);
    xTaskNotifyGive(kDAPTaskHandle);

#else
    send_stage2_submit(header, 0, 0);

    xRingbufferSend(dap_dataIN_handle, data_in, DAP_HANDLE_SIZE, portMAX_DELAY);
    xTaskNotifyGive(kDAPTaskHandle);

#endif

    // dap_respond = DAP_ProcessCommand((uint8_t *)data_in, (uint8_t *)data_out);
    // //handle_dap_data_response(header);
    // send_stage2_submit(header, 0, 0);
}

void handle_dap_data_response(usbip_stage2_header *header)
{
    return;
    // int resLength = dap_respond & 0xFFFF;
    // if (resLength)
    // {

    //     send_stage2_submit_data(header, 0, (void *)DAPDataProcessed.buf, resLength);
    //     dap_respond = 0;
    // }
    // else
    // {
    //     send_stage2_submit(header, 0, 0);
    // }
}

void handle_swo_trace_response(usbip_stage2_header *header)
{
#if (SWO_FUNCTION_ENABLE == 1)
    if (kSwoTransferBusy)
    {
        // busy indicates that there is data to be send
        os_printf("swo use data\r\n");
        send_stage2_submit_data(header, 0, (void *)swo_data_to_send, swo_data_num);
        SWO_TransferComplete();
    }
    else
    {
        // nothing to send.
        send_stage2_submit(header, 0, 0);
    }
#else
    send_stage2_submit(header, 0, 0);
#endif
}

// SWO Data Queue Transfer
//   buf:    pointer to buffer with data
//   num:    number of bytes to transfer
void SWO_QueueTransfer(uint8_t *buf, uint32_t num)
{
    swo_data_to_send = buf;
    swo_data_num = num;
}

/*
 * DAP线程函数
 * 处理DAP命令的主要线程，负责接收、处理和发送DAP数据包
 */
void DAP_Thread(void *argument)
{
    // 创建用于DAP数据传输的环形缓冲区和互斥锁
    dap_dataIN_handle = xRingbufferCreate(DAP_HANDLE_SIZE * DAP_BUFFER_NUM, RINGBUF_TYPE_BYTEBUF);
    dap_dataOUT_handle = xRingbufferCreate(DAP_HANDLE_SIZE * DAP_BUFFER_NUM, RINGBUF_TYPE_BYTEBUF);
    data_response_mux = xSemaphoreCreateMutex();
    size_t packetSize;
    int resLength;
    DapPacket_t *item;

    // 检查资源创建是否成功
    if (dap_dataIN_handle == NULL || dap_dataIN_handle == NULL ||
        data_response_mux == NULL)
    {
        os_printf("Can not create DAP ringbuf/mux!\r\n");
        vTaskDelete(NULL);
    }
    for (;;)
    {
        while (1)
        {
            // 处理DAP句柄重启请求
            if (kRestartDAPHandle)
            {
                free_dap_ringbuf();

                if (kRestartDAPHandle == RESET_HANDLE) {
                    malloc_dap_ringbuf();
                    if (dap_dataIN_handle == NULL || dap_dataIN_handle == NULL)
                    {
                        os_printf("Can not create DAP ringbuf/mux!\r\n");
                        vTaskDelete(NULL);
                    }
                }

                kRestartDAPHandle = NO_SIGNAL;
            }

            ulTaskNotifyTake(pdFALSE, portMAX_DELAY); // 等待事件通知

            // 检查缓冲区是否可用
            if (dap_dataIN_handle == NULL || dap_dataOUT_handle == NULL) {
                continue; // 可能正在使用elaphureLink，等待...
            }

            // 从输入缓冲区接收数据包
            packetSize = 0;
            item = (DapPacket_t *)xRingbufferReceiveUpTo(dap_dataIN_handle, &packetSize,
                                                         pdMS_TO_TICKS(1), DAP_HANDLE_SIZE);
            if (packetSize == 0)
            {
                break;
            }
            // 检查数据包大小是否正确
            else if (packetSize < DAP_HANDLE_SIZE)
            {
                os_printf("Wrong data in packet size:%d , data in remain: %d\r\n", packetSize, (int)xRingbufferGetMaxItemSize(dap_dataIN_handle));
                vRingbufferReturnItem(dap_dataIN_handle, (void *)item);
                break;
            }

            // 处理队列命令
            if (item->buf[0] == ID_DAP_QueueCommands)
            {
                item->buf[0] = ID_DAP_ExecuteCommands;
            }

            // 处理DAP命令并获取响应
            resLength = DAP_ProcessCommand((uint8_t *)item->buf, (uint8_t *)DAPDataProcessed.buf);
            resLength &= 0xFFFF; // 响应长度在低16位

            vRingbufferReturnItem(dap_dataIN_handle, (void *)item); // 处理完成，释放输入缓冲

            // 准备回复数据
#if (USE_WINUSB == 1)
            DAPDataProcessed.length = resLength;
#endif
            xRingbufferSend(dap_dataOUT_handle, (void *)&DAPDataProcessed, DAP_HANDLE_SIZE, portMAX_DELAY);

            // 更新响应计数器
            if (xSemaphoreTake(data_response_mux, portMAX_DELAY) == pdTRUE)
            {
                ++dap_respond;
                xSemaphoreGive(data_response_mux);
            }
        }
    }
}

/*
 * 快速回复函数
 * 用于处理DAP数据的快速响应，提高传输效率
 */
int fast_reply(uint8_t *buf, uint32_t length)
{
    usbip_stage2_header *buf_header = (usbip_stage2_header *)buf;
    // 检查请求是否满足快速回复条件
    if (length == 48 &&
        buf_header->base.command == PP_HTONL(USBIP_STAGE2_REQ_SUBMIT) &&
        buf_header->base.direction == PP_HTONL(USBIP_DIR_IN) &&
        buf_header->base.ep == PP_HTONL(1))
    {
        if (dap_respond > 0)
        {
            // 从输出缓冲区获取响应数据
            DapPacket_t *item;
            size_t packetSize = 0;
            item = (DapPacket_t *)xRingbufferReceiveUpTo(dap_dataOUT_handle, &packetSize,
                                                         pdMS_TO_TICKS(10), DAP_HANDLE_SIZE);
            if (packetSize == DAP_HANDLE_SIZE)
            {
                // 发送响应数据
#if (USE_WINUSB == 1)
                send_stage2_submit_data_fast((usbip_stage2_header *)buf, item->buf, item->length);
#else
                send_stage2_submit_data_fast((usbip_stage2_header *)buf, item->buf, DAP_HANDLE_SIZE);
#endif

                // 更新响应计数器
                if (xSemaphoreTake(data_response_mux, portMAX_DELAY) == pdTRUE)
                {
                    --dap_respond;
                    xSemaphoreGive(data_response_mux);
                }

                vRingbufferReturnItem(dap_dataOUT_handle, (void *)item);
                return 1;
            }
            else if (packetSize > 0)
            {
                os_printf("Wrong data out packet size:%d!\r\n", packetSize);
            }
        }
        else
        {
            // 发送空响应
            buf_header->base.command = PP_HTONL(USBIP_STAGE2_RSP_SUBMIT);
            buf_header->base.direction = PP_HTONL(USBIP_DIR_OUT);
            buf_header->u.ret_submit.status = 0;
            buf_header->u.ret_submit.data_length = 0;
            buf_header->u.ret_submit.error_count = 0;
            usbip_network_send(kSock, buf, 48, 0);
            return 1;
        }
    }
    return 0;
}

void handle_dap_unlink()
{
    // `USBIP_CMD_UNLINK` means calling `usb_unlink_urb()` or `usb_kill_urb()`.
    // Note that execution of an URB is inherently an asynchronous operation, and there may be
    // synchronization problems in the following solutions.

    // One of the reasons this happens is that the host wants to abort the URB transfer operation
    // as soon as possible. USBIP network fluctuations will also cause this error, but I don't know
    // whether this is the main reason.

    // Unlink may be applied to zero length URB of "DIR_IN", or a URB containing data.
    // In the case of unlink, for the new "DIR_IN" request, it may always return an older response,
    // which will lead to panic. This code is a compromise for eliminating the lagging response
    // caused by UNLINK. It will clean up the buffers that may have data for return to the host.
    // In general, we assume that there is at most one piece of data that has not yet been returned.
    if (dap_respond > 0)
    {
        DapPacket_t *item;
        size_t packetSize = 0;
        item = (DapPacket_t *)xRingbufferReceiveUpTo(dap_dataOUT_handle, &packetSize,
                                                     pdMS_TO_TICKS(10), DAP_HANDLE_SIZE);
        if (packetSize == DAP_HANDLE_SIZE)
        {
            if (xSemaphoreTake(data_response_mux, portMAX_DELAY) == pdTRUE)
            {
                --dap_respond;
                xSemaphoreGive(data_response_mux);
            }

            vRingbufferReturnItem(dap_dataOUT_handle, (void *)item);
        }
    }
}