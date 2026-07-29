#include "device_driver.h"
#include <stdio.h>

#define LED_COUNT               8
#define LEFT_HIT_MAX_LED        1
#define RIGHT_HIT_MIN_LED       6

#define START_MOVE_TIME_MS      500
#define MIN_MOVE_TIME_MS        100
#define SPEED_UP_TIME_MS        50

#define GAME_TICK_MS            5
#define BUTTON_DEBOUNCE_COUNT   3

#define WIN_FLASH_COUNT         5
#define WIN_FLASH_TIME_MS       150

#define FAN_ADC_UPDATE_MS       20
#define FAN_PRINT_TIME_MS       250
#define FAN_PWM_FREQ_HZ         1000
#define FAN_MIN_DUTY_PERCENT    20

#define PLAYER_LEFT             0
#define PLAYER_RIGHT            1

#define DIR_LEFT               -1
#define DIR_RIGHT               1

typedef struct
{
    int stable;
    int sample;
    int count;
} BUTTON_FILTER;

static BUTTON_FILTER fan_button;
static int fan_on = 0;
static int fan_adc_elapsed = 0;
static int fan_print_elapsed = 0;
static int fan_duty = 0;
static unsigned int fan_adc_value = 0;
static int fan_led_level = -1;
volatile int fan_uart_toggle_event = 0;

void USART2_IRQHandler(void)
{
    unsigned char data;

    if(Macro_Check_Bit_Set(USART2->SR, 5))
    {
        data = (unsigned char)USART2->DR;

        if((data == 'm') || (data == 'M'))
        {
            fan_uart_toggle_event = 1;
        }
    }

    NVIC_ClearPendingIRQ(38);
}

static void Sys_Init(int baud)
{
    SCB->CPACR |= (0x3 << 10 * 2) | (0x3 << 11 * 2);
    Clock_Init();
    Uart2_Init(baud);
    setvbuf(stdout, NULL, _IONBF, 0);
}

static void Game_Button_Init(void)
{
    Macro_Set_Bit(RCC->AHB1ENR, 1);
    Macro_Set_Bit(RCC->AHB1ENR, 2);

    Macro_Write_Block(GPIOC->MODER, 0x3, 0x0, 26);
    Macro_Write_Block(GPIOC->PUPDR, 0x3, 0x1, 26);

    Macro_Write_Block(GPIOB->MODER, 0x3, 0x0, 16);
    Macro_Write_Block(GPIOB->PUPDR, 0x3, 0x1, 16);
}

static void Fan_Button_Init(void)
{
    Macro_Set_Bit(RCC->AHB1ENR, 0);

    Macro_Write_Block(GPIOA->MODER, 0x3, 0x0, 18);
    Macro_Write_Block(GPIOA->PUPDR, 0x3, 0x2, 18);
}

static int Left_Button_Raw(void)
{
    return Macro_Check_Bit_Clear(GPIOB->IDR, 8);
}

static int Right_Button_Raw(void)
{
    return Macro_Check_Bit_Clear(GPIOC->IDR, 13);
}

static int Fan_Button_Raw(void)
{
    return Macro_Check_Bit_Set(GPIOA->IDR, 9);
}

static void Button_Filter_Reset(BUTTON_FILTER *button, int raw)
{
    button->stable = raw;
    button->sample = raw;
    button->count = BUTTON_DEBOUNCE_COUNT;
}

static int Button_Filter_Update(BUTTON_FILTER *button, int raw)
{
    if(raw == button->sample)
    {
        if(button->count < BUTTON_DEBOUNCE_COUNT)
        {
            button->count++;
        }
    }
    else
    {
        button->sample = raw;
        button->count = 1;
    }

    if((button->count >= BUTTON_DEBOUNCE_COUNT) &&
       (button->stable != button->sample))
    {
        button->stable = button->sample;

        if(button->stable)
        {
            return 1;
        }
    }

    return 0;
}

static unsigned int Fan_ADC_Read(void)
{
    ADC1_Start();

    while(!ADC1_Get_Status());

    return (unsigned int)ADC1_Get_Data();
}

static int Fan_ADC_To_Duty(unsigned int adc_value)
{
    unsigned int duty;

    if(adc_value > 4095u)
    {
        adc_value = 4095u;
    }

    duty = FAN_MIN_DUTY_PERCENT +
           ((adc_value * (100u - FAN_MIN_DUTY_PERCENT) + 2047u) / 4095u);

    if(duty > 100u)
    {
        duty = 100u;
    }

    return (int)duty;
}

static int Fan_ADC_To_LED_Level(unsigned int adc_value)
{
    if(adc_value < 1024u)
    {
        return 1;
    }
    else if(adc_value < 2048u)
    {
        return 2;
    }
    else if(adc_value < 3072u)
    {
        return 3;
    }

    return 4;
}

static void Fan_LED_Update(void)
{
    int level;

    if(fan_on)
    {
        level = Fan_ADC_To_LED_Level(fan_adc_value);
    }
    else
    {
        level = 0;
    }

    if(level != fan_led_level)
    {
        Full_Color_LED_Show_Fan_Level((unsigned int)level);
        fan_led_level = level;
    }
}

static void Motor_PWM_Init(void)
{
    unsigned int prescaler;
    unsigned int period;

    Macro_Set_Bit(RCC->AHB1ENR, 0);
    Macro_Set_Bit(RCC->APB1ENR, 3);

    Macro_Write_Block(GPIOA->MODER, 0x3, 0x1, 0);
    Macro_Clear_Bit(GPIOA->OTYPER, 0);
    Macro_Clear_Bit(GPIOA->ODR, 0);

    Macro_Write_Block(GPIOA->MODER, 0x3, 0x2, 2);
    Macro_Write_Block(GPIOA->AFR[0], 0xF, 0x2, 4);
    Macro_Clear_Bit(GPIOA->OTYPER, 1);
    Macro_Write_Block(GPIOA->OSPEEDR, 0x3, 0x2, 2);

    prescaler = (TIMXCLK / 1000000u) - 1u;
    period = (1000000u / FAN_PWM_FREQ_HZ) - 1;

    TIM5->CR1 = 0;
    TIM5->PSC = prescaler;
    TIM5->ARR = period;
    TIM5->CCR2 = 0;

    Macro_Write_Block(TIM5->CCMR1, 0xFF, 0x68, 8);
    TIM5->CCER = (1 << 4);
    TIM5->CR1 = (1 << 7);

    Macro_Set_Bit(TIM5->EGR, 0);
    Macro_Set_Bit(TIM5->CR1, 0);
}

static void Motor_Set_Duty(int duty)
{
    if(duty < 0)
    {
        duty = 0;
    }
    else if(duty > 100)
    {
        duty = 100;
    }

    fan_duty = duty;
    TIM5->CCR2 = ((TIM5->ARR + 1u) * (unsigned int)duty) / 100u;
}

static void Fan_Init(void)
{
    Fan_Button_Init();
    ADC1_IN6_Init();
    Motor_PWM_Init();

    Button_Filter_Reset(&fan_button, Fan_Button_Raw());
    Motor_Set_Duty(0);
}

static void Fan_Toggle(void)
{
    fan_on = !fan_on;

    if(fan_on)
    {
        fan_adc_elapsed = FAN_ADC_UPDATE_MS;
        fan_print_elapsed = FAN_PRINT_TIME_MS;
        printf("FAN ON\n");
    }
    else
    {
        Motor_Set_Duty(0);
        Fan_LED_Update();
        printf("FAN OFF\n");
    }
}

static void Fan_Update(void)
{
    int toggle_request = 0;

    if(Button_Filter_Update(&fan_button, Fan_Button_Raw()))
    {
        toggle_request = 1;
    }

    if(fan_uart_toggle_event)
    {
        fan_uart_toggle_event = 0;
        toggle_request = 1;
    }

    if(toggle_request)
    {
        Fan_Toggle();
    }

    fan_adc_elapsed += GAME_TICK_MS;
    fan_print_elapsed += GAME_TICK_MS;

    if(fan_on && (fan_adc_elapsed >= FAN_ADC_UPDATE_MS))
    {
        int duty;

        fan_adc_elapsed = 0;
        fan_adc_value = Fan_ADC_Read();
        duty = Fan_ADC_To_Duty(fan_adc_value);

        if(duty != fan_duty)
        {
            Motor_Set_Duty(duty);
        }

        Fan_LED_Update();
    }

    if(fan_on && (fan_print_elapsed >= FAN_PRINT_TIME_MS))
    {
        fan_print_elapsed = 0;
        printf("ADC=%4u, DUTY=%3d%%, CCR2=%4u/%4u\n",
               fan_adc_value,
               fan_duty,
               (unsigned int)TIM5->CCR2,
               (unsigned int)(TIM5->ARR + 1u));
    }
}

static void Delay_With_Fan(int time_ms)
{
    int elapsed = 0;

    while(elapsed < time_ms)
    {
        TIM2_Delay(GAME_TICK_MS);
        Fan_Update();
        elapsed += GAME_TICK_MS;
    }
}

static void LED_All_Off(void)
{
    SPI1_SC16IS752_Write_GPIO(0xFF);
}

static void LED_Show_Ball(int position)
{
    unsigned int data;

    data = (~(1u << position)) & 0xFFu;
    SPI1_SC16IS752_Write_GPIO(data);
}

static void Change_Ball_Direction(int *direction, int *move_time)
{
    *direction = -*direction;

    if(*move_time > MIN_MOVE_TIME_MS)
    {
        *move_time -= SPEED_UP_TIME_MS;

        if(*move_time < MIN_MOVE_TIME_MS)
        {
            *move_time = MIN_MOVE_TIME_MS;
        }
    }
}

static int Play_Game(void)
{
    BUTTON_FILTER left_button;
    BUTTON_FILTER right_button;
    int ball_position = 0;
    int direction = DIR_RIGHT;
    int move_time = START_MOVE_TIME_MS;
    int move_elapsed = 0;

    Button_Filter_Reset(&left_button, Left_Button_Raw());
    Button_Filter_Reset(&right_button, Right_Button_Raw());

    LED_Show_Ball(ball_position);
    SysTick_Run(GAME_TICK_MS);

    printf("Game Start\n");

    for(;;)
    {
        if(SysTick_Check_Timeout())
        {
            int left_pressed;
            int right_pressed;

            SysTick_Run(GAME_TICK_MS);
            Fan_Update();

            left_pressed = Button_Filter_Update(&left_button, Left_Button_Raw());
            right_pressed = Button_Filter_Update(&right_button, Right_Button_Raw());

            if(left_pressed && right_pressed)
            {
                SysTick_Stop();
                return (direction == DIR_LEFT) ? PLAYER_RIGHT : PLAYER_LEFT;
            }

            if(left_pressed)
            {
                if((direction == DIR_LEFT) &&
                   (ball_position <= LEFT_HIT_MAX_LED))
                {
                    Change_Ball_Direction(&direction, &move_time);
                    move_elapsed = 0;
                    printf("LEFT HIT, Speed = %d ms\n", move_time);
                }
                else
                {
                    SysTick_Stop();
                    return PLAYER_RIGHT;
                }
            }

            if(right_pressed)
            {
                if((direction == DIR_RIGHT) &&
                   (ball_position >= RIGHT_HIT_MIN_LED))
                {
                    Change_Ball_Direction(&direction, &move_time);
                    move_elapsed = 0;
                    printf("RIGHT HIT, Speed = %d ms\n", move_time);
                }
                else
                {
                    SysTick_Stop();
                    return PLAYER_LEFT;
                }
            }

            move_elapsed += GAME_TICK_MS;

            if(move_elapsed >= move_time)
            {
                move_elapsed = 0;
                ball_position += direction;

                if(ball_position < 0)
                {
                    SysTick_Stop();
                    return PLAYER_RIGHT;
                }

                if(ball_position >= LED_COUNT)
                {
                    SysTick_Stop();
                    return PLAYER_LEFT;
                }

                LED_Show_Ball(ball_position);
            }
        }
    }
}

static void Winner_Flash(int winner)
{
    int i;
    unsigned int winner_led;

    if(winner == PLAYER_LEFT)
    {
        winner_led = 0xF0;
        printf("LEFT WIN\n");
    }
    else
    {
        winner_led = 0x0F;
        printf("RIGHT WIN\n");
    }

    for(i = 0; i < WIN_FLASH_COUNT; i++)
    {
        SPI1_SC16IS752_Write_GPIO(winner_led);
        Delay_With_Fan(WIN_FLASH_TIME_MS);
        LED_All_Off();
        Delay_With_Fan(WIN_FLASH_TIME_MS);
    }

    Delay_With_Fan(500);
}

void Main(void)
{
    int winner;

    Sys_Init(115200);
    Uart2_RX_Interrupt_Enable(1);
    Game_Button_Init();
    Fan_Init();

    SPI1_SC16IS752_Init(32);
    SPI1_SC16IS752_Config_GPIO(0xFF);
    LED_All_Off();

    Full_Color_LED_Init();
    Full_Color_LED_All_Off();
    fan_led_level = 0;

    printf("SC16IS752 Ping-Pong + ADC Fan V2\n");
    printf("LEFT=PB8, RIGHT=PC13, FAN=PA9/M, ADC=PA6, PWM=PA1\n");
    printf("FULL COLOR LED=PB6/TIM4_CH1: FAN LEVEL 1~4\n");
    printf("LEVEL1=GREEN, LEVEL2=+YELLOW, LEVEL3=+ORANGE, LEVEL4=+RED\n");

    for(;;)
    {
        winner = Play_Game();
        Winner_Flash(winner);
    }
}
