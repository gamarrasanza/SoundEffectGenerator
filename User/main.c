#include "debug.h"
#include <math.h>
#include <stdarg.h>
#include <stdio.h>


#define SAMPLE_RATE_HZ    16000UL 
#define SINE_TABLE_SIZE   256

static int16_t sine_table[SINE_TABLE_SIZE];

static volatile uint32_t phase_acc = 0; /*Phase accumulated value*/
static volatile uint32_t phase_inc = 0; /*Phase increment value*/


#define ENV_MAX   256
#define ENV_STEP  4

static volatile uint16_t env_level  = 0;
static volatile uint16_t env_target = 0;

/*Notes used*/
typedef enum {
    NOTE_REST = 0,
    NOTE_C4 = 262,  NOTE_D4 = 294,  NOTE_E4 = 330,  NOTE_F4 = 349,
    NOTE_G4 = 392,  NOTE_A4 = 440,  NOTE_AS4 = 466, NOTE_B4 = 494,
    NOTE_C5 = 523,  NOTE_D5 = 587,  NOTE_E5 = 659,  NOTE_F5 = 698,
    NOTE_G5 = 784,  NOTE_A5 = 880,  NOTE_B5 = 988,  NOTE_C6 = 1047
} Note_Freq;

typedef struct {
    uint16_t freq_hz;
    uint16_t duration_ms;
} Note;

/* Effects and melodies*/
static const Note effect_beep[] = {
    {NOTE_A5, 150},
};

static const Note effect_double_beep[] = {
    {NOTE_A5, 100}, {NOTE_REST, 80}, {NOTE_A5, 100},
};

static const Note effect_alarm[] = {
    {NOTE_A5, 200}, {NOTE_E5, 200}, {NOTE_A5, 200}, {NOTE_E5, 200},
};

static const Note effect_power_up[] = {
    {NOTE_C4, 80},  {NOTE_E4, 80},  {NOTE_G4, 80},
    {NOTE_C5, 80},  {NOTE_E5, 80},  {NOTE_G5, 120},
};

static const Note effect_power_down[] = {
    {NOTE_G5, 80},  {NOTE_E5, 80},  {NOTE_C5, 80},
    {NOTE_G4, 80},  {NOTE_E4, 80},  {NOTE_C4, 120},
};

static const Note effect_coin[] = {
    {NOTE_B5, 90}, {NOTE_E5, 250},
};

static const Note melody_scale[] = {
    {NOTE_C4, 200}, {NOTE_D4, 200}, {NOTE_E4, 200}, {NOTE_F4, 200},
    {NOTE_G4, 200}, {NOTE_A4, 200}, {NOTE_B4, 200}, {NOTE_C5, 400},
};

static const Note melody_arpeggio[] = {
    {NOTE_C5, 120}, {NOTE_E5, 120}, {NOTE_G5, 120}, {NOTE_C6, 240},
};

static const Note melody_happy_birthday[] = {
    {NOTE_C4, 150}, {NOTE_C4, 150},
    {NOTE_D4, 300}, {NOTE_C4, 300},
    {NOTE_F4, 300}, {NOTE_E4, 600},
    {NOTE_REST, 100},
    {NOTE_C4, 150}, {NOTE_C4, 150},
    {NOTE_D4, 300}, {NOTE_C4, 300},
    {NOTE_G4, 300}, {NOTE_F4, 600},
    {NOTE_REST, 100},
    {NOTE_C4, 150}, {NOTE_C4, 150},
    {NOTE_C5, 300}, {NOTE_A4, 300},
    {NOTE_F4, 300}, {NOTE_E4, 300}, {NOTE_D4, 600},
    {NOTE_REST, 100},
    {NOTE_AS4, 150}, {NOTE_AS4, 150},
    {NOTE_A4, 300}, {NOTE_F4, 300},
    {NOTE_G4, 300}, {NOTE_F4, 600},
};

static const Note melody_got[] = {
    {NOTE_A4, 500}, {NOTE_D4, 500}, {NOTE_F4, 150}, {NOTE_G4, 150},
    {NOTE_A4, 500}, {NOTE_D4, 500}, {NOTE_F4, 150}, {NOTE_G4, 150},
    {NOTE_A4, 500}, {NOTE_D4, 500}, {NOTE_F4, 150}, {NOTE_G4, 150},
    {NOTE_E4, 540},
};

typedef struct {
    const char  *name;
    const Note  *notes;
    uint16_t     count;
} Track;

static const Track tracks[10] = {
    [0] = { "Simple beep",        effect_beep,           sizeof(effect_beep)/sizeof(Note) },
    [1] = { "Double beep",        effect_double_beep,    sizeof(effect_double_beep)/sizeof(Note) },
    [2] = { "Alarm",              effect_alarm,          sizeof(effect_alarm)/sizeof(Note) },
    [3] = { "Power up",           effect_power_up,       sizeof(effect_power_up)/sizeof(Note) },
    [4] = { "Power down",         effect_power_down,     sizeof(effect_power_down)/sizeof(Note) },
    [5] = { "Coin",               effect_coin,           sizeof(effect_coin)/sizeof(Note) },
    [6] = { "Scale (C major)",    melody_scale,          sizeof(melody_scale)/sizeof(Note) },
    [7] = { "Arpeggio",           melody_arpeggio,       sizeof(melody_arpeggio)/sizeof(Note) },
    [8] = { "Happy Birthday",     melody_happy_birthday, sizeof(melody_happy_birthday)/sizeof(Note) },
    [9] = { "Game of Thrones",    melody_got,            sizeof(melody_got)/sizeof(Note) },
};

/* Tempo and pitch controls*/
static int16_t  tempo_offset_ms        = 0;
static uint16_t tempo_scale_pct        = 100;
static int8_t   pitch_offset_semitones = 0;

static uint16_t apply_pitch(uint16_t freq_hz)
{
    if (freq_hz == 0) return 0;
    if (pitch_offset_semitones == 0) return freq_hz;

    float factor  = powf(1.05946309436f, (float)pitch_offset_semitones);
    float shifted = (float)freq_hz * factor; /*Scale frequency the value of the factor*/

    /*Limit the frequency to the hearable band*/
    if (shifted < 30.0f)    shifted = 30.0f; 
    if (shifted > 20000.0f) shifted = 20000.0f;
    return (uint16_t)(shifted + 0.5f);
}

/* USART1_RX_Enable ¡ª add reception (RX) to USART1*/
static void USART1_RX_Enable(void)
{
    /*Initializing the config structures*/
    GPIO_InitTypeDef  GPIO_InitStructure  = {0};
    USART_InitTypeDef USART_InitStructure = {0};

    /* Enable the clocks of the three peripherals involved, all on the HB2 bus
     *   - AFIO   : alternate-function I/O controller (pin mux)
     *   - USART1 : the UART itself
     *   - GPIOA  : the port that holds PA9 (TX) and PA10 (RX)*/
    RCC_HB2PeriphClockCmd(RCC_HB2Periph_AFIO | RCC_HB2Periph_USART1 | RCC_HB2Periph_GPIOA, ENABLE);
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource10, GPIO_AF7); /*For PA10, AF7 = USART1.*/

    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_10; /* pin 10 of port A */
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_Very_High; /* max speed (consistent with TX) */
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_IN_FLOATING; /*No internal pull resistor needed*/
    GPIO_Init(GPIOA, &GPIO_InitStructure); /*Apply configurations*/

    USART_InitStructure.USART_BaudRate            = 115200; /* bits per second (must match the PC) */
    USART_InitStructure.USART_WordLength          = USART_WordLength_8b; /* 8 data bits per byte */
    USART_InitStructure.USART_StopBits            = USART_StopBits_1; /* 1 stop bit */
    USART_InitStructure.USART_Parity              = USART_Parity_No; /* no parity bit */
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None; /* no RequestToSend/ClearToSend */
    USART_InitStructure.USART_Mode                = USART_Mode_Tx | USART_Mode_Rx; /* enable transmitter AND receiver */
    USART_Init(USART1, &USART_InitStructure); /* write parameters*/
    USART_Cmd(USART1, ENABLE); /* enable USART1 */
}

/*USART2 Configuration for the HC-05*/
static void USART2_Init(void)
{
    /*Initializing the config structures*/
    GPIO_InitTypeDef  GPIO_InitStructure  = {0};
    USART_InitTypeDef USART_InitStructure = {0};

    /* Enable the clocks of the three peripherals involved, on HB2 and HB1
     *   - AFIO   : alternate-function I/O controller (pin mux)
     *   - USART1 : the UART itself
     *   - GPIOA  : the port that holds PD5 (TX) and PD6 (RX)*/
    RCC_HB2PeriphClockCmd(RCC_HB2Periph_AFIO | RCC_HB2Periph_GPIOD, ENABLE);
    RCC_HB1PeriphClockCmd(RCC_HB1Periph_USART2, ENABLE);

    /*AF7 = USART2*/
    GPIO_PinAFConfig(GPIOD, GPIO_PinSource5, GPIO_AF7);
    GPIO_PinAFConfig(GPIOD, GPIO_PinSource6, GPIO_AF7);

    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_5; /* Pin 5 of port D */
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_Very_High;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF_PP; /*Alternate-function output*/
    GPIO_Init(GPIOD, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_6; /* Pin 6 of port D */
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_IN_FLOATING; /*Floating input*/
    GPIO_Init(GPIOD, &GPIO_InitStructure);

    USART_InitStructure.USART_BaudRate            = 9600; /* 9600 baud (HC-05 default) */
    USART_InitStructure.USART_WordLength          = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits            = USART_StopBits_1;
    USART_InitStructure.USART_Parity              = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode                = USART_Mode_Tx | USART_Mode_Rx; /* TX and RX enabled */
    USART_Init(USART2, &USART_InitStructure); /* apply to USART2's registers */
    USART_Cmd(USART2, ENABLE); /* turn on USART2 */
}

static void bt_send_char(char c)
{
    while (USART_GetFlagStatus(USART2, USART_FLAG_TXE) == RESET); /*Wait until the transmit register is empty*/
    USART_SendData(USART2, (uint8_t)c);
}

static void bt_send_string(const char *s)
{
    while (*s) bt_send_char(*s++); /*Sends the current character and advances while the character is not '\0' */
}

/* Send text to both pc and smartphone*/
static void dual_printf(const char *fmt, ...)
{
    char buf[128];
    va_list args; /*create a cursor for reading extra args*/
    va_start(args, fmt); /*point the cursor at the first extra arg*/
    vsnprintf(buf, sizeof(buf), fmt, args); /*hand the cursor to vsnprintf so it can read each extra arg as it processes %d, %s, etc. */
    va_end(args); /*clean up the cursor*/

    printf("%s", buf);
    bt_send_string(buf);
}

/* Digital to analog converter */
static void dac_init(void)
{
    /*Initialise structures*/
    GPIO_InitTypeDef GPIO_InitStructure = {0};
    DAC_InitTypeDef  DAC_InitStructure  = {0};

    /* Enable the peripheral clocks
     *   - GPIOA is on HB2 (it's the port that owns PA4)
     *   - DAC   is on HB1*/
    RCC_HB2PeriphClockCmd(RCC_HB2Periph_GPIOA, ENABLE);
    RCC_HB1PeriphClockCmd(RCC_HB1Periph_DAC, ENABLE);

    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_4; /* pin 4 of port A */
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AIN; /* analog mode*/
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_Very_High; /* max speed rate */
    GPIO_Init(GPIOA, &GPIO_InitStructure); /* write these settings into GPIOA's registers */

    DAC_InitStructure.DAC_Trigger        = DAC_Trigger_None; /* no external trigger */
    DAC_InitStructure.DAC_WaveGeneration = DAC_WaveGeneration_None; /*The waveform is generated in software*/
    DAC_InitStructure.DAC_OutputBuffer   = DAC_OutputBuffer_Enable; /*enable the output buffer*/
    DAC_Init(DAC_Channel_1, &DAC_InitStructure); /* apply these settings to DAC channel 1 */

    DAC_Cmd(DAC_Channel_1, ENABLE); /*Enable DAC Ch1*/
    DAC_SetChannel1Data(DAC_Align_12b_R, 2048); /* Write an initial value of 2048, its midpoint*/
}

/*Sine table*/
static void sine_table_init(void)
{
    for (int i = 0; i < SINE_TABLE_SIZE; i++) {
        float angle = (2.0f * 3.14159265f * (float)i) / (float)SINE_TABLE_SIZE; /*Divides 2pi in 256 sections*/
        sine_table[i] = (int16_t)(sinf(angle) * 2047.0f); /*Making the signal go from -2047 to 2047 since resolution is 4095*/
    }
}

/* Timer 6 configuration*/
static void tim6_init(void)
{
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure = {0};

    RCC_HB1PeriphClockCmd(RCC_HB1Periph_TIM6, ENABLE); /*Enable TIM6's clock*/

    TIM_TimeBaseStructure.TIM_Prescaler     = (SystemCoreClock / 1000000UL) - 1; /*Prescaling to 1MHz*/
    TIM_TimeBaseStructure.TIM_CounterMode   = TIM_CounterMode_Up; /*Count upward, then wrap to 0*/
    TIM_TimeBaseStructure.TIM_Period        = (1000000UL / SAMPLE_RATE_HZ) - 1; /*Fires the interrupt around 16k per second*/
    TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1; /*No extra clk division*/
    TIM_TimeBaseInit(TIM6, &TIM_TimeBaseStructure); /*Update settings*/

    TIM_ITConfig(TIM6, TIM_IT_Update, ENABLE); /*Enable the "update" interrupt*/
    NVIC_EnableIRQ(TIM6_IRQn); /*Tell the controller to call TIM6_IRQHandle*/
    TIM_Cmd(TIM6, ENABLE); /*Start the timer counting*/
} 

/* Generates the final sample
 *  On every tick it advances the volume
 * envelope, reads the next point of the sine wave, scales it by the
 * envelope, and writes the result to the DAC.*/
void TIM6_IRQHandler(void) __attribute__((interrupt()));
void TIM6_IRQHandler(void)
{
    if (TIM_GetITStatus(TIM6, TIM_IT_Update) != RESET) { /*Interruption caused by "update" event?*/
        TIM_ClearITPendingBit(TIM6, TIM_IT_Update); /*Clear the interrupt flag*/

        /*Move env_level toward env_target by small steps each sample */
        if (env_level < env_target) {
            env_level += ENV_STEP;
            if (env_level > env_target) env_level = env_target;
        } else if (env_level > env_target) {
            if (env_level < ENV_STEP) env_level = 0;
            else env_level -= ENV_STEP;
            if (env_level < env_target) env_level = env_target;
        }

        uint16_t output;
        if (env_level == 0) { /*Volume is zero = silence*/
            output = 2048;
        } else {
            uint8_t idx    = (uint8_t)(phase_acc >> 24); /*Take the top 8 bits*/
            int16_t sample = sine_table[idx];
            sample = (int16_t)((int32_t)sample * env_level / ENV_MAX); /*Apply the envelope, volume scaling*/
            output = (uint16_t)(sample + 2048); /*Shift the centered wave*/
            phase_acc += phase_inc; /*Advance the phase for the next sample*/
        }

        DAC_SetChannel1Data(DAC_Align_12b_R, output); /*Write the final value to the DAC*/
    }
}

/*Initialise sound variables*/
static void tone_start(uint16_t freq_hz)
{
    if (freq_hz == 0) {
        env_target = 0;
        return;
    }
    phase_inc = (uint32_t)(((uint64_t)freq_hz << 32) / SAMPLE_RATE_HZ);
    if (env_level == 0) {
        phase_acc = 0;
    }
    env_target = ENV_MAX;
}

static void tone_stop(void)
{
    env_target = 0;
}

/*Playing each note of the track */
static void play_track(uint8_t index)
{
    if (index >= 10 || tracks[index].notes == NULL) {
        dual_printf("Track %u is empty.\r\n", index);
        return;
    }

    const Track *t = &tracks[index];
    dual_printf("\r\n>>> Playing track %u: \"%s\"\r\n", index, t->name);

    for (uint16_t i = 0; i < t->count; i++) {
        Note n = t->notes[i];

        int32_t d = ((int32_t)n.duration_ms * tempo_scale_pct) / 100;
        d += tempo_offset_ms;
        if (d < 10)   d = 10;
        if (d > 5000) d = 5000;

        uint16_t played_freq = apply_pitch(n.freq_hz);

        printf(" Note %2u/%u: %4u Hz (orig %4u) for %4u ms\r\n",
               i + 1, t->count, played_freq, n.freq_hz, (uint16_t)d);

        tone_start(played_freq);
        Delay_Ms((uint16_t)d);
        tone_stop();

        Delay_Ms(15);
    }

    dual_printf(">>> Done.\r\n\r\n");
}


static void print_menu(void)
{
    dual_printf("\r\n==== Sound Effect Generator ====\r\n");
    dual_printf("Send a digit to play:\r\n");
    for (uint8_t i = 0; i < 10; i++) {
        if (tracks[i].name)
            dual_printf("  %u : %s\r\n", i, tracks[i].name);
    }
    dual_printf("Tempo:  L slower, F faster, +/- scale, R reset\r\n");
    dual_printf("Pitch:  U up, D down half tone\r\n");
    dual_printf("Other:  m menu\r\n");
}

/*Main*/
int main(void)
{
    SystemInit();
    SystemAndCoreClockUpdate();
    Delay_Init();
    USART_Printf_Init(115200);
    USART1_RX_Enable();
    USART2_Init();
    Delay_Ms(300);

    sine_table_init();
    dac_init();
    tim6_init();

    print_menu();

    while (1) {
        uint8_t rx = 0;
        uint8_t got = 0;

        if (USART_GetFlagStatus(USART1, USART_FLAG_RXNE) != RESET) {
            rx = (uint8_t)USART_ReceiveData(USART1);
            got = 1;
        }
        else if (USART_GetFlagStatus(USART2, USART_FLAG_RXNE) != RESET) {
            rx = (uint8_t)USART_ReceiveData(USART2);
            got = 1;
        }

        if (!got) continue;

        if (rx >= '0' && rx <= '9') {
            play_track(rx - '0');
        }
        else if (rx == 'm' || rx == 'M') {
            print_menu();
        }
        else if (rx == 'l' || rx == 'L') {
            tempo_offset_ms += 20;
            dual_printf("Tempo offset: %+d ms\r\n", tempo_offset_ms);
        }
        else if (rx == 'f' || rx == 'F') {
            tempo_offset_ms -= 20;
            dual_printf("Tempo offset: %+d ms\r\n", tempo_offset_ms);
        }
        else if (rx == '+') {
            if (tempo_scale_pct < 400) tempo_scale_pct += 10;
            dual_printf("Tempo scale: %u%%\r\n", tempo_scale_pct);
        }
        else if (rx == '-') {
            if (tempo_scale_pct > 20) tempo_scale_pct -= 10;
            dual_printf("Tempo scale: %u%%\r\n", tempo_scale_pct);
        }
        else if (rx == 'u' || rx == 'U') {
            if (pitch_offset_semitones < 36) pitch_offset_semitones++;
            dual_printf("Pitch: %+d semitones\r\n", pitch_offset_semitones);
        }
        else if (rx == 'd' || rx == 'D') {
            if (pitch_offset_semitones > -24) pitch_offset_semitones--;
            dual_printf("Pitch: %+d semitones\r\n", pitch_offset_semitones);
        }
        else if (rx == 'r' || rx == 'R') {
            tempo_offset_ms        = 0;
            tempo_scale_pct        = 100;
            pitch_offset_semitones = 0;
            dual_printf("All settings reset.\r\n");
        }
        else if (rx == '\r' || rx == '\n') {
            
        }
        else {
            dual_printf("Unknown: '%c'\r\n", rx);
        }
    }
}