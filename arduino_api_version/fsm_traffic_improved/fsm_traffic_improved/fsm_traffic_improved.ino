// Improved version:
// - Fixed ISR design (no millis inside ISR)
// - Implemented safe software debounce in loop()
// - Separated hardware interrupt from application logic
const int PIN_EW_GREEN   = 17;
const int PIN_EW_YELLOW  = 6;
const int PIN_EW_RED     = 10;
const int PIN_NS_GREEN   = 11;
const int PIN_NS_YELLOW  = 12;
const int PIN_NS_RED     = 13;
const int PIN_PED_BUTTON = 2;

enum TrafficState {
    NS_GREEN_ST,
    NS_YELLOW_ST,
    EW_GREEN_ST,
    EW_YELLOW_ST,
    PEDESTRIAN_CROSSING_ST
};
TrafficState currentState = NS_GREEN_ST;

// 3. Timing
unsigned long previousMillis = 0;
unsigned long interval       = 0; // 0 so FSM triggers immediately on boot

// BUG FIX: ISR sets only a raw flag. Debounce happens in loop() where
// millis() is reliable. Never call millis() inside an ISR on AVR.
volatile bool pedestrian_flag    = false; // Raw ISR flag
         bool pedestrian_waiting = false; // Debounced flag consumed by FSM
unsigned long lastDebounceTime   = 0;
const unsigned long DEBOUNCE_MS  = 200;


void setLights(bool nsG, bool nsY, bool nsR,
               bool ewG, bool ewY, bool ewR) {
    digitalWrite(PIN_NS_GREEN,  nsG);
    digitalWrite(PIN_NS_YELLOW, nsY);
    digitalWrite(PIN_NS_RED,    nsR);
    digitalWrite(PIN_EW_GREEN,  ewG);
    digitalWrite(PIN_EW_YELLOW, ewY);
    digitalWrite(PIN_EW_RED,    ewR);
}

void buttonISR() {
    pedestrian_flag = true;
}

void setup() {
    pinMode(PIN_NS_GREEN,   OUTPUT);
    pinMode(PIN_NS_YELLOW,  OUTPUT);
    pinMode(PIN_NS_RED,     OUTPUT);
    pinMode(PIN_EW_GREEN,   OUTPUT);
    pinMode(PIN_EW_YELLOW,  OUTPUT);
    pinMode(PIN_EW_RED,     OUTPUT);

    pinMode(PIN_PED_BUTTON, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(PIN_PED_BUTTON), buttonISR, FALLING);
}

void loop() {
    unsigned long currentMillis = millis();

    if (pedestrian_flag) {
        pedestrian_flag = false; // Clear raw flag immediately
        if (currentMillis - lastDebounceTime > DEBOUNCE_MS) {
            pedestrian_waiting = true;
            lastDebounceTime   = currentMillis;
        }
    }

    if (currentMillis - previousMillis >= interval) {
        previousMillis = currentMillis;

        switch (currentState) {

            case NS_GREEN_ST:
                setLights(HIGH, LOW, LOW,   // NS: Green
                          LOW,  LOW, HIGH); // EW: Red
                interval     = 3000;
                currentState = NS_YELLOW_ST;
                break;

            case NS_YELLOW_ST:
                setLights(LOW, HIGH, LOW,   // NS: Yellow
                          LOW, LOW,  HIGH); // EW: Red
                interval = 1000;
                // Pedestrian check at the safe moment (end of yellow)
                if (pedestrian_waiting) currentState = PEDESTRIAN_CROSSING_ST;
                else                    currentState = EW_GREEN_ST;
                break;

            case EW_GREEN_ST:
                setLights(LOW, LOW, HIGH,   // NS: Red
                          HIGH, LOW, LOW);  // EW: Green
                interval     = 3000;
                currentState = EW_YELLOW_ST;
                break;

            case EW_YELLOW_ST:
                setLights(LOW, LOW,  HIGH,  // NS: Red
                          LOW, HIGH, LOW);  // EW: Yellow
                interval = 1000;
                if (pedestrian_waiting) currentState = PEDESTRIAN_CROSSING_ST;
                else                    currentState = NS_GREEN_ST;
                break;

            case PEDESTRIAN_CROSSING_ST:
                setLights(LOW, LOW, HIGH,   // NS: Red
                          LOW, LOW, HIGH);  // EW: Red — both stop
                interval = 5000;            // BUG FIX: millis()-based, no overflow
                pedestrian_waiting = false; // Request fulfilled
                currentState       = NS_GREEN_ST;
                break;
        }
    }
}

