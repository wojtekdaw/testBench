/* 
 * CAN Standard test - can_test.h
 */
#ifndef INC_CAN_TEST_H_
#define INC_CAN_TEST_H_

#include "stm32f7xx_hal.h"
#include <stdbool.h>

/* Globalne flagi testowe */
extern uint8_t g_test_ecu_relay;
extern uint8_t g_test_ign_hw;

/* Inicjalizacja CAN1 na PD0/PD1 */
void CAN_Standard_Test_Init(void);

/* Wysłanie ramki testowej na podstawie stanu ECU Relay */
void CAN_Standard_Test_Send_CFG(uint8_t relay_state);

/* Przetwarzanie odebranych ramek */
void CAN_Standard_Test_Process_RX(void);

#endif /* INC_CAN_TEST_H_ */
