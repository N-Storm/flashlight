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

//Timer0 Settings: Prescaler = 4, TMR0 count = 256 (no preload), Freq = 976.56 Hz, Period = 1024 us
// These values are for PWM duty cycle modes for main light. Duty cycle = RAW VALUE / 255.
#define TMR0_ADD 14 // Busy on "other things" compensation
#define TMR0_PWM_VAL0 25 + TMR0_ADD // ~10% for mode LOW
#define TMR0_PWM_VAL1 77 + TMR0_ADD // ~30% for mode MED
#define TMR0_PWM_VAL2 182 + TMR0_ADD // ~71% for mode HIGH

// Delays for buttons
// Longs press equals 0.6s (cnt divided by one timer cycle period)
#define BTN_BUF_DELAY_CNT (const uint16_t)(0.6*976.56)
#define BTN_BUF_DEBOUNCE_CNT 15

#include <xc.h>
#include <stdint.h>        /* For uint8_t definition */
#include <stdbool.h>       /* For true/false definition */
#include <pic10f200.h>

#define _XTAL_FREQ 4000000 // 4 MHz INTOSC

typedef enum { bounce, short_press, long_press } buttons_t;
typedef enum { power_on, sleep, wake, aux_white, aux_red, main_full, main_low = TMR0_PWM_VAL0, main_med = TMR0_PWM_VAL1, main_high = TMR0_PWM_VAL2 } state_t;

typedef struct {
    state_t main;
    state_t aux;
} settings_t;

// Global vars
state_t state;
settings_t settings __at(0x1A);
buttons_t btn = bounce;

inline void init() {
    OPTION = ~(nGPWU | T0CS | PSA | PS2 | PS1); // GPWU (wake-up on GP change) enabled, T0 = internal clock, Fosc/4, Prescaler assigned to TMR0, 1:4 prescaler
    if (!GPWUF && (nPD || __powerdown)) { // 1st power on (i.e. not waking from sleep mode)
        GPIO = (1 << OUT_WHITE) | (1 << OUT_RED) | (1 << OUT_XML); // Start with every output at HIGH
        state = power_on;
    } else
        state = wake;
    TRIS = (1 << IN_BTN); // IN_BTN = INPUT, rest = OUTPUT
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

    if (state >= main_low)
        GPIO = ~(1 << OUT_XML);

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

    if (state >= main_low) {
        while (TMR0 < state);
        GPIO = 0xFF;
        while (TMR0 >= state);
    } else
        while (TMR0 > 0);
}

void fsm_transit(state_t next_state) {
    if (btn == short_press)
        state = sleep;
    else if (btn == long_press) {
        state = next_state;
        if (state == aux_red || state == aux_white)
            settings.aux = state;
        else
            settings.main = state;
    }
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
            fsm_transit(main_med);
            break;
        case main_med:
            fsm_transit(main_high);
            break;
        case main_high:
            fsm_transit(main_full);
            break;
        case main_full:
            GPIO = ~(1 << OUT_XML);
            fsm_transit(main_low);
            break;
        case aux_white:
            GPIO = ~(1 << OUT_WHITE);
            fsm_transit(aux_red);
            break;
        case aux_red:
            GPIO = ~(1 << OUT_RED);
            fsm_transit(aux_white);
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
