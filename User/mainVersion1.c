/********************************** (C) COPYRIGHT *******************************
 * File Name          : main_v3f.c
 * Author             : WCH
 * Version            : V1.0.0
 * Date               : 2025/03/01
 * Description        : Main program body for V3F.
 *********************************************************************************
 * Copyright (c) 2025 Nanjing Qinheng Microelectronics Co., Ltd.
 * Attention: This software (modified or not) and binary are used for
 * microcontroller manufactured by Nanjing Qinheng Microelectronics.
 *******************************************************************************/

/*
 *@Note
 polling transceiver mode, master/slave transceiver routine:
 Master:USART2_Tx(PD5)\USART2_Rx(PD6).
 Slave:USART3_Tx(PB10)\USART3_Rx(PB11).
 This example demonstrates sending from USART2 and receiving from USART3.

   Hardware connection:
			   PD5 -- PB11
               PD6 -- PB10

*/

#include "debug.h"
#include "hardware.h"

/* Add PA10 as USART1 RX and re-enable USART1 in TX+RX mode */
void USART1_RX_Enable(void)
{
    GPIO_InitTypeDef  GPIO_InitStructure  = {0};
    USART_InitTypeDef USART_InitStructure = {0};

    /* Clocks were already enabled by USART_Printf_Init, but re-enabling is harmless */
    RCC_HB2PeriphClockCmd(RCC_HB2Periph_AFIO | RCC_HB2Periph_USART1 | RCC_HB2Periph_GPIOA, ENABLE);

    /* Route PA10 to USART1 (same AF7 as PA9) */
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource10, GPIO_AF7);

    /* PA10 as input for USART1_RX */
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_10;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_Very_High;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_IN_FLOATING;   /* try GPIO_Mode_IPU if flaky */
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    /* Re-init USART1 with BOTH Tx and Rx enabled (Printf_Init set Tx only) */
    USART_InitStructure.USART_BaudRate            = 115200;
    USART_InitStructure.USART_WordLength          = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits            = USART_StopBits_1;
    USART_InitStructure.USART_Parity              = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode                = USART_Mode_Tx | USART_Mode_Rx;
    USART_Init(USART1, &USART_InitStructure);
    USART_Cmd(USART1, ENABLE);
}
/*********************************************************************
 * @fn      main
 *
 * @brief   Main program.
 *
 * @return  none
 */

int main(void)
{
	uint8_t rx_byte;

    SystemInit();
    SystemAndCoreClockUpdate();
    Delay_Init();
    USART_Printf_Init(115200);   /* sets up TX on PA9 */
    USART1_RX_Enable();          /* adds RX on PA10   */
    Delay_Ms(500);

    printf("V3F ready. Type something:\r\n");

    while(1)
    {
        /* Poll the RX-not-empty flag, non-blocking style */
        if(USART_GetFlagStatus(USART1, USART_FLAG_RXNE) != RESET)
        {
            rx_byte = (uint8_t)USART_ReceiveData(USART1);   /* clears RXNE */
            printf("Got: '%c' (0x%02X)\r\n", rx_byte, rx_byte);
        }
    }
}
