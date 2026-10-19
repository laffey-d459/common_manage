/********************************************************************************************
 * file description: 串行异步通信
 *
 * Version: V1.0.0
 * Copyright (c) 2026, laffey-d459.
 * Licensed under the MIT License.
 *
 * Modify Record:
 * 2026/09/17  laffey-d459  V1.0.0  初始版本
 ********************************************************************************************/
#ifndef __SERIAL_H
#define __SERIAL_H

#include <stdint.h>
#include "ring_buffer.h"

typedef size_t (*srl_function)(void *buf, size_t size);

typedef enum
{
    HSRL_NO_INIT = 0, // 句柄未初始化
    HSRL_OK,          // 句柄可以使用
    HSRL_BUSY,        // 句柄正在使用
} HSRL_STATE_T;

typedef enum
{
    SRL_OK = 0,          // 操作成功
    SRL_INVALID_VALUE,   // 无效参数
    SRL_NOT_ENOUGH,      // 无足够数据以执行相应操作
    SRL_MEM_ERROR,       // 内存操作错误
    SRL_BUSY,            // 正忙
    SRL_HANDLE_UNUSABLE, // 句柄不可用
} SRL_STATE_T;

typedef struct
{
    ringbuf_t *hrb_tx;     // 发送环形缓冲结构体句柄
    ringbuf_t *hrb_rx;     // 接收环形缓冲结构体句柄
    void *rb_tx_buf;       // 发送环形缓冲区
    void *rb_rx_buf;       // 接收环形缓冲区
    size_t rb_tx_buf_size; // 发送环形缓冲区的字节大小
    size_t rb_rx_buf_size; // 接收环形缓冲区的字节大小

    srl_function func_send; /* @brief 实现1：异步发送数据（用于类似dma发送）
                             * @param buf 待发送数据缓存区
                             * @param size 待发送数据字节长度
                             * @return void
                             * @note 1.启动发送后返回（类似启动dma后返回）
                             *       2.实际完成一次发送后必须调用 srl_send_data_callback
                             * */

    srl_function func_rece; /* @brief 实现1：异步接收数据（用于类似dma接收数据）
                             * @param buf void
                             * @param size void
                             * @return 实际接收的数据的字节长度
                             * @note 1.实际完成一次接收后必须调用 srl_rece_data_callback
                             * */

} srl_init_t;

typedef struct
{
    volatile HSRL_STATE_T handle_state;
    volatile uint8_t is_tx_busy;
    volatile uint8_t is_data_cut;

    volatile size_t tx_rema_size;

    ringbuf_t *hrb_tx;
    ringbuf_t *hrb_rx;

    void *tx_data_addr;

    srl_function func_send_data;
    srl_function func_rece_data;

} srl_t;

SRL_STATE_T srl_init(srl_t *h_srl, srl_init_t *h_srl_initstruct);
SRL_STATE_T srl_send_data(srl_t *h_srl, uint8_t *buf, uint16_t len);
SRL_STATE_T srl_rece_data(srl_t *h_srl, uint8_t *buf, uint16_t len);

SRL_STATE_T srl_send_data_callback(srl_t *h_srl);
SRL_STATE_T srl_rece_data_callback(srl_t *h_srl);

/// @brief 获取未处理的发送数据的字节长度
/// @param h_srl 串行通信句柄
/// @return 数据的字节长度
static inline size_t srl_get_tx_usize(srl_t *h_srl)
{
    if (h_srl == 0 || h_srl->hrb_tx == 0)
    {
        return 0;
    }

    return ringbuf_get_stored_size(h_srl->hrb_tx);
}

/// @brief 获取未处理的接收数据的字节长度
/// @param h_srl 串行通信句柄
/// @return 数据的字节长度
static inline size_t srl_get_rx_usize(srl_t *h_srl)
{
    if (h_srl == 0 || h_srl->hrb_rx == 0)
    {
        return 0;
    }

    return ringbuf_get_stored_size(h_srl->hrb_rx);
}

#endif
