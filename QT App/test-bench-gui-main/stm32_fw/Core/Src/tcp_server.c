/**
 * @file    tcp_server.c
 * @brief   LwIP RAW API TCP server – Test Bench Gateway (STM32 Nucleo-F767ZI)
 *
 * Architektura:
 *  - LwIP RAW API (wywołania z tcpip_thread FreeRTOS)
 *  - Jeden klient naraz (wystarczy na stanowisko testowe)
 *  - Bufor liniowy z memmove – bezpieczny dla ramek do PROTO_MAX_FRAME
 *  - Statyczne bufory odpowiedzi (RAW API → brak reentrancji)
 *
 * Zarządzanie pamięcią pbuf:
 *  - tcp_server_recv ZAWSZE wyczerpuje pbuf przez iterację łańcucha pbuf->next
 *  - tcp_recved() wywoływane raz z tot_len całego łańcucha
 *  - pbuf_free() wywoływane raz na każdy pbuf przekazany przez callback
 *  - Przy błędzie połączenia (p==NULL lub err!=ERR_OK): pbuf zwalniany jeśli != NULL,
 *    a tcb zwolniony przez tcp_close — NIE przez mem_free (LwIP robi to sam).
 */

#include "tcp_server.h"
#include "main.h"          /* HAL, LD1_GPIO_Port / LD1_Pin (CubeMX) */
#include "lwip/tcp.h"
#include "lwip/pbuf.h"
#include "lwip/mem.h"

#include <string.h>
#include <stdio.h>         /* snprintf (FW_VER) */

/* ── Wersja firmware ─────────────────────────────────────────────────────── */
#define FW_VER_MAJOR  1u
#define FW_VER_MINOR  0u
#define FW_VER_PATCH  0u

/* ── Stan połączenia per klient ──────────────────────────────────────────── */
typedef struct
{
    /* Akumulacyjny bufor odbiorczy – podwójny rozmiar ramki jako margines */
    uint8_t  rx_buf[PROTO_MAX_FRAME * 2u];
    uint16_t rx_len;

    /* Stan diody LED (synchronizowany z HAL_GPIO) */
    uint8_t  led_state;
} conn_state_t;

/* Statyczny bufor odpowiedzi (RAW API: brak równoległości wywołań) */
static uint8_t s_resp_buf[PROTO_MAX_FRAME];

/* ──────────────────────────────────────────────────────────────────────────
 * Budowanie ramki protokołu w s_resp_buf
 * Zwraca długość gotowej ramki (0 = błąd: payload za długi).
 *
 * CRC liczone nad: CMD(1B) + LEN_LO(1B) + LEN_HI(1B) + PAYLOAD(N)
 * Identycznie jak w protocol.py: crc_data = pack("<BH", cmd, length) + payload
 * ────────────────────────────────────────────────────────────────────────── */
static uint16_t build_frame(uint8_t cmd, const uint8_t *payload, uint16_t payload_len)
{
    if (payload_len > MAX_PAYLOAD_LEN)
    {
        return 0u;
    }

    /* Nagłówek */
    s_resp_buf[0] = PROTO_STX;
    s_resp_buf[1] = cmd;
    s_resp_buf[2] = (uint8_t)(payload_len & 0xFFu);
    s_resp_buf[3] = (uint8_t)(payload_len >> 8u);

    /* Payload */
    if (payload_len > 0u && payload != NULL)
    {
        memcpy(&s_resp_buf[4], payload, payload_len);
    }

    /* CRC – liczymy inline przez cały zakres CMD+LEN+PAYLOAD bez VLA */
    uint16_t crc = 0xFFFFu;
    const uint8_t crc_header[3] = { cmd, s_resp_buf[2], s_resp_buf[3] };
    for (uint8_t i = 0u; i < 3u; i++)
    {
        crc ^= (uint16_t)crc_header[i] << 8u;
        for (uint8_t j = 0u; j < 8u; j++)
            crc = (crc & 0x8000u) ? ((crc << 1u) ^ 0x1021u) : (crc << 1u);
    }
    for (uint16_t i = 0u; i < payload_len; i++)
    {
        crc ^= (uint16_t)payload[i] << 8u;
        for (uint8_t j = 0u; j < 8u; j++)
            crc = (crc & 0x8000u) ? ((crc << 1u) ^ 0x1021u) : (crc << 1u);
    }

    /* Trailer */
    uint16_t trailer_offset = PROTO_HEADER_SIZE + payload_len;
    s_resp_buf[trailer_offset]      = (uint8_t)(crc & 0xFFu);
    s_resp_buf[trailer_offset + 1u] = (uint8_t)(crc >> 8u);
    s_resp_buf[trailer_offset + 2u] = PROTO_ETX;

    return (uint16_t)(PROTO_HEADER_SIZE + payload_len + PROTO_TRAILER_SIZE);
}

/* ──────────────────────────────────────────────────────────────────────────
 * Wysyłanie odpowiedzi przez LwIP
 * tcp_write() kopiuje dane (TCP_WRITE_FLAG_COPY) – s_resp_buf może być statyczny.
 * ────────────────────────────────────────────────────────────────────────── */
static void send_response(struct tcp_pcb *tpcb, uint8_t cmd,
                          const uint8_t *payload, uint16_t payload_len)
{
    uint16_t frame_len = build_frame(cmd, payload, payload_len);
    if (frame_len == 0u) return;

    /* Sprawdź czy jest miejsce w TCP send buffer */
    if (tcp_sndbuf(tpcb) < frame_len) return;

    err_t err = tcp_write(tpcb, s_resp_buf, frame_len, TCP_WRITE_FLAG_COPY);
    if (err == ERR_OK)
    {
        tcp_output(tpcb);
    }
}

/* ──────────────────────────────────────────────────────────────────────────
 * Logika komend – tu dodawaj nowe komendy
 * ────────────────────────────────────────────────────────────────────────── */
static void process_command(struct tcp_pcb *tpcb, conn_state_t *cs,
                            uint8_t cmd, const uint8_t *payload, uint16_t payload_len)
{
    (void)payload_len; /* używane przez ECHO i future commands */

    switch (cmd)
    {
        /* ── PING → PONG (brak payloadu) ────────────────────────────────── */
        case CMD_PING:
            send_response(tpcb, CMD_PONG, NULL, 0u);
            break;

        /* ── ECHO_REQ → ECHO_RSP (ten sam payload) ───────────────────────── */
        case CMD_ECHO_REQ:
            send_response(tpcb, CMD_ECHO_RSP, payload, payload_len);
            break;

        /* ── LED ON ─────────────────────────────────────────────────────── */
        case CMD_LED_ON:
            HAL_GPIO_WritePin(LD1_GPIO_Port, LD1_Pin, GPIO_PIN_SET);
            cs->led_state = 1u;
            send_response(tpcb, CMD_LED_ACK, &cs->led_state, 1u);
            break;

        /* ── LED OFF ────────────────────────────────────────────────────── */
        case CMD_LED_OFF:
            HAL_GPIO_WritePin(LD1_GPIO_Port, LD1_Pin, GPIO_PIN_RESET);
            cs->led_state = 0u;
            send_response(tpcb, CMD_LED_ACK, &cs->led_state, 1u);
            break;

        /* ── LED TOGGLE ─────────────────────────────────────────────────── */
        case CMD_LED_TOGGLE:
            HAL_GPIO_TogglePin(LD1_GPIO_Port, LD1_Pin);
            cs->led_state = (HAL_GPIO_ReadPin(LD1_GPIO_Port, LD1_Pin) == GPIO_PIN_SET)
                            ? 1u : 0u;
            send_response(tpcb, CMD_LED_ACK, &cs->led_state, 1u);
            break;

        /* ── GET_STATUS → STATUS_RSP (1B flagi) ─────────────────────────── */
        case CMD_GET_STATUS:
        {
            uint8_t flags = STATUS_ETH_LINK_UP; /* połączenie TCP aktywne = ETH OK */
            if (cs->led_state) flags |= STATUS_LED_STATE;
            /* CAN_BUS_OK, TEENSY_ALIVE – uzupełnij gdy dostępne */
            send_response(tpcb, CMD_STATUS_RSP, &flags, 1u);
            break;
        }

        /* ── GET_FW_VER → FW_VER_RSP (3B: major, minor, patch) ─────────── */
        case CMD_GET_FW_VER:
        {
            uint8_t ver[3] = {FW_VER_MAJOR, FW_VER_MINOR, FW_VER_PATCH};
            send_response(tpcb, CMD_FW_VER_RSP, ver, 3u);
            break;
        }

        /* ── Nieznana komenda → NACK (1B: echo nieznanego CMD) ───────────── */
        default:
        {
            uint8_t nack_info = cmd;
            send_response(tpcb, CMD_NACK, &nack_info, 1u);
            break;
        }
    }
}

/* ──────────────────────────────────────────────────────────────────────────
 * Parser ramek z rx_buf
 *
 * Algorytm:
 *  1. Szukaj STX, odrzuć śmieci przed nim.
 *  2. Czekaj na kompletny nagłówek (4B).
 *  3. Odczytaj LEN, wylicz oczekiwaną długość ramki.
 *  4. Waliduj ETX i CRC.
 *  5. Przetwarzaj komendę, usuń ramkę z bufora (memmove).
 *  6. Powtarzaj dopóki są dane.
 * ────────────────────────────────────────────────────────────────────────── */
static void parse_rx_buffer(struct tcp_pcb *tpcb, conn_state_t *cs)
{
    uint8_t  *buf       = cs->rx_buf;
    uint16_t  available = cs->rx_len;

    while (available >= PROTO_MIN_FRAME)
    {
        /* Krok 1: Znajdź STX */
        uint16_t stx_pos = 0u;
        while (stx_pos < available && buf[stx_pos] != PROTO_STX)
        {
            stx_pos++;
        }

        if (stx_pos >= available)
        {
            /* Brak STX – wyczyść cały bufor */
            cs->rx_len = 0u;
            return;
        }

        if (stx_pos > 0u)
        {
            /* Odrzuć bajty przed STX */
            memmove(buf, buf + stx_pos, available - stx_pos);
            available -= stx_pos;
            cs->rx_len = available;
        }

        /* Krok 2: Potrzebujemy przynajmniej nagłówka */
        if (available < PROTO_HEADER_SIZE) break;

        /* Krok 3: Odczytaj LEN (LE) */
        uint16_t payload_len = (uint16_t)buf[2] | ((uint16_t)buf[3] << 8u);

        if (payload_len > MAX_PAYLOAD_LEN)
        {
            /* Nieprawidłowy LEN – przesuń o 1 (pomiń ten STX) */
            memmove(buf, buf + 1u, available - 1u);
            available--;
            cs->rx_len = available;
            continue;
        }

        uint16_t frame_len = PROTO_HEADER_SIZE + payload_len + PROTO_TRAILER_SIZE;

        /* Krok 4a: Czekaj na kompletną ramkę */
        if (available < frame_len) break;

        /* Krok 4b: Waliduj ETX */
        if (buf[frame_len - 1u] != PROTO_ETX)
        {
            memmove(buf, buf + 1u, available - 1u);
            available--;
            cs->rx_len = available;
            continue;
        }

        /* Krok 4c: Waliduj CRC-16/CCITT nad CMD+LEN+PAYLOAD */
        {
            /* Liczymy CRC inline, bez VLA */
            uint16_t crc = 0xFFFFu;
            /* CMD */
            uint16_t b = (uint16_t)buf[1];
            crc ^= b << 8u;
            for (uint8_t j = 0u; j < 8u; j++)
                crc = (crc & 0x8000u) ? ((crc << 1u) ^ 0x1021u) : (crc << 1u);
            /* LEN_LO */
            b = (uint16_t)buf[2];
            crc ^= b << 8u;
            for (uint8_t j = 0u; j < 8u; j++)
                crc = (crc & 0x8000u) ? ((crc << 1u) ^ 0x1021u) : (crc << 1u);
            /* LEN_HI */
            b = (uint16_t)buf[3];
            crc ^= b << 8u;
            for (uint8_t j = 0u; j < 8u; j++)
                crc = (crc & 0x8000u) ? ((crc << 1u) ^ 0x1021u) : (crc << 1u);
            /* PAYLOAD */
            for (uint16_t i = 0u; i < payload_len; i++)
            {
                b = (uint16_t)buf[PROTO_HEADER_SIZE + i];
                crc ^= b << 8u;
                for (uint8_t j = 0u; j < 8u; j++)
                    crc = (crc & 0x8000u) ? ((crc << 1u) ^ 0x1021u) : (crc << 1u);
            }

            uint16_t crc_recv = (uint16_t)buf[PROTO_HEADER_SIZE + payload_len]
                              | ((uint16_t)buf[PROTO_HEADER_SIZE + payload_len + 1u] << 8u);

            if (crc != crc_recv)
            {
                /* Błąd CRC – odrzuć całą ramkę */
                memmove(buf, buf + frame_len, available - frame_len);
                available -= frame_len;
                cs->rx_len = available;
                continue;
            }
        }

        /* Krok 5: Poprawna ramka – przetwarzaj */
        uint8_t cmd = buf[1];
        const uint8_t *payload = (payload_len > 0u) ? &buf[PROTO_HEADER_SIZE] : NULL;
        process_command(tpcb, cs, cmd, payload, payload_len);

        /* Usuń ramkę z bufora */
        memmove(buf, buf + frame_len, available - frame_len);
        available -= frame_len;
        cs->rx_len = available;
    }
}

/* ──────────────────────────────────────────────────────────────────────────
 * LwIP RAW API – callbacki
 * ────────────────────────────────────────────────────────────────────────── */

/**
 * @brief  Callback wywoływany przez LwIP przy odebraniu danych TCP.
 *
 * ZARZĄDZANIE PBUF:
 *  - p == NULL oznacza zamknięcie połączenia przez drugą stronę (FIN)
 *  - Iterujemy po łańcuchu pbuf (q = p; q != NULL; q = q->next)
 *  - tcp_recved() raz z p->tot_len informuje LwIP o odczytaniu okna
 *  - pbuf_free(p) zwalnia cały łańcuch (LwIP liczy ref)
 *  - NIE wywołujemy pbuf_free(q) w pętli – to spowodowałoby double-free!
 */
static err_t tcp_server_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err)
{
    conn_state_t *cs = (conn_state_t *)arg;

    /* Połączenie zamknięte przez klienta lub błąd */
    if (p == NULL || err != ERR_OK)
    {
        if (p != NULL)
        {
            tcp_recved(tpcb, p->tot_len);
            pbuf_free(p);
        }
        tcp_arg(tpcb, NULL);
        tcp_recv(tpcb, NULL);
        tcp_err(tpcb, NULL);
        tcp_close(tpcb);
        mem_free(cs);
        return ERR_OK;
    }

    /* Kopiuj dane z łańcucha pbuf do rx_buf */
    struct pbuf *q = p;
    while (q != NULL)
    {
        uint16_t space = (uint16_t)(sizeof(cs->rx_buf)) - cs->rx_len;
        uint16_t to_copy = (q->len <= space) ? (uint16_t)q->len : space;

        if (to_copy > 0u)
        {
            memcpy(&cs->rx_buf[cs->rx_len], q->payload, to_copy);
            cs->rx_len += to_copy;
        }
        /* Jeśli bufor pełny – reszta odrzucona (nie ma to prawa się zdarzyć
           przy normalnym ruchu, bo MAX_FRAME*2 >> pojedynczy segment TCP) */

        q = q->next;
    }

    /* Informuj LwIP o odczytanych bajtach (przesuwa okno TCP) */
    tcp_recved(tpcb, p->tot_len);

    /* Zwolnij cały łańcuch pbuf – JEDEN pbuf_free wystarczy */
    pbuf_free(p);

    /* Parsuj ramki z rx_buf */
    parse_rx_buffer(tpcb, cs);

    return ERR_OK;
}

/**
 * @brief  Callback błędu (np. RST od klienta, timeout).
 *
 * UWAGA: Gdy wywoływany jest tcp_err, tpcb jest JUŻ zwolniony przez LwIP.
 *        NIE wywołuj tcp_close(tpcb) – spowoduje to double-free!
 */
static void tcp_server_err(void *arg, err_t err)
{
    (void)err;
    conn_state_t *cs = (conn_state_t *)arg;
    if (cs != NULL)
    {
        mem_free(cs);
    }
    /* tpcb już zwolniony przez LwIP – nic nie robimy */
}

/**
 * @brief  Callback akceptacji nowego połączenia TCP.
 */
static err_t tcp_server_accept(void *arg, struct tcp_pcb *newpcb, err_t err)
{
    (void)arg;

    if (err != ERR_OK || newpcb == NULL)
    {
        return ERR_VAL;
    }

    /* Alokacja stanu połączenia z puli LwIP (nie z systemu alokacji C!) */
    conn_state_t *cs = (conn_state_t *)mem_malloc(sizeof(conn_state_t));
    if (cs == NULL)
    {
        tcp_close(newpcb);
        return ERR_MEM;
    }
    memset(cs, 0, sizeof(conn_state_t));

    /* Priorytet minimalny (serwer diagnostyczny) */
    tcp_setprio(newpcb, TCP_PRIO_MIN);

    /* Rejestracja callbacków */
    tcp_arg(newpcb, cs);
    tcp_recv(newpcb, tcp_server_recv);
    tcp_err(newpcb, tcp_server_err);
    /* tcp_poll – opcjonalny, np. do keep-alive lub flush */

    return ERR_OK;
}

/* ──────────────────────────────────────────────────────────────────────────
 * Publiczna inicjalizacja serwera
 * Wywołaj z wątku FreeRTOS PO MX_LWIP_Init().
 * ────────────────────────────────────────────────────────────────────────── */
err_t tcp_server_init(void)
{
    struct tcp_pcb *pcb = tcp_new();
    if (pcb == NULL)
    {
        return ERR_MEM;
    }

    err_t err = tcp_bind(pcb, IP_ADDR_ANY, TCP_SERVER_PORT);
    if (err != ERR_OK)
    {
        tcp_close(pcb);
        return err;
    }

    struct tcp_pcb *listen_pcb = tcp_listen(pcb);
    if (listen_pcb == NULL)
    {
        /* tcp_listen() zwraca NULL i zwalnia pcb gdy brak pamięci */
        return ERR_MEM;
    }

    tcp_accept(listen_pcb, tcp_server_accept);

    return ERR_OK;
}
