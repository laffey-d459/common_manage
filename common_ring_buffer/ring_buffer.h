/********************************************************************************************
 * file description: 环形缓存区
 *
 * Version: V1.0.0
 * Copyright (c) 2026, laffey-d459.
 * Licensed under the MIT License.
 *
 * Modify Record:
 * 2026/09/17  laffey-d459  V1.0.0  初始版本
 ********************************************************************************************/
#ifndef __RING_BUFFER_H
#define __RING_BUFFER_H

#include <string.h>
#include <stdint.h>

typedef enum
{
    RBUF_OK = 0x00U,    // 操作成功
    RBUF_INVALID_VALUE, // 无效参数
    RBUF_NOT_ENOUGH,    // 无足够数据以执行相应操作
} RBUF_STATE_T;

typedef struct
{
    void *buf;   // 环形缓存区内存地址
    size_t size; // 缓冲区总大小（字节）

    volatile size_t borrowed_size; // 借用的内存大小

    volatile size_t head; // 写入索引
    volatile size_t tail; // 读取索引
} ringbuf_t;

RBUF_STATE_T ringbuf_init(ringbuf_t *h_rbuf, void *buf, size_t size);
RBUF_STATE_T ringbuf_write(ringbuf_t *h_rbuf, void *buf, size_t size);
RBUF_STATE_T ringbuf_read(ringbuf_t *h_rbuf, void *buf, size_t size);
RBUF_STATE_T ringbuf_submit(ringbuf_t *h_rbuf, uint8_t *is_cover, size_t size);
size_t ringbuf_borrow(ringbuf_t *h_rbuf, void **addr, uint8_t *is_cut, size_t size);
RBUF_STATE_T ringbuf_reback(ringbuf_t *h_rbuf, void **addr, size_t size);
size_t ringbuf_check_borrow_size(ringbuf_t *h_rbuf);
size_t ringbuf_get_stored_size(const ringbuf_t *h_rbuf);
size_t ringbuf_get_space_size(const ringbuf_t *h_rbuf);
uint8_t ringbuf_is_empty(const ringbuf_t *h_rbuf);
uint8_t ringbuf_is_full(const ringbuf_t *h_rbuf);
RBUF_STATE_T ringbuf_clear(ringbuf_t *h_rbuf, size_t size);
RBUF_STATE_T ringbuf_reset(ringbuf_t *h_rbuf);

#endif
