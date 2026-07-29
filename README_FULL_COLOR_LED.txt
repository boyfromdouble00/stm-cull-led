STM32F411 M4 + BUS-EXT Full Color LED integration

Wiring already used:
- M4 PB6 -> BUS-EXT pin 9 LED_DIN
- BUS-EXT pin 4 VDD_3V3 -> BUS-EXT pin 1 VDD_5V jumper
- Existing BUS-EXT 3.3V and GND remain connected
- Do not connect a separate 5V supply to BUS pins 1/2 while pins 4 and 1 are jumpered.

Driver:
- PB6 = TIM4_CH1, AF2
- Timer clock = 96 MHz
- WS2812B bit rate = 800 kHz
- PWM period: ARR=119, PSC=0
- Logic 0: CCR1=38 (~0.396 us HIGH)
- Logic 1: CCR1=82 (~0.854 us HIGH)
- PWM mode 1
- CCR1 preload enabled (OC1PE=1)
- ARR preload enabled (ARPE=1)
- Continuous/repeat mode (OPM=0)
- GRB, MSB first
- 4 LEDs x 24 bits = 96 entries
- Reset LOW = 80 us

Implementation note:
TIM4 update interrupt writes the next CCR preload value. This gives the ISR a full
1.25 us PWM period to prepare the next bit. A CCR compare interrupt would leave only
about 0.4 us after a logic-1 HIGH pulse, which is unsafe for a C ISR at 96 MHz.

Startup display:
LED0 Red, LED1 Orange, LED2 Yellow, LED3 Green at reduced brightness.

TIM4 is reserved for the WS2812B driver. Do not call TIM4_Repeat(),
TIM4_Repeat_Interrupt_Enable(), or other TIM4 functions while using this driver.


Fan level RGB behavior
- Fan OFF: all full-color LEDs OFF
- Level 1 (ADC 0~1023): GREEN
- Level 2 (ADC 1024~2047): GREEN + YELLOW
- Level 3 (ADC 2048~3071): GREEN + YELLOW + ORANGE
- Level 4 (ADC 3072~4095): GREEN + YELLOW + ORANGE + RED
- PA9 button or Tera Term M/m toggles fan ON/OFF.
