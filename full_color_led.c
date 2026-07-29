#include "device_driver.h"

#define FULL_COLOR_LED_COUNT       4u
#define WS2812_BITS_PER_LED        24u
#define WS2812_DATA_COUNT          (FULL_COLOR_LED_COUNT * WS2812_BITS_PER_LED)

#define WS2812_TIMER_HZ            TIMXCLK
#define WS2812_BIT_RATE_HZ         800000u
#define WS2812_PERIOD_TICKS        (WS2812_TIMER_HZ / WS2812_BIT_RATE_HZ)
#define WS2812_ARR_DATA            (WS2812_PERIOD_TICKS - 1u)

#define WS2812_T0H_TICKS           38u
#define WS2812_T1H_TICKS           82u
#define WS2812_RESET_US            80u
#define WS2812_ARR_RESET           (((WS2812_TIMER_HZ / 1000000u) * WS2812_RESET_US) - 1u)

typedef struct
{
    unsigned char red;
    unsigned char green;
    unsigned char blue;
} FULL_COLOR_RGB;

enum
{
    WS2812_STATE_IDLE = 0,
    WS2812_STATE_DATA,
    WS2812_STATE_RESET_PENDING,
    WS2812_STATE_RESET_ACTIVE
};

static FULL_COLOR_RGB full_color_led[FULL_COLOR_LED_COUNT];
static unsigned short ws2812_ccr_table[WS2812_DATA_COUNT];
static volatile unsigned int ws2812_index = 0;
static volatile unsigned int ws2812_state = WS2812_STATE_IDLE;
static volatile int ws2812_busy = 0;

static void Full_Color_LED_Build_Table(void)
{
    unsigned int led;
    unsigned int byte_index;
    int bit;
    unsigned int table_index = 0;

    for(led = 0; led < FULL_COLOR_LED_COUNT; led++)
    {
        unsigned char grb[3];

        grb[0] = full_color_led[led].green;
        grb[1] = full_color_led[led].red;
        grb[2] = full_color_led[led].blue;

        for(byte_index = 0; byte_index < 3u; byte_index++)
        {
            for(bit = 7; bit >= 0; bit--)
            {
                if(grb[byte_index] & (1u << bit))
                {
                    ws2812_ccr_table[table_index] = WS2812_T1H_TICKS;
                }
                else
                {
                    ws2812_ccr_table[table_index] = WS2812_T0H_TICKS;
                }

                table_index++;
            }
        }
    }
}

void Full_Color_LED_Init(void)
{
    unsigned int i;

    Macro_Set_Bit(RCC->AHB1ENR, 1);
    Macro_Set_Bit(RCC->APB1ENR, 2);

    Macro_Write_Block(GPIOB->MODER, 0x3, 0x2, 12);
    Macro_Write_Block(GPIOB->AFR[0], 0xF, 0x2, 24);
    Macro_Write_Block(GPIOB->PUPDR, 0x3, 0x0, 12);
    Macro_Write_Block(GPIOB->OSPEEDR, 0x3, 0x3, 12);
    Macro_Clear_Bit(GPIOB->OTYPER, 6);

    TIM4->CR1 = (1u << 7);
    TIM4->CR2 = 0;
    TIM4->SMCR = 0;
    TIM4->DIER = 0;
    TIM4->CCMR1 = (6u << 4) | (1u << 3);
    TIM4->CCER = 0;
    TIM4->PSC = 0;
    TIM4->ARR = WS2812_ARR_DATA;
    TIM4->CCR1 = 0;
    TIM4->CNT = 0;
    TIM4->EGR = 1u;
    TIM4->SR = 0;

    NVIC_DisableIRQ(TIM4_IRQn);
    NVIC_ClearPendingIRQ(TIM4_IRQn);
    NVIC_SetPriority(TIM4_IRQn, 0);
    NVIC_EnableIRQ(TIM4_IRQn);

    for(i = 0; i < FULL_COLOR_LED_COUNT; i++)
    {
        full_color_led[i].red = 0;
        full_color_led[i].green = 0;
        full_color_led[i].blue = 0;
    }
}

void Full_Color_LED_Set_RGB(unsigned int index,
                            unsigned char red,
                            unsigned char green,
                            unsigned char blue)
{
    if(index >= FULL_COLOR_LED_COUNT)
    {
        return;
    }

    full_color_led[index].red = red;
    full_color_led[index].green = green;
    full_color_led[index].blue = blue;
}

int Full_Color_LED_Is_Busy(void)
{
    return ws2812_busy;
}

void Full_Color_LED_Update(void)
{
    unsigned int systick_interrupt_enabled;
    unsigned int usart2_rx_interrupt_enabled;

    while(ws2812_busy);

    Full_Color_LED_Build_Table();

    systick_interrupt_enabled = SysTick->CTRL & SysTick_CTRL_TICKINT_Msk;
    usart2_rx_interrupt_enabled = USART2->CR1 & USART_CR1_RXNEIE;

    SysTick->CTRL &= ~SysTick_CTRL_TICKINT_Msk;
    USART2->CR1 &= ~USART_CR1_RXNEIE;

    TIM4->CR1 &= ~TIM_CR1_CEN;
    TIM4->DIER = 0;
    TIM4->CCER = 0;

    TIM4->PSC = 0;
    TIM4->ARR = WS2812_ARR_DATA;
    TIM4->CCR1 = 0;
    TIM4->EGR = TIM_EGR_UG;
    TIM4->SR = 0;

    TIM4->CCR1 = ws2812_ccr_table[0];
    TIM4->CNT = WS2812_ARR_DATA;

    ws2812_index = 1;
    ws2812_state = WS2812_STATE_DATA;
    ws2812_busy = 1;

    NVIC_ClearPendingIRQ(TIM4_IRQn);
    TIM4->DIER = TIM_DIER_UIE;
    TIM4->CCER = TIM_CCER_CC1E;
    TIM4->CR1 |= TIM_CR1_CEN;

    while(ws2812_busy);

    if(systick_interrupt_enabled)
    {
        SysTick->CTRL |= SysTick_CTRL_TICKINT_Msk;
    }

    if(usart2_rx_interrupt_enabled)
    {
        USART2->CR1 |= USART_CR1_RXNEIE;
    }
}

void Full_Color_LED_All_Off(void)
{
    unsigned int i;

    for(i = 0; i < FULL_COLOR_LED_COUNT; i++)
    {
        Full_Color_LED_Set_RGB(i, 0, 0, 0);
    }

    Full_Color_LED_Update();
}

void Full_Color_LED_Show_4_Colors(void)
{
    Full_Color_LED_Set_RGB(0, 40, 0, 0);
    Full_Color_LED_Set_RGB(1, 40, 8, 0);
    Full_Color_LED_Set_RGB(2, 40, 28, 0);
    Full_Color_LED_Set_RGB(3, 0, 40, 0);
    Full_Color_LED_Update();
}

void Full_Color_LED_Show_Fan_Level(unsigned int level)
{
    unsigned int i;

    if(level > 4u)
    {
        level = 4u;
    }

    for(i = 0; i < FULL_COLOR_LED_COUNT; i++)
    {
        Full_Color_LED_Set_RGB(i, 0, 0, 0);
    }

    if(level >= 1u)
    {
        Full_Color_LED_Set_RGB(3, 0, 40, 0);
    }

    if(level >= 2u)
    {
        Full_Color_LED_Set_RGB(2, 40, 28, 0);
    }

    if(level >= 3u)
    {
        Full_Color_LED_Set_RGB(1, 40, 8, 0);
    }

    if(level >= 4u)
    {
        Full_Color_LED_Set_RGB(0, 40, 0, 0);
    }

    Full_Color_LED_Update();
}

void TIM4_IRQHandler(void)
{
    TIM4->SR = 0;

    if(ws2812_state == WS2812_STATE_DATA)
    {
        if(ws2812_index < WS2812_DATA_COUNT)
        {
            TIM4->CCR1 = ws2812_ccr_table[ws2812_index++];
        }
        else
        {
            TIM4->CCR1 = 0;
            TIM4->ARR = WS2812_ARR_RESET;
            ws2812_state = WS2812_STATE_RESET_PENDING;
        }
    }
    else if(ws2812_state == WS2812_STATE_RESET_PENDING)
    {
        ws2812_state = WS2812_STATE_RESET_ACTIVE;
    }
    else
    {
        TIM4->CR1 &= ~TIM_CR1_CEN;
        TIM4->DIER = 0;
        TIM4->CCER = 0;
        TIM4->CCR1 = 0;
        ws2812_state = WS2812_STATE_IDLE;
        ws2812_busy = 0;
    }
}
