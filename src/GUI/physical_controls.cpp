#include "physical_controls.h"

// Buttons
const int buttonPins[5] = {41, 42, 2, 1, 38};
const char* buttonNames[5] = {"Button 1", "Button 2", "Button 3", "Button 4", "Encoder Button"};
int lastStates[5]; // store button states
int lastButtonStates[5]; // for debouncing
unsigned long pressStart[5] = {0};
bool longReported[5] = {false};
unsigned long lastDebounceTime[5] = {0}; // debounce timing for each button
const unsigned long debounceDelay = 50; // debounce time in ms
const unsigned long longPressThresholdMs = 700;

// Rotary encoder
const int encoderClk = 40; // CLK
const int encoderDt = 39; // DT
int encoderState = 0;
int lastDtState = 0;


void initializeControls() {
    // Setup buttons
    for (int i = 0; i < 5; i++) {
        pinMode(buttonPins[i], INPUT_PULLUP);
        lastStates[i] = HIGH; // not pressed
        lastButtonStates[i] = HIGH;
        pressStart[i] = 0;
        longReported[i] = false;
    }

    // Setup rotary encoder
    pinMode(encoderClk, INPUT_PULLUP);
    pinMode(encoderDt, INPUT_PULLUP);
    encoderState = digitalRead(encoderClk);
    lastDtState = digitalRead(encoderDt);
}

ButtonEvent updateButtons() {
    ButtonEvent evt{.navButton = -1, .encoderShortPress = false, .encoderLongPress = false, .encoderHeld = false};

    // --- Handle push buttons with non-blocking debounce ---
    for (int i = 0; i < 5; i++) {
        const int reading = digitalRead(buttonPins[i]);

        // Check if state changed (reset debounce timer)
        if (reading != lastButtonStates[i]) {
            lastDebounceTime[i] = millis();
        }

        const unsigned long stableFor = millis() - lastDebounceTime[i];

        // Check if enough time has passed and state is stable
        if (stableFor > debounceDelay) {
            // If the stable state is different from last confirmed state
            if (reading != lastStates[i]) {
                lastStates[i] = reading;

                // If button was just pressed (went to LOW)
                if (reading == LOW) {
                    pressStart[i] = millis();
                    longReported[i] = false;

                    // Treat buttons 0-3 as navigation inputs
                    if (i >= 0 && i <= 3) {
                        evt.navButton = i;
                        Serial.println(buttonNames[i]);
                    }
                } else { // button released
                    const unsigned long held = millis() - pressStart[i];
                    if (i == 4 && !longReported[i]) {
                        if (held >= debounceDelay) {
                            evt.encoderShortPress = true;
                        }
                    }
                }
            }

            // Long-press detection for encoder button while held
            if (i == 4 && lastStates[i] == LOW && !longReported[i]) {
                const unsigned long held = millis() - pressStart[i];
                if (held >= longPressThresholdMs) {
                    evt.encoderLongPress = true;
                    longReported[i] = true;
                }
            }
        }

        lastButtonStates[i] = reading;
    }

    // Report live held state for encoder button
    evt.encoderHeld = (lastStates[4] == LOW);

    return evt;
}

int readRotaryDelta() {
    int delta = 0;
    const int clkState = digitalRead(encoderClk);
    const int dtState = digitalRead(encoderDt);

    // Check if encoder CLK has changed (falling edge)
    if (clkState != encoderState && clkState == LOW) {
        // Read DT pin to determine direction
        if (dtState != clkState) {
            delta = -1; // Counter-Clockwise now negative
        } else {
            delta = 1; // Clockwise now positive
        }
    }

    encoderState = clkState;
    lastDtState = dtState;

    return delta;
}