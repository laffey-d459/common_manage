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
#include "serial.h"
#include <string.h>

/// @brief 串行通信初始化(使用环形缓存区)
/// @param h_srl 串行通信句柄
/// @param h_srl_initstruct 串行通信初始化结构体
/// @retval SRL_OK 初始化成功
/// @retval SRL_INVALID_VALUE 无效的参数
SRL_STATE_T srl_init(srl_t *h_srl, srl_init_t *h_srl_initstruct)
{
    if (h_srl_initstruct == 0 || h_srl == 0 ||
        h_srl_initstruct->func_rece == 0 || h_srl_initstruct->func_send == 0 ||
        h_srl_initstruct->hrb_rx == 0 || h_srl_initstruct->hrb_tx == 0 ||
        h_srl_initstruct->rb_rx_buf == 0 || h_srl_initstruct->rb_tx_buf == 0 ||
        h_srl_initstruct->rb_rx_buf_size == 0 || h_srl_initstruct->rb_tx_buf_size == 0)
    {
        return SRL_INVALID_VALUE;
    }

    h_srl->handle_state = HSRL_NO_INIT;

    h_srl->is_tx_busy = 0;
    h_srl->tx_data_addr = 0;
    h_srl->tx_rema_size = 0;

    h_srl->hrb_tx = h_srl_initstruct->hrb_tx;
    h_srl->hrb_rx = h_srl_initstruct->hrb_rx;

    if (ringbuf_init(h_srl->hrb_tx, h_srl_initstruct->rb_tx_buf, h_srl_initstruct->rb_tx_buf_size) != RBUF_OK)
    {
        return SRL_INVALID_VALUE;
    }
    if (ringbuf_init(h_srl->hrb_rx, h_srl_initstruct->rb_rx_buf, h_srl_initstruct->rb_rx_buf_size) != RBUF_OK)
    {
        return SRL_INVALID_VALUE;
    }

    h_srl->func_send_data = h_srl_initstruct->func_send;
    h_srl->func_rece_data = h_srl_initstruct->func_rece;

    h_srl->handle_state = HSRL_OK;

    return SRL_OK;
}

/// @brief 启动消息发送
/// @param h_srl 串行通信句柄
/// @retval SRL_OK 启动成功
/// @retval SRL_INVALID_VALUE 无效的参数
/// @retval SRL_HANDLE_UNUSABLE 句柄不可用
/// @retval SRL_BUSY 正在发送
/// @retval SRL_NOT_ENOUGH 没有要发送的消息
/// @retval SRL_MEM_ERROR 借用环形缓存区内存失败
static inline SRL_STATE_T srl_start_transmit(srl_t *h_srl)
{
    if (h_srl == 0)
    {
        return SRL_INVALID_VALUE;
    }

    if (h_srl->handle_state == HSRL_NO_INIT)
    {
        return SRL_HANDLE_UNUSABLE;
    }

    if (h_srl->is_tx_busy == 1)
    {
        return SRL_BUSY;
    }

    h_srl->is_tx_busy = 1;
    size_t act_size = ringbuf_check_borrow_size(h_srl->hrb_tx);
    if (act_size != 0)
    {
        ringbuf_reback(h_srl->hrb_tx, &h_srl->tx_data_addr, act_size);
    }

    uint16_t mes_len = 0;
    if (h_srl->is_data_cut == 1)
    {
        mes_len = h_srl->tx_rema_size;
    }
    else
    {
        if (ringbuf_read(h_srl->hrb_tx, (uint8_t *)&mes_len, sizeof(mes_len)) != RBUF_OK)
        {
            h_srl->is_tx_busy = 0;
            return SRL_NOT_ENOUGH;
        }
    }

    if (mes_len == 0)
    {
        h_srl->is_tx_busy = 0;
        return SRL_NOT_ENOUGH;
    }

    act_size = ringbuf_borrow(h_srl->hrb_tx, &h_srl->tx_data_addr, (uint8_t *)&h_srl->is_data_cut, mes_len);
    if (act_size == 0 || h_srl->tx_data_addr == 0)
    {
        h_srl->is_tx_busy = 0;
        return SRL_MEM_ERROR;
    }

    if (h_srl->is_data_cut)
    {
        h_srl->tx_rema_size = mes_len - act_size;
    }

    h_srl->func_send_data(h_srl->tx_data_addr, act_size);
    return SRL_OK;
}

/// @brief 串行通信发送数据
/// @param h_srl 串行通信句柄
/// @param buf 数据缓存区
/// @param size 数据字节长度（最大是环形缓存区大小-2）
/// @retval SRL_OK 数据成功推入发送流程
/// @retval SRL_INVALID_VALUE 无效的参数
/// @retval SRL_HANDLE_UNUSABLE 句柄不可用
/// @retval SRL_NOT_ENOUGH 剩余空间不足
/// @retval SRL_MEM_ERROR 写入数据失败
SRL_STATE_T srl_send_data(srl_t *h_srl, uint8_t *buf, uint16_t size)
{
    if (h_srl == 0 || buf == 0 || size + 2 > h_srl->hrb_tx->size)
    {
        return SRL_INVALID_VALUE;
    }

    if (h_srl->handle_state == HSRL_NO_INIT)
    {
        return SRL_HANDLE_UNUSABLE;
    }

    if (size == 0)
    {
        return SRL_OK;
    }

    if (ringbuf_get_space_size(h_srl->hrb_tx) < 2 + size)
    {
        return SRL_NOT_ENOUGH;
    }

    if (ringbuf_write(h_srl->hrb_tx, &size, 2) != RBUF_OK)
    {
        return SRL_MEM_ERROR;
    }
    if (ringbuf_write(h_srl->hrb_tx, buf, size) != RBUF_OK)
    {
        uint16_t t_rubbish = 0;
        ringbuf_read(h_srl->hrb_tx, &t_rubbish, 2);
        return SRL_MEM_ERROR;
    }

    if (h_srl->is_tx_busy == 1)
    {
        return SRL_BUSY;
    }

    srl_start_transmit(h_srl);
    return SRL_OK;
}

/// @brief 串行通信接收数据
/// @param h_srl 串行通信句柄
/// @param buf 数据缓存区
/// @param size 数据字节长度（最大是环形缓存区大小-2）
/// @retval SRL_OK 接收成功
/// @retval SRL_INVALID_VALUE 无效的参数
/// @retval SRL_HANDLE_UNUSABLE 句柄不可用
/// @retval SRL_MEM_ERROR 读取数据失败
SRL_STATE_T srl_rece_data(srl_t *h_srl, uint8_t *buf, uint16_t size)
{
    if (h_srl == 0 || buf == 0 || size + 2 > h_srl->hrb_rx->size)
    {
        return SRL_INVALID_VALUE;
    }

    if (h_srl->handle_state == HSRL_NO_INIT)
    {
        return SRL_HANDLE_UNUSABLE;
    }

    if (size == 0)
    {
        return SRL_OK;
    }

    if (ringbuf_read(h_srl->hrb_rx, buf, size) != RBUF_OK)
    {
        return SRL_MEM_ERROR;
    }
    return SRL_OK;
}

/// @brief 发送完成回调函数
/// @param h_srl 串行通信句柄
/// @return SRL_OK 操作成功
/// @retval SRL_INVALID_VALUE 无效的参数
/// @retval SRL_HANDLE_UNUSABLE 句柄不可用
/// @note 需发送完后调用
SRL_STATE_T srl_send_data_callback(srl_t *h_srl)
{
    if (h_srl == 0)
    {
        return SRL_INVALID_VALUE;
    }

    if (h_srl->handle_state == HSRL_NO_INIT)
    {
        return SRL_HANDLE_UNUSABLE;
    }

    if (h_srl->is_tx_busy == 1)
    {
        h_srl->is_tx_busy = 0;
        srl_start_transmit(h_srl);
    }

    return SRL_OK;
}

/// @brief 接收完成回调函数
/// @param h_srl 串行通信句柄
/// @return SRL_OK 操作成功
/// @retval SRL_INVALID_VALUE 无效的参数
/// @retval SRL_HANDLE_UNUSABLE 句柄不可用
/// @note 需要接收完后调用
SRL_STATE_T srl_rece_data_callback(srl_t *h_srl)
{
    if (h_srl == 0)
    {
        return SRL_INVALID_VALUE;
    }

    if (h_srl->handle_state == HSRL_NO_INIT)
    {
        return SRL_HANDLE_UNUSABLE;
    }

    uint8_t is_cover = 0;
    size_t get_size = h_srl->func_rece_data(0, 0);
    if (get_size > 0)
    {
        ringbuf_submit(h_srl->hrb_rx, &is_cover, get_size);
    }

    return SRL_OK;
}
