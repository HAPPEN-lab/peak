#ifndef PHYSICAL_CONTROLS_H
#define PHYSICAL_CONTROLS_H

#include <Arduino.h>

struct ButtonEvent {
	int navButton;            // 0-3 for the four screen buttons, -1 when none
	bool encoderShortPress;   // true once when encoder button is released before long-press threshold
	bool encoderLongPress;    // true once when encoder button is held past threshold
	bool encoderHeld;         // true while encoder button is currently held down
};

// Initialize the physical controls (buttons, encoder)
void initializeControls();

// Read debounced button states; distinguishes encoder short vs long press
ButtonEvent updateButtons();

// Read rotary encoder movement; returns -1, 0, or +1 per detent
int readRotaryDelta();
#endif // PHYSICAL_CONTROLS_H