// 1. Pin Definitions
const int EW_GREEN  = 17;
const int EW_YELLOW = 6;
const int EW_RED    = 10;
const int NS_GREEN  = 11;
const int NS_YELLOW = 12;
const int NS_RED    = 13;
const int PED_BUTTON =2;

// 2. FSM States
enum TrafficState {
  NS_GREEN_ST,
  NS_YELLOW_ST,
  EW_GREEN_ST,
  EW_YELLOW_ST,
  PEDESTRIAN_CROSSING_ST
};

TrafficState currentState = NS_GREEN_ST;

// 3. Timing Variables (Replacing the hardware timer)
unsigned long previousMillis = 0;
unsigned long interval = 0; // Starts at 0 so it triggers immediately on boot

// 4. Interrupt Variables for the Button
volatile bool pedestrian_waiting = false;
volatile unsigned long lastDebounceTime = 0; 
const unsigned long debounceDelay = 200; // 200ms software debounce

void setup() {
  // Setup output pins
  pinMode(NS_GREEN, OUTPUT);
  pinMode(NS_YELLOW, OUTPUT);
  pinMode(NS_RED, OUTPUT);
  pinMode(EW_GREEN, OUTPUT);
  pinMode(EW_YELLOW, OUTPUT);
  pinMode(EW_RED, OUTPUT);

  
  pinMode(PED_BUTTON, INPUT_PULLUP);

  // Attach the standard Arduino external interrupt
  attachInterrupt(digitalPinToInterrupt(PED_BUTTON), buttonISR, FALLING);
}

// Interrupt Service Routine for the button
void buttonISR() {
  if (millis() - lastDebounceTime > debounceDelay) {
    pedestrian_waiting = true;
    lastDebounceTime = millis();
  }
}

// Helper function to make the main loop cleaner
void setLights(bool nsG, bool nsY, bool nsR, bool ewG, bool ewY, bool ewR) {
  digitalWrite(NS_GREEN, nsG);
  digitalWrite(NS_YELLOW, nsY);
  digitalWrite(NS_RED, nsR);
  digitalWrite(EW_GREEN, ewG);
  digitalWrite(EW_YELLOW, ewY);
  digitalWrite(EW_RED, ewR);
}

void loop() {
  // SOFTWARE POLLING: Constantly check the current time
  unsigned long currentMillis = millis();

  // If the required interval has passed, execute the state machine
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis; // Reset the clock for the next state

    switch (currentState) {
      
      case NS_GREEN_ST:
        setLights(HIGH, LOW, LOW, LOW, LOW, HIGH); // NS Green, EW Red
        interval = 3000; // 3 seconds
        currentState = NS_YELLOW_ST;
        break;

      case NS_YELLOW_ST:
        setLights(LOW, HIGH, LOW, LOW, LOW, HIGH); // NS Yellow, EW Red
        interval = 1000; // 1 second
        
        // Safe routing check
        if (pedestrian_waiting) currentState = PEDESTRIAN_CROSSING_ST;
        else currentState = EW_GREEN_ST;
        break;

      case EW_GREEN_ST:
        setLights(LOW, LOW, HIGH, HIGH, LOW, LOW); // NS Red, EW Green
        interval = 3000;
        currentState = EW_YELLOW_ST;
        break;

      case EW_YELLOW_ST:
        setLights(LOW, LOW, HIGH, LOW, HIGH, LOW); // NS Red, EW Yellow
        interval = 1000;
        
        if (pedestrian_waiting) currentState = PEDESTRIAN_CROSSING_ST;
        else currentState = NS_GREEN_ST;
        break;

      case PEDESTRIAN_CROSSING_ST:
        setLights(LOW, LOW, HIGH, LOW, LOW, HIGH); // Both Red
        interval = 5000; // 5 seconds
        
        pedestrian_waiting = false; // Fulfill request
        currentState = NS_GREEN_ST;
        break;
    }
  }
}
