#ifndef __DAP_CONFIGURATION_H__
#define __DAP_CONFIGURATION_H__

/**
 * @brief 指定是否使用WINUSB
 *
 */
#define USE_WINUSB 1

/**
 * @brief 启用此选项后,无需物理连接MOSI和MISO引脚
 *
 */
#define USE_SPI_SIO 1

/**
 * @brief 指定是否启用USB 3.0
 *
 */
#define USE_USB_3_0 0

// 对于USB 3.0,端点大小必须为1024字节
#if (USE_USB_3_0 == 1)
    #define USB_ENDPOINT_SIZE 1024U
#else
    #define USB_ENDPOINT_SIZE 512U
#endif

/// 命令和响应数据的最大包大小
/// 此配置用于优化与调试器的通信性能,取决于USB外设
/// 典型值:全速USB HID或WinUSB为64字节
/// 高速USB HID为1024字节,高速USB WinUSB为512字节
#if (USE_WINUSB == 1)
    #define DAP_PACKET_SIZE 512U // WinUSB模式下为512字节
#else
    #define DAP_PACKET_SIZE 255U // USB HID模式下为255字节
#endif

#endif

/**
 * @brief 启用此选项以在复位设备时强制执行软件复位
 *
 * 某些调试器软件(如Keil)在复位目标时不执行软件复位。
 * 启用此选项后,当执行DAP_ResetTarget()时会尝试软复位。
 * 这是通过写入Cortex-M架构中AIRCR寄存器的SYSRESETREQ位来实现的。
 *
 * 此功能应该适用于ARMv6-m、ARMv7-m和ARMv8-m架构。
 * 但不能保证复位操作一定会正确执行。
 *
 * 仅在SWD模式下可用。
 */
#define USE_FORCE_SYSRESETREQ_AFTER_FLASH 1