#include "can_handler.h"
#include "stm32f7xx.h"
#include <string.h>

CAN_Feedback_t g_can_feedback = {0};

void CAN_Init(void) {
    /* 1. Enable GPIOA and CAN1 clocks */
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    RCC->APB1ENR |= RCC_APB1ENR_CAN1EN;

    /* 2. Configure PA11 (RX) and PA12 (TX) for Alternate Function 9 (CAN1) */
    GPIOA->MODER &= ~(GPIO_MODER_MODER11 | GPIO_MODER_MODER12);
    GPIOA->MODER |= (GPIO_MODER_MODER11_1 | GPIO_MODER_MODER12_1); // AF mode
    
    GPIOA->OSPEEDR |= (GPIO_OSPEEDER_OSPEEDR11 | GPIO_OSPEEDER_OSPEEDR12); // High speed
    
    GPIOA->PUPDR &= ~(GPIO_PUPDR_PUPDR11 | GPIO_PUPDR_PUPDR12);
    GPIOA->PUPDR |= (GPIO_PUPDR_PUPDR11_0 | GPIO_PUPDR_PUPDR12_0); // Pull-up
    
    GPIOA->AFR[1] &= ~((0xFU << (4 * (11 - 8))) | (0xFU << (4 * (12 - 8))));
    GPIOA->AFR[1] |= ((9U << (4 * (11 - 8))) | (9U << (4 * (12 - 8)))); // AF9

    /* Exit sleep mode first */
    CAN1->MCR &= ~CAN_MCR_SLEEP;

    /* 3. Enter CAN Initialization mode */
    CAN1->MCR |= CAN_MCR_INRQ;
    while ((CAN1->MSR & CAN_MSR_INAK) == 0); // Wait for init mode

    /* 4. Configure CAN bit timing
       APB1 Clock = 54 MHz
       Baudrate = 500 kbps
       Prescaler = 6 -> 9 MHz time quantum clock
       1 tq = 1/9 us. We need 18 tq per bit (9MHz / 18 = 500kHz)
       Sync = 1tq, BS1 = 14tq, BS2 = 3tq, SJW = 1tq
    */
    CAN1->BTR = 0;
    CAN1->BTR |= (5U << 0);           // BRP = 6 - 1 = 5
    CAN1->BTR |= (13U << 16);         // TS1 = 14 - 1 = 13
    CAN1->BTR |= (2U << 20);          // TS2 = 3 - 1 = 2
    CAN1->BTR |= (0U << 24);          // SJW = 1 - 1 = 0

    /* 5. Configure Filters */
    CAN1->FMR |= CAN_FMR_FINIT;       // Filter init mode
    CAN1->FA1R &= ~1U;                // Deactivate filter 0
    CAN1->FS1R |= 1U;                 // Single 32-bit scale for filter 0
    CAN1->FM1R &= ~1U;                // Mask mode for filter 0
    CAN1->sFilterRegister[0].FR1 = 0; // ID = 0 (accept all)
    CAN1->sFilterRegister[0].FR2 = 0; // Mask = 0 (accept all)
    CAN1->FFA1R &= ~1U;               // Assign filter 0 to FIFO 0
    CAN1->FA1R |= 1U;                 // Activate filter 0
    CAN1->FMR &= ~CAN_FMR_FINIT;      // Leave filter init mode

    /* 6. Leave Init mode */
    CAN1->MCR &= ~CAN_MCR_INRQ;
    while ((CAN1->MSR & CAN_MSR_INAK) != 0); // Wait for normal mode
}

bool CAN_Send_Tester_CMD(uint16_t p1, uint16_t p2, uint16_t temp, uint16_t lf_dc, uint16_t rf_dc, uint16_t lr_dc, uint16_t rr_dc) {
    if ((CAN1->TSR & CAN_TSR_TME0) == 0) return false; // Mailbox 0 full

    /* Note: STM32F7 bxCAN only supports 8 bytes max DLC.
       Teensy expects 14-16 bytes for Tester_CMD.
       We can only send the first 8 bytes.
    */
    CAN1->sTxMailBox[0].TDTR = 8; // DLC = 8
    CAN1->sTxMailBox[0].TIR = (CAN_ID_TESTER_CMD << 21) | CAN_TI0R_TXRQ; // Standard ID, Request TX
    
    CAN1->sTxMailBox[0].TDLR = (p1 >> 8) | ((p1 & 0xFF) << 8) | ((p2 >> 8) << 16) | ((p2 & 0xFF) << 24);
    CAN1->sTxMailBox[0].TDHR = (temp >> 8) | ((temp & 0xFF) << 8) | ((lf_dc >> 8) << 16) | ((lf_dc & 0xFF) << 24);

    return true;
}

bool CAN_Send_Tester_CFG(uint16_t lf_freq, uint16_t rf_freq, uint16_t lr_freq, uint16_t rr_freq, uint16_t ps_v, uint16_t ps_i, uint8_t flags1, uint8_t flags2) {
    if ((CAN1->TSR & CAN_TSR_TME1) == 0) return false; // Mailbox 1 full

    CAN1->sTxMailBox[1].TDTR = 8; // DLC = 8
    CAN1->sTxMailBox[1].TIR = (CAN_ID_TESTER_CFG << 21) | CAN_TI1R_TXRQ;

    CAN1->sTxMailBox[1].TDLR = (lf_freq >> 8) | ((lf_freq & 0xFF) << 8) | ((rf_freq >> 8) << 16) | ((rf_freq & 0xFF) << 24);
    CAN1->sTxMailBox[1].TDHR = (lr_freq >> 8) | ((lr_freq & 0xFF) << 8) | ((rr_freq >> 8) << 16) | ((rr_freq & 0xFF) << 24);

    return true;
}

void CAN_ProcessRX(void) {
    /* Check if FIFO 0 has messages */
    if ((CAN1->RF0R & CAN_RF0R_FMP0) != 0) {
        uint32_t id = CAN1->sFIFOMailBox[0].RIR >> 21;
        uint32_t dlc = CAN1->sFIFOMailBox[0].RDTR & 0xF;
        uint32_t dlow = CAN1->sFIFOMailBox[0].RDLR;
        uint32_t dhigh = CAN1->sFIFOMailBox[0].RDHR;

        if (id == CAN_ID_TESTER_RET) {
            g_can_feedback.hall_feedback = dlow & 0xFF;
            g_can_feedback.current_comp = ((dlow >> 8) & 0xFF) << 8 | ((dlow >> 16) & 0xFF);
            // We only get 8 bytes max with bxCAN, so we can't read the full 20 bytes.
            g_can_feedback.updated = true;
        }

        /* Release FIFO 0 */
        CAN1->RF0R |= CAN_RF0R_RFOM0;
    }
}
