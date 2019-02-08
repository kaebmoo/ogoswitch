// Detect the falling edge of multiple buttons.
// Eight buttons with internal pullups.
// Toggles a LED when any button is pressed.
// Buttons on pins 2,3,4,5,6,7,8,9

// Include the Bounce2 library found here :
// https://github.com/thomasfredericks/Bounce2
#include <Bounce2.h>

#define LED_PIN 13

#define NUM_BUTTONS 4
const uint8_t BUTTON_PINS[NUM_BUTTONS] = {4, 5, 6, 7};
// 4 limit switch 2nd floor
// 5 limit switch 1st floor
// 6 up
// 7 down

int ledState = LOW;

Bounce * buttons = new Bounce[NUM_BUTTONS];

void setup() {

  for (int i = 0; i < NUM_BUTTONS; i++) {
    buttons[i].attach( BUTTON_PINS[i] , INPUT_PULLUP  );       //setup the bounce instance for the current button
    buttons[i].interval(25);              // interval in ms
  }

  // Setup the LED :
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, ledState);


}

void loop() {

  bool needToToggleLed = false;

  buttons[0]update();
  buttons[1]update();
  buttons[2]update();
  buttons[3]update();

  if (buttons[0].rose() ) { // limit switch 2nd floor
    digitalWrite(8, LOW);
  }
  if (buttons[1].rose() ) { // limit switch 1st floor
    digitalWrite(9, LOW);
  }
  if (buttons[2].rose() ) { // Relay 1 up
    digitalWrite(8, HIGH);
  }
  if (buttons[3].rose() ) { // Relay 2 down
    digitalWrite(9, HIGH);
  }
  
  
  for (int i = 0; i < NUM_BUTTONS; i++)  {
    // Update the Bounce instance :
    buttons[i].update();
    // If it fell, flag the need to toggle the LED
    if ( buttons[i].fell() ) {
      needToToggleLed = true;
    }
  }

  // if a LED toggle has been flagged :
  if ( needToToggleLed ) {
    // Toggle LED state :
    ledState = !ledState;
    digitalWrite(LED_PIN, ledState);
  }


}
