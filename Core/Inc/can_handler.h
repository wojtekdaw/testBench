#ifndef CAN_HANDLER_H
#define CAN_HANDLER_H

#include <stdint.h>
#include <stdbool.h>

/* CAN Message IDs */
#define CAN_ID_TESTER_CMD 0x18D
#define CAN_ID_TESTER_CFG 0x68D
#define CAN_ID_TESTER_RET 0x18E
#define CAN_ID_TESTER_RET2 0x28E

/* Structure to store latest received feedback */
typedef struct {
    uint8_t hall_feedback;
    int16_t current_comp;
    int16_t current_exh;
    int16_t current_damp1;
    int16_t current_damp2;
    int16_t current_damp3;
    int16_t current_damp4;
    uint16_t voltage_fb;
    uint16_t current_fb;
    bool updated;
} CAN_Feedback_t;

extern CAN_Feedback_t g_can_feedback;

/* Initialization */
void CAN_Init(void);

/* Transmit Functions */
bool CAN_Send_Tester_CMD(uint16_t p1, uint16_t p2, uint16_t temp, uint16_t lf_dc, uint16_t rf_dc, uint16_t lr_dc, uint16_t rr_dc);
bool CAN_Send_Tester_CFG(uint16_t lf_freq, uint16_t rf_freq, uint16_t lr_freq, uint16_t rr_freq, uint16_t ps_v, uint16_t ps_i, uint8_t flags1, uint8_t flags2);

/* This function should be called in a background task or loop to process received frames */
void CAN_ProcessRX(void);

#endif /* CAN_HANDLER_H */
