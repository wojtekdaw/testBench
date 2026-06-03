/* 
 * CAN Standard test - can_test.c
 */
#include "can_test.h"
#include <stdio.h>

extern CAN_HandleTypeDef hcan1;

/* Globalne flagi testowe */
uint8_t g_test_ecu_relay = 0;
uint8_t g_test_ign_hw = 0;

/**
 * @brief Inicjalizacja CAN1 na PD0 (RX) i PD1 (TX)
 * Konfiguracja: 500 kbps (przy APB1 = 54 MHz)
 */
void CAN_Standard_Test_Init(void) {
    /* 1. Konfiguracja filtrów (odbieraj wszystko dla testu) */
    CAN_FilterTypeDef sFilterConfig;
    sFilterConfig.FilterBank = 0;
    sFilterConfig.FilterMode = CAN_FILTERMODE_IDMASK;
    sFilterConfig.FilterScale = CAN_FILTERSCALE_32BIT;
    sFilterConfig.FilterIdHigh = 0x0000;
    sFilterConfig.FilterIdLow = 0x0000;
    sFilterConfig.FilterMaskIdHigh = 0x0000;
    sFilterConfig.FilterMaskIdLow = 0x0000;
    sFilterConfig.FilterFIFOAssignment = CAN_RX_FIFO0;
    sFilterConfig.FilterActivation = ENABLE;
    sFilterConfig.SlaveStartFilterBank = 14;

    if (HAL_CAN_ConfigFilter(&hcan1, &sFilterConfig) != HAL_OK) {
        printf("CAN: Filter Error!\r\n");
        return;
    }

    /* 5. Start CAN */
    if (HAL_CAN_Start(&hcan1) != HAL_OK) {
        printf("CAN: Start Error!\r\n");
        return;
    }

    printf("CAN: Standard Test Initialized (500kbps, PD0/PD1)\r\n");
}

void CAN_Standard_Test_Send_CFG(uint8_t relay_state) {
    CAN_TxHeaderTypeDef TxHeader;
    uint8_t TxData[8] = {0};
    uint32_t TxMailbox;

    TxHeader.StdId = 0x68D;       /* Tester_CFG ID */
    TxHeader.RTR = CAN_RTR_DATA;
    TxHeader.IDE = CAN_ID_STD;
    TxHeader.DLC = 8;
    TxHeader.TransmitGlobalTime = DISABLE;

    /* Ustawiamy stan przekaźnika w Bajcie 0, Bit 0 (dla testu) */
    if (relay_state) {
        TxData[0] |= 0x01;
    } else {
        TxData[0] &= ~0x01;
    }

    if (HAL_CAN_AddTxMessage(&hcan1, &TxHeader, TxData, &TxMailbox) != HAL_OK) {
        printf("CAN: TX Error (0x68D)\r\n");
    } else {
        printf("CAN: Sent CFG (Relay=%d)\r\n", relay_state);
    }
}

void CAN_Standard_Test_Process_RX(void) {
    CAN_RxHeaderTypeDef RxHeader;
    uint8_t RxData[8];

    if (HAL_CAN_GetRxFifoFillLevel(&hcan1, CAN_RX_FIFO0) > 0) {
        if (HAL_CAN_GetRxMessage(&hcan1, CAN_RX_FIFO0, &RxHeader, RxData) == HAL_OK) {
            /* Jeśli ID = 0x123, ustaw IGN HW na podstawie Bitu 4 Bajtu 0 */
            if (RxHeader.StdId == 0x123) {
                g_test_ign_hw = (RxData[0] & 0x10) ? 1 : 0;
                printf("CAN: Received 0x123, IGN_HW set to %d\r\n", g_test_ign_hw);
            }
        }
    }
}
