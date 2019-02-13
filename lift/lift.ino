// Detect the falling edge of multiple buttons.
// Eight buttons with internal pullups.
// Toggles a LED when any button is pressed.
// Buttons on pins 2,3,4,5,6,7,8,9

// Include the Bounce2 library found here :
// https://github.com/thomasfredericks/Bounce2
#include <Bounce2.h>

#define LED_PIN 13

#define NUM_BUTTONS 5
const uint8_t BUTTON_PINS[NUM_BUTTONS] = {4, 5, 6, 7, 3};

// 4 limit switch 2nd floor
// 5 limit switch 1st floor
// 6 up
// 7 down
// 3 Emergency

int ledState = LOW;
// Generally, you should use "unsigned long" for variables that hold time
// The value will quickly become too large for an int to store
unsigned long previousMillis = 0;        // will store last time LED was updated

// constants won't change:
const long interval = 1000;           // interval at which to blink (milliseconds)


Bounce * buttons = new Bounce[NUM_BUTTONS];

#define RELAY1  8
#define RELAY2  9
#define LAMP1   10
#define LAMP2   11

bool limitUp = false;
bool limitDown = false;

void setup() {

  Serial.begin(9600);
  for (int i = 0; i < NUM_BUTTONS; i++) {
    buttons[i].attach( BUTTON_PINS[i] , INPUT_PULLUP  );       //setup the bounce instance for the current button
    buttons[i].interval(25);              // interval in ms
  }

  // Setup the LED :
  pinMode(LED_PIN, OUTPUT);
  pinMode(RELAY1, OUTPUT);
  pinMode(RELAY2, OUTPUT);
  pinMode(LAMP1, OUTPUT);
  pinMode(LAMP2, OUTPUT);
  digitalWrite(LED_PIN, ledState);
  Serial.println();
  Serial.println("Starting");
  // reset all relay
  digitalWrite(RELAY1, LOW);
  digitalWrite(RELAY2, LOW);
  digitalWrite(LAMP1, LOW);
  digitalWrite(LAMP2, LOW);
}

void loop() {

  bool needToToggleLed = false;
  int limitUpPin, limitDownPin;

  buttons[0].update();
  buttons[1].update();
  buttons[2].update();
  buttons[3].update();
  buttons[4].update();

  limitUpPin = digitalRead(BUTTON_PINS[0]);   // limit switch 2nd floor, Normal Open Switch - NO
  limitDownPin = digitalRead(BUTTON_PINS[1]); // limit switch 1st floor, Normal Open Switch - NO

  if (buttons[4].rose() ) {     // emergency switch
    digitalWrite(RELAY1, LOW);
    digitalWrite(RELAY2, LOW);
    digitalWrite(LAMP1, LOW);
    digitalWrite(LAMP2, LOW);
  }
  if (buttons[0].rose() || limitUpPin == HIGH) { // limit switch 2nd floor, turn off
    Serial.println("Limit Switch 2nd floor");
    if (limitUp != true) {
      digitalWrite(RELAY1, LOW);
      digitalWrite(RELAY2, LOW);
      delay(500);
      digitalWrite(LAMP1, LOW);   // Lamp 1
      limitUp = true;
      limitDown = false;
    }
  }
  if (buttons[1].rose() || limitDownPin == HIGH) { // limit switch 1st floor, turn off
    Serial.println("Limit Switch 1st floor");
    if (limitDown != true) {
      digitalWrite(RELAY1, LOW);
      digitalWrite(RELAY2, LOW);
      delay(500);
      digitalWrite(LAMP2, LOW);   // Lamp 2
      limitDown = true;
      limitUp = false;
    }

  }
  if (buttons[2].rose() ) { // Relay 1 up
    Serial.println("Up Button");
    delay(500);
    limitUpPin = digitalRead(BUTTON_PINS[0]); // prevent
    if (limitUpPin == LOW) {       // if HIGH lift position is limit
      digitalWrite(RELAY2, LOW);
      delay(3000);                 // delay for protect motor
      digitalWrite(RELAY1, HIGH);
      digitalWrite(LAMP1, HIGH);   // Lamp 1
      limitUp = true;
      limitDown = false;
    }
  }
  if (buttons[3].rose() ) { // Relay 2 down
    Serial.println("Down Button");
    delay(500);
    limitDownPin = digitalRead(BUTTON_PINS[1]);
    if (limitDownPin == LOW) {
      digitalWrite(RELAY1, LOW);
      delay(3000);                 // delay for protect motor
      digitalWrite(RELAY2, HIGH);
      digitalWrite(LAMP2, HIGH);   // Lamp 2
      limitUp = false;
      limitDown = true;
    }
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
