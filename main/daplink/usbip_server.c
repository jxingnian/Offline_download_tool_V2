// 包含标准库头文件
#include <stdint.h>
#include <string.h>

// 包含项目头文件
#include "usbip_server.h"
// #include "main/kcp_server.h"
// #include "main/tcp_netconn.h"
#include "DAP_handle.h"
#include "wifi/wifi_configuration.h"

// 包含USBIP组件头文件
#include "components/USBIP/usb_handle.h"
#include "components/USBIP/usb_descriptor.h"

// 包含lwip网络协议栈头文件
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>


// attach阶段相关的辅助函数声明
static int read_stage1_command(uint8_t *buffer, uint32_t length);
static void handle_device_list(uint8_t *buffer, uint32_t length);
static void handle_device_attach(uint8_t *buffer, uint32_t length);
static void send_stage1_header(uint16_t command, uint32_t status);
static void send_device_list();
static void send_device_info();
static void send_interface_info();

// 模拟USB设备相关的辅助函数声明
static void pack(void *data, int size);
static void unpack(void *data, int size);
static int handle_submit(usbip_stage2_header *header, uint32_t length);
static int read_stage2_command(usbip_stage2_header *header, uint32_t length);

static void handle_unlink(usbip_stage2_header *header);
// unlink相关的辅助函数声明
static void send_stage2_unlink(usbip_stage2_header *req_header);

// 根据不同的网络传输方式发送数据
int usbip_network_send(int s, const void *dataptr, size_t size, int flags) {
#if (USE_KCP == 1)
    // return kcp_network_send(dataptr, size);
#elif (USE_TCP_NETCONN == 1)
    // return tcp_netconn_send(dataptr, size);
#else // BSD socket方式
    return send(s, dataptr, size, flags);
#endif
}

// 处理USB设备attach阶段的请求
int attach(uint8_t *buffer, uint32_t length)
{
    int command = read_stage1_command(buffer, length);
    if (command < 0)
    {
        return -1;
    }

    switch (command)
    {
    case USBIP_STAGE1_CMD_DEVICE_LIST: // 处理获取设备列表请求
        handle_device_list(buffer, length);
        break;

    case USBIP_STAGE1_CMD_DEVICE_ATTACH: // 处理设备attach请求
        handle_device_attach(buffer, length);
        break;

    default:
        os_printf("attach Unknown command: %d\r\n", command);
        break;
    }
    return 0;
}

// 读取stage1阶段的命令类型
static int read_stage1_command(uint8_t *buffer, uint32_t length)
{
    if (length < sizeof(usbip_stage1_header))
    {
        return -1;
    }
    usbip_stage1_header *req = (usbip_stage1_header *)buffer;
    return (ntohs(req->command) & 0xFF); // 取低8位作为命令类型
}

// 处理获取设备列表请求
static void handle_device_list(uint8_t *buffer, uint32_t length)
{
    os_printf("Handling dev list request...\r\n");
    send_stage1_header(USBIP_STAGE1_CMD_DEVICE_LIST, 0);
    send_device_list();
}

// 处理设备attach请求
static void handle_device_attach(uint8_t *buffer, uint32_t length)
{
    os_printf("Handling dev attach request...\r\n");

    //char bus[USBIP_BUSID_SIZE];
    if (length < sizeof(USBIP_BUSID_SIZE))
    {
        os_printf("handle device attach failed!\r\n");
        return;
    }
    //client.readBytes((uint8_t *)bus, USBIP_BUSID_SIZE);

    send_stage1_header(USBIP_STAGE1_CMD_DEVICE_ATTACH, 0);

    send_device_info();

    kState = EMULATING;
}

// 发送stage1阶段的响应头
static void send_stage1_header(uint16_t command, uint32_t status)
{
    os_printf("Sending header...\r\n");
    usbip_stage1_header header;
    header.version = htons(273); // USBIP协议版本号
    // 参考: https://github.com/Oxalin/usbip_windows/issues/4

    header.command = htons(command);
    header.status = htonl(status);

    usbip_network_send(kSock, (uint8_t *)&header, sizeof(usbip_stage1_header), 0);
}

// 发送设备列表信息
static void send_device_list()
{
    os_printf("Sending device list...\r\n");

    // 发送设备列表大小
    os_printf("Sending device list size...\r\n");
    usbip_stage1_response_devlist response_devlist;

    // 只有一个设备
    response_devlist.list_size = htonl(1);

    usbip_network_send(kSock, (uint8_t *)&response_devlist, sizeof(usbip_stage1_response_devlist), 0);

    // 如果有多个设备可以使用循环:

    {
        // 发送设备信息
        send_device_info();
        // 发送设备接口信息
        send_interface_info();
    }
}

// 发送USB设备描述符信息
static void send_device_info()
{
    os_printf("Sending device info...\r\n");
    usbip_stage1_usb_device device;

    strcpy(device.path, "/sys/devices/pci0000:00/0000:00:01.2/usb1/1-1");
    strcpy(device.busid, "1-1");

    device.busnum = htonl(1);
    device.devnum = htonl(1);
    device.speed = htonl(3); // USB设备速度,参考usb_device_speed枚举

    device.idVendor = htons(USBD0_DEV_DESC_IDVENDOR);
    device.idProduct = htons(USBD0_DEV_DESC_IDPRODUCT);
    device.bcdDevice = htons(USBD0_DEV_DESC_BCDDEVICE);

    device.bDeviceClass = 0x00; // 使用非USB-IF标准设备类,设为0x00
    device.bDeviceSubClass = 0x00;
    device.bDeviceProtocol = 0x00;

    device.bConfigurationValue = 1;
    device.bNumConfigurations = 1;
    device.bNumInterfaces = 1;

    usbip_network_send(kSock, (uint8_t *)&device, sizeof(usbip_stage1_usb_device), 0);
}

// 发送USB接口描述符信息
static void send_interface_info()
{
    os_printf("Sending interface info...\r\n");
    usbip_stage1_usb_interface interface;
    interface.bInterfaceClass = USBD_CUSTOM_CLASS0_IF0_CLASS;
    interface.bInterfaceSubClass = USBD_CUSTOM_CLASS0_IF0_SUBCLASS;
    interface.bInterfaceProtocol = USBD_CUSTOM_CLASS0_IF0_PROTOCOL;
    interface.padding = 0; // 填充字段必须为0

    usbip_network_send(kSock, (uint8_t *)&interface, sizeof(usbip_stage1_usb_interface), 0);
}

///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////

// 处理USB设备模拟阶段的请求
int emulate(uint8_t *buffer, uint32_t length)
{

    if(fast_reply(buffer, length))
    {
        return 0;
    }

    int command = read_stage2_command((usbip_stage2_header *)buffer, length);
    if (command < 0)
    {
        return -1;
    }

    switch (command)
    {
    case USBIP_STAGE2_REQ_SUBMIT: // 处理URB提交请求
        handle_submit((usbip_stage2_header *)buffer , length);
        break;

    case USBIP_STAGE2_REQ_UNLINK: // 处理URB取消请求
        handle_unlink((usbip_stage2_header *)buffer);
        break;

    default:
        os_printf("emulate unknown command:%d\r\n", command);
        //handle_submit((usbip_stage2_header *)buffer, length);
        return -1;
    }
    return 0;
}

// 读取stage2阶段的命令类型
static int read_stage2_command(usbip_stage2_header *header, uint32_t length)
{
    if (length < sizeof(usbip_stage2_header))
    {
        return -1;
    }

    //client.readBytes((uint8_t *)&header, sizeof(usbip_stage2_header));
    unpack((uint32_t *)header, sizeof(usbip_stage2_header));
    return header->base.command;
}

/**
 * @brief 打包以下数据包头(偏移0x00 - 0x28):
 *       - cmd_submit
 *       - ret_submit
 *       - cmd_unlink
 *       - ret_unlink
 *
 * @param data 指向数据包头的指针
 * @param size 数据包头大小
 */
static void pack(void *data, int size)
{

    // 忽略setup字段
    int sz = (size / sizeof(uint32_t)) - 2;
    uint32_t *ptr = (uint32_t *)data;

    for (int i = 0; i < sz; i++)
    {

        ptr[i] = htonl(ptr[i]);
    }
}

/**
 * @brief 解包以下数据包头(偏移0x00 - 0x28):
 *       - cmd_submit
 *       - ret_submit
 *       - cmd_unlink
 *       - ret_unlink
 *
 * @param data 指向数据包头的指针
 * @param size 数据包头大小
 */
static void unpack(void *data, int size)
{

    // 忽略setup字段
    int sz = (size / sizeof(uint32_t)) - 2;
    uint32_t *ptr = (uint32_t *)data;

    for (int i = 0; i < sz; i++)
    {
        ptr[i] = ntohl(ptr[i]);
    }
}

/**
 * @brief 处理USB传输请求
 *
 */
static int handle_submit(usbip_stage2_header *header, uint32_t length)
{
    switch (header->base.ep)
    {
    // 控制端点(端点0)
    case 0x00:
        //// TODO: 判断USB setup包是否为8字节
        handleUSBControlRequest(header);
        break;

    // 端点1用于数据接收和响应
    case 0x01:
        if (header->base.direction == 0)
        {
            //os_printf("EP 01 DATA FROM HOST");
            handle_dap_data_request(header ,length);
        }
        else
        {
            // os_printf("EP 01 DATA TO HOST\r\n");
            handle_dap_data_response(header);
        }
        break;
    // 端点2用于SWO跟踪
    case 0x02:
        if (header->base.direction == 0)
        {
            // os_printf("EP 02 DATA FROM HOST");
            send_stage2_submit(header, 0, 0);
        }
        else
        {
            // os_printf("EP 02 DATA TO HOST");
            handle_swo_trace_response(header);
        }
        break;
    // 保存数据到设备的请求
    case 0x81:
        if (header->base.direction == 0)
        {
            os_printf("*** WARN! EP 81 DATA TX");
        }
        else
        {
            os_printf("*** WARN! EP 81 DATA RX");
        }
        return -1;

    default:
        os_printf("*** WARN ! UNKNOWN ENDPOINT: %d\r\n", (int)header->base.ep);
        return -1;
    }
    return 0;
}

// 发送stage2 submit响应
void send_stage2_submit(usbip_stage2_header *req_header, int32_t status, int32_t data_length)
{

    req_header->base.command = USBIP_STAGE2_RSP_SUBMIT;
    req_header->base.direction = !(req_header->base.direction);

    memset(&(req_header->u.ret_submit), 0, sizeof(usbip_stage2_header_ret_submit));

    req_header->u.ret_submit.status = status;
    req_header->u.ret_submit.data_length = data_length;
    // 已经解包过的数据需要重新打包
    pack(req_header, sizeof(usbip_stage2_header));
    usbip_network_send(kSock, req_header, sizeof(usbip_stage2_header), 0);
}

// 发送stage2 submit响应和数据
void send_stage2_submit_data(usbip_stage2_header *req_header, int32_t status, const void *const data, int32_t data_length)
{

    send_stage2_submit(req_header, status, data_length);

    if (data_length)
    {
        usbip_network_send(kSock, data, data_length, 0);
    }
}

// 快速发送stage2 submit响应和数据
void send_stage2_submit_data_fast(usbip_stage2_header *req_header, const void *const data, int32_t data_length)
{
    uint8_t * send_buf = (uint8_t *)req_header;

    req_header->base.command = PP_HTONL(USBIP_STAGE2_RSP_SUBMIT);
    req_header->base.direction = htonl(!(req_header->base.direction));

    memset(&(req_header->u.ret_submit), 0, sizeof(usbip_stage2_header_ret_submit));
    req_header->u.ret_submit.data_length = htonl(data_length);


    // 复制数据负载
    memcpy(&send_buf[sizeof(usbip_stage2_header)], data, data_length);
    usbip_network_send(kSock, send_buf, sizeof(usbip_stage2_header) + data_length, 0);
}


// 处理URB取消请求
static void handle_unlink(usbip_stage2_header *header)
{
    os_printf("s2 handling cmd unlink...\r\n");
    handle_dap_unlink();
    send_stage2_unlink(header);
}

// 发送stage2 unlink响应
static void send_stage2_unlink(usbip_stage2_header *req_header)
{

    req_header->base.command = USBIP_STAGE2_RSP_UNLINK;
    req_header->base.direction = USBIP_DIR_OUT;

    memset(&(req_header->u.ret_unlink), 0, sizeof(usbip_stage2_header_ret_unlink));

    // 更准确的说应该是 `-ECONNRESET`, 但usbip-win只关心是否为非零值
    // 非零值表示UNLINK操作"成功",但主机驱动可能有不同的行为,甚至可能忽略这个状态
    // 为了保持一致性,这里使用非零值。另见handle_dap_unlink()的相关注释
    req_header->u.ret_unlink.status = -1;

    pack(req_header, sizeof(usbip_stage2_header));

    usbip_network_send(kSock, req_header, sizeof(usbip_stage2_header), 0);
}