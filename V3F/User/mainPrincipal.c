#include "debug.h"
#include "hardware.h"
#include <string.h>
#include <math.h>

/* Tempo and tone controls (adjustable at runtime via terminal) */
static int16_t  tempo_offset_ms = 0;     /* added to each note's duration */
static uint16_t tempo_scale_pct = 100;   /* percentage: 100 = normal speed */
static int8_t pitch_offset_semitones = 0;   /* +1 = up half tone, -1 = down half tone */

/* Configuration */
#define SIMULATION_MODE   0     /* 1 = printf only, 0 = drive buzzer */


/* We prescale TIM1's clock down to 1 MHz so ARR maps directly to period in ?s:
 *   freq_hz = 1,000,000 / (ARR + 1)*/
#define TIM1_TARGET_CLK_HZ   1000000UL /* Timer clock setup.

/* Buzzer connected to PA8*/
#define BUZZER_PIN        GPIO_Pin_8
#define BUZZER_PORT       GPIOA

/* Note table (frequencies in Hz)*/
typedef enum {
    NOTE_REST = 0,
    NOTE_C4 = 262, NOTE_D4 = 294, NOTE_E4 = 330, NOTE_F4 = 349,
    NOTE_G4 = 392, NOTE_A4 = 440, NOTE_AS4 = 466, NOTE_B4 = 494,
    NOTE_C5 = 523, NOTE_D5 = 587, NOTE_E5 = 659, NOTE_F5 = 698,
    NOTE_G5 = 784, NOTE_A5 = 880, NOTE_B5 = 988,
    NOTE_C6 = 1047
} Note_Freq;


typedef struct {
    uint16_t freq_hz;       /* 0 = rest (silence) */
    uint16_t duration_ms;
} Note;

/* Melody / effect definitions */
static const Note melody_scale[] = {
    {NOTE_C4, 200}, {NOTE_D4, 200}, {NOTE_E4, 200}, {NOTE_F4, 200},
    {NOTE_G4, 200}, {NOTE_A4, 200}, {NOTE_B4, 200}, {NOTE_C5, 400},
};

static const Note melody_arpeggio[] = {
    {NOTE_C5, 120}, {NOTE_E5, 120}, {NOTE_G5, 120}, {NOTE_C6, 240},
};

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

/* Game of Thrones Theme*/
static const Note melody_got[] = {
    {NOTE_A4, 400}, {NOTE_D4, 400}, {NOTE_F4, 100}, {NOTE_G4, 100},
    {NOTE_A4, 400}, {NOTE_D4, 400}, {NOTE_F4, 100}, {NOTE_G4, 100},
    {NOTE_A4, 400}, {NOTE_D4, 400}, {NOTE_F4, 100}, {NOTE_G4, 100},
    {NOTE_E4, 540},
};

/* Pairs a melody pointer with its length, indexed by digit '0'..'9' */
typedef struct {
    const char  *name;
    const Note  *notes;
    uint16_t     count;
} Track;

static const Track tracks[10] = {
    [0] = { "Simple beep",        effect_beep,         sizeof(effect_beep)/sizeof(Note) },
    [1] = { "Double beep",        effect_double_beep,  sizeof(effect_double_beep)/sizeof(Note) },
    [2] = { "Alarm",              effect_alarm,        sizeof(effect_alarm)/sizeof(Note) },
    [3] = { "Power up",           effect_power_up,     sizeof(effect_power_up)/sizeof(Note) },
    [4] = { "Power down",         effect_power_down,   sizeof(effect_power_down)/sizeof(Note) },
    [5] = { "Coin",               effect_coin,         sizeof(effect_coin)/sizeof(Note) },
    [6] = { "Scale (C major)",    melody_scale,        sizeof(melody_scale)/sizeof(Note) },
    [7] = { "Arpeggio",           melody_arpeggio,     sizeof(melody_arpeggio)/sizeof(Note) },
    [8] = { "Happy Birthday",     melody_happy_birthday, sizeof(melody_happy_birthday)/sizeof(Note) },
    [9] = { "Game of Thrones",    melody_got,            sizeof(melody_got)/sizeof(Note) },
};

/* One-time timer setup, called once from main() */
static void buzzer_hw_init(void)
{
    /*Define initial values in structures */
    GPIO_InitTypeDef         GPIO_InitStructure   = {0}; 
    TIM_TimeBaseInitTypeDef  TIM_TimeBaseStructure = {0};
    TIM_OCInitTypeDef        TIM_OCInitStructure   = {0};

    /* Enable peripheral clocks */
    /*alternate-function I/O controller, the port that owns PA8 and the timer that will generate the PWM*/
    RCC_HB2PeriphClockCmd(RCC_HB2Periph_AFIO | RCC_HB2Periph_GPIOA | RCC_HB2Periph_TIM1, ENABLE); 

    /* PA8 as TIM1_CH1 (AF1) */
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource8, GPIO_AF1); 
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_8;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_Very_High; /*To have exact square pulses*/
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF_PP; /*Square waves need Push-Pull. The pin should drive both high and low*/
    GPIO_Init(GPIOA, &GPIO_InitStructure); /* Write the GPIO_InitStructure fields into the actual GPIOA registers. */

    /* Timer base: prescale to 1 MHz tick */
    TIM_TimeBaseStructure.TIM_Prescaler     = (SystemCoreClock / TIM1_TARGET_CLK_HZ) - 1; /*Set the PSC such that f = 1MHz*/
    TIM_TimeBaseStructure.TIM_CounterMode   = TIM_CounterMode_Up; /*Count from 0 on*/
    TIM_TimeBaseStructure.TIM_Period        = 1000 - 1;      /*initial period, updated per note */
    TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1; /*No division*/
    TIM_TimeBaseStructure.TIM_RepetitionCounter = 0; /*No repetition*/
    TIM_TimeBaseInit(TIM1, &TIM_TimeBaseStructure); /* Apply the time-base settings to TIM1's registers */

    /* Output Compare PWM channel 1, 50% duty */
    TIM_OCInitStructure.TIM_OCMode      = TIM_OCMode_PWM1; /* PWM mode 1: output is HIGH while counter < CCR, LOW after */
    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable; /* Enable the output for this channel */
    TIM_OCInitStructure.TIM_Pulse       = 500; /* 50% duty cycle */
    TIM_OCInitStructure.TIM_OCPolarity  = TIM_OCPolarity_High; /*Idle level when output is "off". active output when High.*/
    TIM_OC1Init(TIM1, &TIM_OCInitStructure); /* Apply this struct to TIM1's channel 1 */
    TIM_OC1PreloadConfig(TIM1, TIM_OCPreload_Enable); /*A new value is not loaded inmediately but at the next counter*/
    TIM_ARRPreloadConfig(TIM1, ENABLE); /*A new value is not loaded inmediately but at the next counter*/

    /* Keep main output OFF until a note starts */
    TIM_CtrlPWMOutputs(TIM1, DISABLE);
    TIM_Cmd(TIM1, ENABLE); /* Start the counter running */
}

/* Returns the input frequency shifted by `pitch_offset_semitones` half-steps.
 * Rests (freq 0) stay as rests. */
static uint16_t apply_pitch(uint16_t freq_hz)
{
    if (freq_hz == 0) return 0;
    if (pitch_offset_semitones == 0) return freq_hz;

    /* 2^(1/12) = 1.0594631...  raised to the offset */
    float factor = powf(1.05946309436f, (float)pitch_offset_semitones);
    float shifted = (float)freq_hz * factor;

    /* Clamp to a sensible audible range */
    if (shifted < 30.0f)    shifted = 30.0f;
    if (shifted > 20000.0f) shifted = 20000.0f;
    return (uint16_t)(shifted + 0.5f);   /* round to nearest */
}

static void tone_start(uint16_t freq_hz)
{
#if SIMULATION_MODE
    if (freq_hz == 0) printf("  [tone] -- silence --\r\n");
    else              printf("  [tone] %4u Hz ON\r\n", freq_hz);
#else
    if (freq_hz == 0) {
        TIM_CtrlPWMOutputs(TIM1, DISABLE);   /* rest = silence */
        return;
    }
    uint32_t arr = (TIM1_TARGET_CLK_HZ / freq_hz) - 1; /*Compute the auto-reload value*/
    TIM_SetAutoreload(TIM1, arr); /*Set it in the register*/
    TIM_SetCompare1(TIM1, (arr + 1) / 2);    /* Setting CCR to 50% duty */
    TIM_CtrlPWMOutputs(TIM1, ENABLE);
#endif
}

static void tone_stop(void)
{
#if SIMULATION_MODE
    printf("  [tone] OFF\r\n");
#else
    TIM_CtrlPWMOutputs(TIM1, DISABLE);
#endif
}

/* ============================================================
 *  Play a sequence of notes
 * ============================================================ */
static void play_track(uint8_t index)
{
    if (index >= 10 || tracks[index].notes == NULL) {
        printf("Track %u is empty.\r\n", index);
        return;
    }

    const Track *t = &tracks[index]; /*Retrieving the track of the parameter*/
    printf("\r\n>>> Playing track %u: \"%s\" (%u notes)\r\n", index, t->name, t->count);

    for (uint16_t i = 0; i < t->count; i++) {
        Note n = t->notes[i]; /*One note at a time*/
        //printf(" Note %2u/%u: %4u Hz for %4u ms\r\n",
        //       i + 1, t->count, n.freq_hz, n.duration_ms);

        /* Apply scale first, then offset. Clamp to a sane range. */
        int32_t d = ((int32_t)n.duration_ms * tempo_scale_pct) / 100;
        d += tempo_offset_ms;
        if (d < 10)   d = 10;       /* minimum audible note */
        if (d > 5000) d = 5000;     /* safety cap */

        printf(" Note %2u/%u: %4u Hz (orig %4u) for %4u ms\r\n",
        i + 1, t->count, apply_pitch(n.freq_hz), n.freq_hz, (uint16_t)d);

        tone_start(apply_pitch(n.freq_hz));
        Delay_Ms((uint16_t)d);
        tone_stop();

        /* tiny gap between notes so repeated same-pitch notes are distinguishable */
        Delay_Ms(15);
    }

    printf(">>> Done.\r\n\r\n");
}

/* ============================================================
 *  UI
 * ============================================================ */
static void print_menu(void)
{
    printf("\r\n==== Sound Effect Generator ====\r\n");
    printf("Mode: %s\r\n", SIMULATION_MODE ? "SIMULATION (no buzzer)" : "HARDWARE");
    printf("Send a digit to play:\r\n");
    for (uint8_t i = 0; i < 10; i++) {
        if (tracks[i].name)
            printf("  %u : %s\r\n", i, tracks[i].name);
    }
    printf("  m : show this menu again\r\n");
    printf("Tempo:\r\n");
    printf("  L : slower (+20 ms per note)\r\n");
    printf("  F : faster (-20 ms per note)\r\n");
    printf("  + : scale x1.1 (slower)\r\n");
    printf("  - : scale x0.9 (faster)\r\n");
    printf("  R : reset tempo\r\n");
    printf("Pitch:\r\n");
    printf("  U : up   half tone\r\n");
    printf("  D : down half tone\r\n");
    printf("================================\r\n");
}

/* Add PA10 as USART1 RX and re-enable USART1 in TX+RX mode */
void USART1_RX_Enable(void)
{
    /*Define initial values in structures */
    GPIO_InitTypeDef  GPIO_InitStructure  = {0};
    USART_InitTypeDef USART_InitStructure = {0};

    /* Clocks were already enabled by USART_Printf_Init, but are enabled again to avoid problems*/
    /* alternate-function I/O controller, UART and the port that owns PA9 (TX) and PA10 (RX)*/
    RCC_HB2PeriphClockCmd(RCC_HB2Periph_AFIO | RCC_HB2Periph_USART1 | RCC_HB2Periph_GPIOA, ENABLE);

    /* Route PA10 to USART1 by choosing the alternate function 7 (same AF7 as PA9) */
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource10, GPIO_AF7);

    /* PA10 configurednas digital input for USART1_RX */
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_10;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_Very_High;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_IN_FLOATING; /*No internal pull-up or pull-down*/  
    GPIO_Init(GPIOA, &GPIO_InitStructure); /* Write these fields into GPIOA's registers. */

    /* Re-init USART1 with BOTH Tx and Rx enabled (Printf_Init set Tx only) */
    USART_InitStructure.USART_BaudRate            = 115200;
    USART_InitStructure.USART_WordLength          = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits            = USART_StopBits_1;
    USART_InitStructure.USART_Parity              = USART_Parity_No; /* No parity bit */
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None; /* No RequestToSend/ClearToSend flow-control lines */
    USART_InitStructure.USART_Mode                = USART_Mode_Tx | USART_Mode_Rx; /* Enable both TX and RX */
    USART_Init(USART1, &USART_InitStructure); /* Write all those settings into USART1's hardware registers. */
    USART_Cmd(USART1, ENABLE); /* Master enable for the USART*/
}



/* ============================================================
 *  main
 * ============================================================ */
int main(void)
{
    uint8_t rx;

    /*Initialize variables and peripherals*/

    SystemInit();
    SystemAndCoreClockUpdate();
    Delay_Init();
    USART_Printf_Init(115200);
    USART1_RX_Enable();
    buzzer_hw_init();
    Delay_Ms(300);

    print_menu();

    while (1) {
        if (USART_GetFlagStatus(USART1, USART_FLAG_RXNE) != RESET) {
            rx = (uint8_t)USART_ReceiveData(USART1); /*Takes the data that comes from the RX port of the UART*/

            if (rx >= '0' && rx <= '9') {
                play_track(rx - '0');
            } else if (rx == 'm' || rx == 'M') {
                print_menu(); /*Show menu*/
            } else if (rx == '\r' || rx == '\n') {
                /*Ignore line endings*/
            }else if (rx == 'l' || rx == 'L') {
                tempo_offset_ms += 20; /*Make the music 20ms slower*/
                printf("Tempo offset: %+d ms per note\r\n", tempo_offset_ms);
            }
            else if (rx == 'f' || rx == 'F') {
                tempo_offset_ms -= 20; /*Make the music 20ms faster*/
                printf("Tempo offset: %+d ms per note\r\n", tempo_offset_ms);
            }
            else if (rx == '+') {
                if (tempo_scale_pct < 400) tempo_scale_pct += 10; /*Make the music proportionaly slower*/
                printf("Tempo scale: %u%% (higher = slower)\r\n", tempo_scale_pct);
            }
            else if (rx == '-') {
            if (tempo_scale_pct > 20) tempo_scale_pct -= 10; /*Make the music proportionaly faster*/
            printf("Tempo scale: %u%% (higher = slower)\r\n", tempo_scale_pct);
            }
            else if (rx == 'r' || rx == 'R') {
                tempo_offset_ms = 0; /*Reset variables*/
                tempo_scale_pct = 100;
                pitch_offset_semitones = 0;
                printf("All settings reset.\r\n");
            }
            else if (rx == 'u' || rx == 'U') {
                if (pitch_offset_semitones < 36) pitch_offset_semitones++;
                printf("Pitch: %+d semitones (%s)\r\n",
                pitch_offset_semitones,
                pitch_offset_semitones == 0 ? "original" :
                pitch_offset_semitones  > 0 ? "higher" : "lower"); /*Play music a semitone higher*/
            }
            else if (rx == 'd' || rx == 'D') {
                if (pitch_offset_semitones > -24) pitch_offset_semitones--;
                printf("Pitch: %+d semitones (%s)\r\n",
                pitch_offset_semitones,
                pitch_offset_semitones == 0 ? "original" :
                pitch_offset_semitones  > 0 ? "higher" : "lower"); /*Play music a semitone higher*/
            } 
            else {
                printf("Unknown command: '%c' (0x%02X). Send 'm' for menu.\r\n", rx, rx);
            }
        }
    }
}