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
// Generally, you should use "unsigned long" for variables that hold time
// The value will quickly become too large for an int to store
unsigned long previousMillis = 0;        // will store last time LED was updated

// constants won't change:
const long interval = 500;           // interval at which to blink (milliseconds)


Bounce * buttons = new Bounce[NUM_BUTTONS];

void setup() {

  Serial.begin(9600);
  for (int i = 0; i < NUM_BUTTONS; i++) {
    buttons[i].attach( BUTTON_PINS[i] , INPUT_PULLUP  );       //setup the bounce instance for the current button
    buttons[i].interval(25);              // interval in ms
  }

  // Setup the LED :
  pinMode(LED_PIN, OUTPUT);
  pinMode(8, OUTPUT);
  pinMode(9, OUTPUT);
  digitalWrite(LED_PIN, ledState);
  Serial.println();
  Serial.println("Starting");

}

void loop() {

  bool needToToggleLed = false;

  buttons[0].update();
  buttons[1].update();
  buttons[2].update();
  buttons[3].update();

  if (buttons[0].rose() ) { // limit switch 2nd floor
    Serial.println("Limit Switch 2nd floor");
    digitalWrite(8, LOW);
    delay(500);
    digitalWrite(10, LOW);   // Lamp 1
  }
  if (buttons[1].rose() ) { // limit switch 1st floor
    Serial.println("Limit Switch 1st floor");
    digitalWrite(9, LOW);
    delay(500);
    digitalWrite(11, LOW);   // Lamp 2
  }
  if (buttons[2].rose() ) { // Relay 1 up
    Serial.println("Up Button");
    delay(500);
    digitalWrite(9, LOW);
    delay(500);
    digitalWrite(8, HIGH);    
    digitalWrite(10, HIGH);   // Lamp 1
  }
  if (buttons[3].rose() ) { // Relay 2 down
    Serial.println("Down Button");
    delay(500);
    digitalWrite(8, LOW);
    delay(500);
    digitalWrite(9, HIGH); 
    digitalWrite(11, HIGH);   // Lamp 2
  }
  
  /*
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
  */
  blink();

}

void blink()
{
  unsigned long currentMillis = millis();

  if (currentMillis - previousMillis >= interval) {
    // save the last time you blinked the LED
    previousMillis = currentMillis;

    // if the LED is off turn it on and vice-versa:
    if (ledState == LOW) {
      ledState = HIGH;
    } else {
      ledState = LOW;
    }

    // set the LED with the ledState of the variable:
    digitalWrite(LED_PIN, ledState);
  }
}
