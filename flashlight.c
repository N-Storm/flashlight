/*
 * File:   flashlight.c
 * Author: NStorm
 */

// CONFIG
#pragma config WDTE = OFF       // Watchdog Timer (WDT disabled)
#pragma config CP = OFF         // Code Protect (Code protection off)
#pragma config MCLRE = OFF      // Master Clear Enable (GP3/MCLR pin fuction is digital I/O, MCLR internally tied to VDD)

#define OUT_WHITE 0 // GP0
#define OUT_RED 1 // GP1
#define OUT_XML 2 // GP2
#define IN_BTN 3 // GP3
#define IN_BTN_GP GP3

//Timer0 Registers Prescaler = 64 - TMR0 Preset = 230 - Freq = 600.96 Hz - Period = 0.001664 seconds
// Soft PWM preload values for timer, affects PWM frequency
#define TMR0_PRELOAD 230
#define TMR0_CNT 255 - TMR0_PRELOAD
// These values are for PWM modes duty cycle. While one are used as HIGH level period other are used low for LOW mode
// and vice-versa for othermode, resulting in 33% / 66% duty cycle for LOW / MED modes.
#define TMR0_PWM_VAL0 (const uint8_t)(255 - ((TMR0_CNT) * 0.34))
#define TMR0_PWM_VAL1 (const uint8_t)(255 - ((TMR0_CNT) * 0.66))

// Delays for buttons
// Longs press equals 0.6s (cnt divided by one timer cycle period)
#define BTN_BUF_DELAY_CNT (const uint16_t)(0.6/0.001664)
#define BTN_BUF_DEBOUNCE_CNT 10

#include <xc.h>
#include <stdint.h>        /* For uint8_t definition */
#include <stdbool.h>       /* For true/false definition */
#include <pic10f200.h>

#define _XTAL_FREQ 4000000 // 4 MHz INTOSC

typedef enum {bounce, short_press, long_press} buttons_t;
typedef enum {power_on, sleep, wake, main_high, main_med, main_low, aux_white, aux_red} state_t;

typedef struct {
    state_t main;
    state_t aux;
} settings_t;

// Global vars
state_t state;
settings_t settings  __at(0x1A);
buttons_t btn = bounce;

inline void init() {
    OPTION = ~(nGPWU | T0CS | PSA | PS1); // GPWU (wake-up on GP change) enabled, T0 = internal clock, Fosc/4, Prescaler assigned to TMR0, 1:64 prescaler
    if (!GPWUF && (nPD || __powerdown)) { // 1st power on (i.e. not waking from sleep mode)
        GPIO = (1 << OUT_WHITE) | (1 << OUT_RED) | (1 << OUT_XML); // Start with every output at HIGH
        state = power_on;
    } else
        state = wake;
    TRIS = (1 << IN_BTN); // IN_BTN = INPUT, reset = OUTPUT
}

// Prepartion before going to SLEEP mode
void enter_sleep() {
    GPIO = 0xFF; // Turn off any GPIO
    state = sleep;    
    if (GPIO); // Dummy-read GPIO before going to sleep
    GPWUF = 0; // Reset the GPWUF flag
    while (true)
        SLEEP();
}

void timed_step() {
    static uint16_t btn_buf;

    TMR0 = TMR0_PWM_VAL0; // preset for timer register
    if (state == main_low)
        GPIO = 0xFF;
    else if (state == main_med)
        GPIO = ~(1 << OUT_XML);
    while (TMR0 >= TMR0_PWM_VAL0);

    TMR0 = TMR0_PWM_VAL1;
    if (state == main_low)
        GPIO = ~(1 << OUT_XML);
    else if (state == main_med)
        GPIO = 0xFF;

    if (!IN_BTN_GP) {
        if (btn_buf < BTN_BUF_DELAY_CNT)
            btn_buf++;
    } else {
        if (btn_buf <= BTN_BUF_DEBOUNCE_CNT)
            btn = bounce;
        else if (btn_buf >= BTN_BUF_DELAY_CNT)
            btn = long_press;
        else
            btn = short_press;
        btn_buf = 0; // reset counter when button are not pressed
    }

    while (TMR0 >= TMR0_PWM_VAL1);
}

void fsm_routine() {
    switch (state) {
        case power_on:
            settings.aux = aux_white;
            settings.main = main_low;
            if (btn == short_press)
                state = main_low;
            else if (btn == long_press)
                state = aux_white;
            break;
        case wake:
            if (btn == short_press)
                state = settings.main;
            else if (btn == long_press)
                state = settings.aux;
            break;
        case main_low:
            if (btn == short_press)
                state = sleep;
            else if (btn == long_press) {
                state = main_med;
                settings.main = state;
            }
            break;
        case main_med:
            if (btn == short_press)
                state = sleep;
            else if (btn == long_press) {
                state = main_high;
                settings.main = state;
            }
            break;
        case main_high:
            GPIO = ~(1 << OUT_XML);
            if (btn == short_press)
                state = sleep;
            else if (btn == long_press) {
                state = main_low;
                settings.main = state;
            }
            break;
        case aux_white:
            GPIO = ~(1 << OUT_WHITE);
            if (btn == short_press)
                state = sleep;
            else if (btn == long_press) {
                state = aux_red;
                settings.aux = state;
            }
            break;
        case aux_red:
            GPIO = ~(1 << OUT_RED);
            if (btn == short_press)
                state = sleep;
            else if (btn == long_press) {
                state = aux_white;
                settings.aux = state;
            }
            break;
        case sleep:
            enter_sleep();
            break;
    }
}

void main(void) {
    init();

    while (true) { // main loop
        timed_step();
        fsm_routine();
    }
}
