#include "stm32f4xx.h"
#include "option.h"
#include "macro.h"
#include "malloc.h"

// Uart.c

extern void Uart2_Init(int baud);
extern void Uart2_Send_Byte(char data);
extern void Uart2_RX_Interrupt_Enable(int en);

extern void Uart1_Init(int baud);
extern void Uart1_Send_Byte(char data);
extern void Uart1_Send_String(char *pt);
extern void Uart1_Printf(char *fmt,...);
extern char Uart1_Get_Char(void);
extern char Uart1_Get_Pressed(void);

// SysTick.c

extern void SysTick_Run(unsigned int msec);
extern int SysTick_Check_Timeout(void);
extern unsigned int SysTick_Get_Time(void);
extern unsigned int SysTick_Get_Load_Time(void);
extern void SysTick_Stop(void);

// Led.c

extern void LED_Init(void);
extern void LED_On(void);
extern void LED_Off(void);

// Clock.c

extern void Clock_Init(void);

// Key.c

extern void Key_Poll_Init(void);
extern int Key_Get_Pressed(void);
extern void Key_Wait_Key_Released(void);
extern void Key_Wait_Key_Pressed(void);
extern void Key_ISR_Enable(int en);

// Timer.c

extern void TIM2_Delay(int time);
extern void TIM2_Stopwatch_Start(void);
extern unsigned int TIM2_Stopwatch_Stop(void);
extern void TIM4_Repeat(int time);
extern int TIM4_Check_Timeout(void);
extern void TIM4_Stop(void);
extern void TIM4_Change_Value(int time);
extern void TIM4_Repeat_Interrupt_Enable(int en, int time);
extern void TIM3_Out_Init(void);
extern void TIM3_Out_Freq_Generation(unsigned short freq);
extern void TIM3_Out_Stop(void);

// i2c.c

#define SC16IS752_IODIR				0x0A
#define SC16IS752_IOSTATE			0x0B

extern void I2C1_SC16IS752_Init(unsigned int freq);
extern void I2C1_SC16IS752_Write_Reg(unsigned int addr, unsigned int data);
extern void I2C1_SC16IS752_Config_GPIO(unsigned int config);
extern void I2C1_SC16IS752_Write_GPIO(unsigned int data);

// spi.c

extern void SPI1_SC16IS752_Init(unsigned int div);
extern void SPI1_SC16IS752_Write_Reg(unsigned int addr, unsigned int data);
extern void SPI1_SC16IS752_Config_GPIO(unsigned int config);
extern void SPI1_SC16IS752_Write_GPIO(unsigned int data);

// Adc.c
extern void ADC1_IN6_Init(void);
extern void ADC1_Start(void);
extern void ADC1_Stop(void);
extern int ADC1_Get_Status(void);
extern int ADC1_Get_Data(void);

// Full_Color_LED.c - PB6 / TIM4_CH1 / WS2812B x4
extern void Full_Color_LED_Init(void);
extern void Full_Color_LED_Set_RGB(unsigned int index, unsigned char red, unsigned char green, unsigned char blue);
extern void Full_Color_LED_Update(void);
extern void Full_Color_LED_All_Off(void);
extern void Full_Color_LED_Show_4_Colors(void);
extern void Full_Color_LED_Show_Fan_Level(unsigned int level);
extern int Full_Color_LED_Is_Busy(void);
