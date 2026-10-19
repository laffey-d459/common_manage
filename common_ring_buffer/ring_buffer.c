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
#include "ring_buffer.h"

/// @brief 环形缓存区初始化
/// @param h_rbuf 环形缓存句柄
/// @param buf 数据缓存区
/// @param size 数据缓存区字节大小
/// @retval RBUF_OK 初始化完成
/// @retval RBUF_INVALID_VALUE 传入的参数无效
RBUF_STATE_T ringbuf_init(ringbuf_t *h_rbuf, void *buf, size_t size)
{
    if (h_rbuf == 0 || buf == 0 || size == 0)
    {
        return RBUF_INVALID_VALUE;
    }

    h_rbuf->buf = buf;
    h_rbuf->size = size;
    h_rbuf->borrowed_size = 0;
    h_rbuf->tail = 0;
    h_rbuf->head = 0;

    return RBUF_OK;
}

/// @brief 写入数据到环形缓存区
/// @param h_rbuf 环形缓存句柄
/// @param buf 数据缓存区
/// @param size 写入数据的字节大小
/// @retval RBUF_OK 写入成功
/// @retval RBUF_INVALID_VALUE 传入的参数无效
/// @retval RBUF_NOT_ENOUGH 剩余空间不足
/// @note 空间不足时，不会写入数据
RBUF_STATE_T ringbuf_write(ringbuf_t *h_rbuf, void *buf, size_t size)
{
    if (h_rbuf == 0 || buf == 0 || size >= h_rbuf->size)
    {
        return RBUF_INVALID_VALUE;
    }
    if (size == 0)
    {
        return RBUF_OK;
    }

    if (size > ringbuf_get_space_size(h_rbuf))
    {
        return RBUF_NOT_ENOUGH;
    }

    size_t space_to_end = h_rbuf->size - h_rbuf->head;
    if (size <= space_to_end)
    {
        memcpy((uint8_t *)h_rbuf->buf + h_rbuf->head, buf, size);
    }
    else
    {
        memcpy((uint8_t *)h_rbuf->buf + h_rbuf->head, buf, space_to_end);
        memcpy(h_rbuf->buf, (uint8_t *)buf + space_to_end, size - space_to_end);
    }
    h_rbuf->head = (h_rbuf->head + size) % h_rbuf->size;

    return RBUF_OK;
}

/// @brief 从环形缓存区读取数据
/// @param h_rbuf 环形缓存句柄
/// @param buf 数据缓存区
/// @param size 读取数据的字节大小
/// @retval RBUF_OK 读取成功
/// @retval RBUF_INVALID_VALUE 传入的参数无效
/// @retval RBUF_NOT_ENOUGH 存有数据不足
/// @note 数据不足时，不会读取数据
/// @note 借用后,在归还前无法读取
RBUF_STATE_T ringbuf_read(ringbuf_t *h_rbuf, void *buf, size_t size)
{
    if (h_rbuf == 0 || buf == 0 || size >= h_rbuf->size || h_rbuf->borrowed_size != 0)
    {
        return RBUF_INVALID_VALUE;
    }
    if (size == 0)
    {
        return RBUF_OK;
    }

    if (size > ringbuf_get_stored_size(h_rbuf))
    {
        return RBUF_NOT_ENOUGH;
    }

    size_t space_to_end = h_rbuf->size - h_rbuf->tail;
    if (size <= space_to_end)
    {
        memcpy(buf, (uint8_t *)h_rbuf->buf + h_rbuf->tail, size);
    }
    else
    {
        memcpy(buf, (uint8_t *)h_rbuf->buf + h_rbuf->tail, space_to_end);
        memcpy((uint8_t *)buf + space_to_end, h_rbuf->buf, size - space_to_end);
    }
    h_rbuf->tail = (h_rbuf->tail + size) % h_rbuf->size;

    return RBUF_OK;
}

/// @brief 提交数据信息（早已通过dma等方式写入数据）
/// @param h_rbuf 环形缓存句柄
/// @param is_cover 新数据是否覆盖旧数据（0：未覆盖；1：已覆盖）
/// @param size 写入数据的字节大小
/// @retval RBUF_OK 提交成功
/// @retval RBUF_INVALID_VALUE 传入的参数无效
/// @note 只是通知，修改早已做出，需自行留意覆盖问题。
/// @note 提交后若tail超过head（即新数据覆盖了旧数据）,
///       会导致缓存区只认新的head到tail间的数据（即认为只存有覆盖后的数据那段数据）
RBUF_STATE_T ringbuf_submit(ringbuf_t *h_rbuf, uint8_t *is_cover, size_t size)
{
    if (h_rbuf == 0 || is_cover == 0 || size >= h_rbuf->size)
    {
        return RBUF_INVALID_VALUE;
    }
    if (size == 0)
    {
        return RBUF_OK;
    }

    *is_cover = (size > ringbuf_get_space_size(h_rbuf)) ? 1 : 0;
    h_rbuf->head = (h_rbuf->head + size) % h_rbuf->size;

    return RBUF_OK;
}

/// @brief 读取数据,并借用内存
/// @param h_rbuf 环形缓存句柄
/// @param addr 输出借用的内存的地址
/// @param is_cut 申请借用的内存是否被截断（即遇到环形缓存区的末尾）（0非，1是）
/// @param size 读取数据（借用内存）的字节大小
/// @return 实际读取数据（借用内存）的字节大小
/// @note 归还前无法读取数据或再次借用
/// @note 申请借用的区域被截断后，仅返回前半段的大小和地址
size_t ringbuf_borrow(ringbuf_t *h_rbuf, void **addr, uint8_t *is_cut, size_t size)
{
    if (h_rbuf == 0 || addr == 0 || is_cut == 0 || size == 0 || size >= h_rbuf->size || h_rbuf->borrowed_size != 0)
    {
        if (is_cut != 0)
            *is_cut = 0;
        if (addr != 0)
            *addr = 0;
        return 0;
    }

    size_t avai_size = ringbuf_get_stored_size(h_rbuf);
    if (avai_size == 0)
    {
        *is_cut = 0;
        *addr = 0;
        return 0;
    }

    size_t act_size;
    size_t space_to_end = h_rbuf->size - h_rbuf->tail;

    if (avai_size > space_to_end)
    {
        *is_cut = 1;
        act_size = space_to_end;
    }
    else
    {
        *is_cut = 0;
        act_size = (size > avai_size) ? avai_size : size;
    }

    h_rbuf->borrowed_size = act_size;
    *addr = (uint8_t *)h_rbuf->buf + h_rbuf->tail;

    return act_size;
}

/// @brief 归还借用环形缓存区的内存
/// @param h_rbuf 环形缓存句柄
/// @param addr 借用内存的地址（如果是部分归还会输出新的地址）
/// @param size 归还的内存的字节大小
/// @retval RBUF_OK 归还成功
/// @retval RBUF_INVALID_VALUE 传入的参数无效
/// @note 支持不完全归还
RBUF_STATE_T ringbuf_reback(ringbuf_t *h_rbuf, void **addr, size_t size)
{
    if (h_rbuf == 0 || addr == 0 || *addr != (uint8_t *)h_rbuf->buf + h_rbuf->tail || size > h_rbuf->borrowed_size)
    {
        return RBUF_INVALID_VALUE;
    }

    if (size == 0)
    {
        return RBUF_OK;
    }

    h_rbuf->borrowed_size -= size;
    h_rbuf->tail = (h_rbuf->tail + size) % h_rbuf->size;
    *addr = (uint8_t *)h_rbuf->buf + h_rbuf->tail;

    return RBUF_OK;
}

/// @brief 查看当前借用的内存的大小
/// @param h_rbuf 环形缓存句柄
/// @return 当前借用的内存字节大小
size_t ringbuf_check_borrow_size(ringbuf_t *h_rbuf)
{
    if (h_rbuf == 0)
    {
        return 0;
    }

    return h_rbuf->borrowed_size;
}

/// @brief 计算环形缓存区当前已存数据字节大小
/// @param h_rbuf 环形缓存句柄
/// @return 已存数据的字节大小
size_t ringbuf_get_stored_size(const ringbuf_t *h_rbuf)
{
    if (h_rbuf == 0 || h_rbuf->size == 0)
    {
        return 0;
    }

    return (h_rbuf->head - h_rbuf->tail) % h_rbuf->size;
}

/// @brief 计算环形缓存区剩余空间
/// @param h_rbuf 环形缓存句柄
/// @return 剩余空间的字节大小
size_t ringbuf_get_space_size(const ringbuf_t *h_rbuf)
{
    if (h_rbuf == 0)
    {
        return 0;
    }

    return h_rbuf->size - ringbuf_get_stored_size(h_rbuf) - 1;
}

/// @brief 判断环形缓存区是否为空
/// @param h_rbuf 环形缓存句柄
/// @retval 1 已空
/// @retval 0 非空
uint8_t ringbuf_is_empty(const ringbuf_t *h_rbuf)
{
    if (h_rbuf == 0)
    {
        return 1;
    }

    return (h_rbuf->head == h_rbuf->tail) ? 1 : 0;
}

/// @brief 判断环形缓存区是否为满
/// @param h_rbuf 环形缓存句柄
/// @retval 1 已满
/// @retval 0 非满
uint8_t ringbuf_is_full(const ringbuf_t *h_rbuf)
{
    if (h_rbuf == 0 || h_rbuf->size == 0)
    {
        return 1;
    }

    return (((h_rbuf->head + 1) % h_rbuf->size) == h_rbuf->tail) ? 1 : 0;
}

/// @brief 清除指定长的数据
/// @param h_rbuf 环形缓存句柄
/// @param size 清理的数据字节大小
/// @retval RBUF_OK 清除完成
/// @retval RBUF_INVALID_VALUE 传入的参数无效
/// @retval RBUF_NOT_ENOUGH 无足够可清除数据
RBUF_STATE_T ringbuf_clear(ringbuf_t *h_rbuf, size_t size)
{
    if (h_rbuf == 0 || h_rbuf->buf == 0 || h_rbuf->size == 0)
    {
        return RBUF_INVALID_VALUE;
    }

    if (size > ringbuf_get_stored_size(h_rbuf))
    {
        return RBUF_NOT_ENOUGH;
    }

    h_rbuf->tail = (h_rbuf->tail + size) % h_rbuf->size;

    return RBUF_OK;
}

/// @brief 清空环形缓存区数据
/// @param h_rbuf 环形缓存句柄
/// @retval RBUF_OK 清除完成
/// @retval RBUF_INVALID_VALUE 传入的参数无效
RBUF_STATE_T ringbuf_reset(ringbuf_t *h_rbuf)
{
    if (h_rbuf == 0)
    {
        return RBUF_INVALID_VALUE;
    }

    h_rbuf->head = 0;
    h_rbuf->tail = 0;
    h_rbuf->borrowed_size = 0;

    return RBUF_OK;
}
