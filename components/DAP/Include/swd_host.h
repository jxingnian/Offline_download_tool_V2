/*
 * @Author: xingnian j_xingnian@163.com
 * @Date: 2025-03-31 17:02:53
 * @LastEditors: xingnian j_xingnian@163.com
 * @LastEditTime: 2025-03-31 17:39:16
 * @FilePath: \Offline_download_tool_V2\components\DAP\Include\swd_host.h
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#ifndef SWDHOST_CM_H
#define SWDHOST_CM_H

#include <stdint.h>
#include "flash_blob.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    RESET_HOLD,       // Hold target in reset
    RESET_PROGRAM,    // Reset target and setup for flash programming.
    RESET_RUN,        // Reset target and run normally
    NO_DEBUG,         // Disable debug on running target
    DEBUG,            // Enable debug on running target
    HALT,             // Halt the target without resetting it
    RUN,              // Resume the target without resetting it
    POST_FLASH_RESET, // Reset target after flash programming
    POWER_ON,         // Poweron the target
    SHUTDOWN,         // Poweroff the target
} target_state_t;

typedef enum
{
    FLASHALGO_RETURN_BOOL,
    FLASHALGO_RETURN_POINTER
} flash_algo_return_t;

uint8_t swd_init(void);
uint8_t swd_off(void);
uint8_t swd_init_debug(void);
uint8_t swd_read_dp(uint8_t adr, uint32_t *val);
uint8_t swd_write_dp(uint8_t adr, uint32_t val);
uint8_t swd_read_ap(uint32_t adr, uint32_t *val);
uint8_t swd_write_ap(uint32_t adr, uint32_t val);
uint8_t swd_read_memory(uint32_t address, uint8_t *data, uint32_t size);
uint8_t swd_write_memory(uint32_t address, uint8_t *data, uint32_t size);
uint8_t swd_flash_syscall_exec(const program_syscall_t *sysCallParam, uint32_t entry, uint32_t arg1, uint32_t arg2, uint32_t arg3, uint32_t arg4);
void swd_set_target_reset(uint8_t asserted);
uint8_t swd_set_target_state_hw(target_state_t state);
uint8_t swd_set_target_state_sw(target_state_t state);
uint8_t swd_write_word(uint32_t addr, uint32_t val);
#ifdef __cplusplus
}
#endif

#endif
