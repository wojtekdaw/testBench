/**
 * @file    tcp_server.h
 * @brief   LwIP RAW API TCP server – Test Bench Gateway (STM32 Nucleo-F767ZI)
 *
 * Protokół binarny:
 *   [STX:1B=0x02][CMD:1B][LEN:2B LE][PAYLOAD:0..1024B][CRC16:2B LE][ETX:1B=0x03]
 *   CRC-16/CCITT (poly 0x1021, init 0xFFFF) liczone nad: CMD + LEN + PAYLOAD
 */

#ifndef TCP_SERVER_H
#define TCP_SERVER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lwip/err.h"
#include <stdint.h>

/* ── Konfiguracja serwera ─────────────────────────────────────────────────── */
#define TCP_SERVER_PORT      5000u
#define MAX_PAYLOAD_LEN      1024u

/* ── Stałe protokołu ─────────────────────────────────────────────────────── */
#define PROTO_STX            0x02u
#define PROTO_ETX            0x03u
#define PROTO_HEADER_SIZE    4u    /* STX + CMD + LEN_LO + LEN_HI */
#define PROTO_TRAILER_SIZE   3u    /* CRC_LO + CRC_HI + ETX       */
#define PROTO_MIN_FRAME      7u    /* ramka bez payloadu           */
#define PROTO_MAX_FRAME      (PROTO_HEADER_SIZE + MAX_PAYLOAD_LEN + PROTO_TRAILER_SIZE)

/* ── Identyfikatory komend ───────────────────────────────────────────────── */
#define CMD_PING             0x01u
#define CMD_PONG             0x02u
#define CMD_ECHO_REQ         0x03u
#define CMD_ECHO_RSP         0x04u
#define CMD_LED_ON           0x05u
#define CMD_LED_OFF          0x06u
#define CMD_LED_TOGGLE       0x07u
#define CMD_LED_ACK          0x08u
#define CMD_GET_STATUS       0x09u
#define CMD_STATUS_RSP       0x0Au
#define CMD_GET_FW_VER       0x0Bu
#define CMD_FW_VER_RSP       0x0Cu
#define CMD_NACK             0xFFu

/* ── Flagi statusu (payload STATUS_RSP, 1B) ──────────────────────────────── */
#define STATUS_ETH_LINK_UP   0x01u
#define STATUS_CAN_BUS_OK    0x02u
#define STATUS_TEENSY_ALIVE  0x04u
#define STATUS_LED_STATE     0x08u

/* ── Publiczne API ────────────────────────────────────────────────────────── */

/**
 * @brief  Inicjalizuje serwer TCP na porcie TCP_SERVER_PORT.
 *         Wywołaj po MX_LWIP_Init() z poziomu wątku FreeRTOS.
 * @retval ERR_OK   – sukces
 * @retval ERR_MEM  – brak pamięci LwIP
 * @retval inne     – błąd bind/listen
 */
err_t tcp_server_init(void);

#ifdef __cplusplus
}
#endif

#endif /* TCP_SERVER_H */
