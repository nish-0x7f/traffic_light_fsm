#include <avr/io.h>
#include <avr/interrupt.h>

// =============================================================
// PIN MAPPING (Port B: D8-D13, Port D: D2)
// PB0=D8=EW_GREEN, PB1=D9=EW_YELLOW, PB2=D10=EW_RED
// PB3=D11=NS_GREEN, PB4=D12=NS_YELLOW, PB5=D13=NS_RED
// PD2=D2=BUTTON (INT0)
// =============================================================

typedef enum {
    NS_GREEN_ST,
    NS_YELLOW_ST,
    EW_GREEN_ST,
    EW_YELLOW_ST,
    PEDESTRIAN_CROSSING_ST
} TrafficState;

volatile TrafficState currentState = NS_GREEN_ST;


#define TICKS_1_SEC 15625UL

volatile uint8_t  timer_event    = 1; // Start at 1 so FSM runs immediately on boot
volatile uint8_t  tick_count     = 0; // Software tick counter
volatile uint8_t  ticks_needed   = 3; // How many 1-sec ticks before state changes

// --- Button Variables ---
volatile bool pedestrian_flag    = false; // Raw flag set by ISR
volatile bool pedestrian_waiting = false; // Debounced, safe flag used by FSM

// Simple millisecond counter incremented by Timer0 (we configure it separately)
volatile uint32_t ms_counter = 0;
uint32_t lastDebounceTime    = 0;
#define DEBOUNCE_MS 200

// =============================================================
// HELPER: Set all 6 LEDs at once via a single PORTB write
// Bits: [5=NS_RED][4=NS_YELLOW][3=NS_GREEN][2=EW_RED][1=EW_YELLOW][0=EW_GREEN]
// =============================================================
inline void setLights(uint8_t pattern) {
    PORTB = (PORTB & 0xC0) | (pattern & 0x3F);
}

// Light patterns
#define LIGHTS_NS_GREEN  0b001001  // NS=Green,  EW=Red
#define LIGHTS_NS_YELLOW 0b010001  // NS=Yellow, EW=Red  
#define LIGHTS_EW_GREEN  0b100100  // NS=Red,    EW=Green
#define LIGHTS_EW_YELLOW 0b100010  // NS=Red,    EW=Yellow
#define LIGHTS_ALL_RED   0b100100  // NS=Red,    EW=Red  — pedestrian phase
//                         
//               [NS_RED|NS_YEL|NS_GRN|EW_RED|EW_YEL|EW_GRN]

void setup() {
    // 1. LED outputs: PB0–PB5 (D8–D13)
    DDRB |= 0x3F;
    PORTB &= ~0x3F; // All LEDs off initially

    // 2. Button input: PD2 with internal pull-up
    DDRD  &= ~(1 << DDD2);
    PORTD |=  (1 << PORTD2);

    // 3. External Interrupt INT0: falling edge (button press pulls LOW)
    EICRA |=  (1 << ISC01);
    EICRA &= ~(1 << ISC00);
    EIMSK |=  (1 << INT0);

    // 4. Timer0: used purely for millisecond counting (like Arduino's millis())
    //    CTC mode, 1024 prescaler → fires every 1.024ms ≈ close enough
    TCCR0A = (1 << WGM01);                     // CTC mode
    TCCR0B = (1 << CS02) | (1 << CS00);        // 1024 prescaler
    OCR0A  = 15;                                // 16MHz/1024/16 ≈ 976Hz ≈ 1ms
    TIMSK0 |= (1 << OCIE0A);

  .
    TCCR1A = 0;
    TCCR1B = 0;
    TCNT1  = 0;
    OCR1A  = TICKS_1_SEC;                       // Set BEFORE enabling interrupt
    TCCR1B |= (1 << WGM12);                     // CTC mode
    TCCR1B |= (1 << CS12) | (1 << CS10);       // 1024 prescaler
    TIMSK1 |= (1 << OCIE1A);

    sei();
}

// --- Timer0 ISR: millisecond counter ---
ISR(TIMER0_COMPA_vect) {
    ms_counter++;
}

// --- Timer1 ISR: 1-second tick for FSM ---
ISR(TIMER1_COMPA_vect) {
    tick_count++;
    if (tick_count >= ticks_needed) {
        tick_count   = 0;
        timer_event  = 1;
    }
}

// --- Button ISR: raw flag only, no millis() ---
ISR(INT0_vect) {
    pedestrian_flag = true;
}

void loop() {
    // --- Debounce in main loop (safe, no ISR timing issues) ---
    if (pedestrian_flag) {
        uint32_t now = ms_counter; // Atomic enough for 8-bit AVR at this rate
        if (now - lastDebounceTime > DEBOUNCE_MS) {
            pedestrian_waiting = true;
            lastDebounceTime   = now;
        }
        pedestrian_flag = false;
    }

    // --- FSM: only runs when timer_event fires ---
    if (timer_event) {
        timer_event = 0;

        switch (currentState) {

            case NS_GREEN_ST:
                setLights(LIGHTS_NS_GREEN);
                ticks_needed = 3;           // 3 seconds
                currentState = NS_YELLOW_ST;
                break;

            case NS_YELLOW_ST:
                setLights(LIGHTS_NS_YELLOW);
                ticks_needed = 1;           // 1 second
                if (pedestrian_waiting) currentState = PEDESTRIAN_CROSSING_ST;
                else                    currentState = EW_GREEN_ST;
                break;

            case EW_GREEN_ST:
                setLights(LIGHTS_EW_GREEN);
                ticks_needed = 3;
                currentState = EW_YELLOW_ST;
                break;

            case EW_YELLOW_ST:
                setLights(LIGHTS_EW_YELLOW);
                ticks_needed = 1;
                if (pedestrian_waiting) currentState = PEDESTRIAN_CROSSING_ST;
                else                    currentState = NS_GREEN_ST;
                break;

            case PEDESTRIAN_CROSSING_ST:
                setLights(LIGHTS_ALL_RED);
                ticks_needed       = 5;     // 5 seconds — no overflow possible now
                pedestrian_waiting = false;
                currentState       = NS_GREEN_ST;

                // Clear any ghost bounces accumulated during crossing
                EIFR  |= (1 << INTF0);
                break;
        }

        TCNT1     = 0; // Reset Timer1 for next interval
        tick_count = 0;
    }
}