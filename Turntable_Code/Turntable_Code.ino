/*******************************************
   Photo_Turntable V1.0
   Colin J Smith
   cosm5801@colorado.edu
   Created 09/11/19


       Simple Stepper Motor Control Exaple Code
        by Dejan Nedelkovski, www.HowToMechatronics.com


       Debounce
        created 21 Nov 2006 by David A. Mellis
        modified 30 Aug 2011 by Limor Fried
        modified 28 Dec 2012 by Mike Walters
        modified 30 Aug 2016 by Arturo Guadalupi
        This example code is in the public domain.
        http://www.arduino.cc/en/Tutorial/Debounce


       ArduinoInteruptsExample (Timer.One.h library and interrupts)
       http://educ8s.tv/arduino-interrupts-tutorial/


       name:I2C LCD1602 example 1 (www.sunfounder.com)


       custom character play and pause: https://maxpromer.github.io/LCD-Character-Creator/


       A4988 Driver Pinout Diagram------ https://howtomechatronics.com/wp-content/uploads/2015/08/A4988-Wiring-Diagram.png
          see pinout guide and table below to hardwire to 5v

          ---MS1---MS2---MS3---Resolution---

          ---LOW---LOW---LOW---FULL STEP---
          ---HIGH--LOW---LOW---HALF STEP---
          ---LOW---HIGH--LOW---QUARTER STEP---
          ---HIGH--HIGH--LOW---EIGHTH STEP---
          ---HIGH--HIGH--HIGH--SIXTEENTH STEP---


********************************/
#include <multiCameraIrControl.h>
#include <LiquidCrystal_I2C.h>
#include <LiquidCrystal.h>
#include <ClickEncoder.h>
#include <TimerOne.h> //UPDATED library TimerOne
#include <Wire.h>

//*****DEBUGGING*****
bool debug = false;
//*****LCD*****
LiquidCrystal_I2C lcd(0x27, 16, 2); // set the LCD address to 0x27 for a 16 chars and 2 line display
String line1;
String line2;
//*****GENERAL*****
int mode = 0; // current machine operating state
String settings[] = {"Frames", "Degrees", "Period"}; // machine settings
int settings_vals_default[] = {10, 360, 2};
int settings_vals[] = {settings_vals_default[0], settings_vals_default[1], settings_vals_default[2]};
int setting = 0;
int frames_done = 0; // number of frames completed
int progress = 0; // percent of frames completed
volatile bool is_running = false; // varible to signify if the turntable is running
bool pause = false;
//*****START/STOP BUTTON*****
const int buttonPin = 13;    // the pin number of the red pushbutton
int buttonState;             // the current reading from the input pin
int lastButtonState = HIGH;   // the previous reading from the input pin
// the following variables are unsigned longs because the time, measured in
// milliseconds, will quickly become a bigger number than can be stored in an int.
unsigned long lastDebounceTime = 0;  // the last time the output pin was toggled
unsigned long debounceDelay = 50;    // the debounce time; increase if the output flickers
//*****MOTOR*****
const int HCSteps = 1;//16;
const float GenSpecStepAngle = 1.8;// in degrees from data sheet
const float FuncStepAngle = GenSpecStepAngle / HCSteps; //@1/16 = 0.1125 deg./ step
float degPerTurn = 360;
float microSteps = 0;
float gearRatio = 37.84; // 756 tooth big gear / 20 tooth little gear
//A4988 stepper driver pins
const int stepPin = 3;
const int dirPin = 2;
const int resetPin = 5;
const int sleepPin = 4;
const int ms1 = 8;
const int ms2 = 7;
const int ms3 = 6;
const int enablePin = 9;
//*****CLICK ENCODER*****
ClickEncoder *encoder;
ClickEncoder::Button b;
int enc_val = 0;
int enc_val_last = 0;
//******CAMERA******
Canon Eos7D(11);

void setup() {
  //******DEBUGGING*****
  Serial.begin(9600); //Open Serial connection for debugging
  //*****LCD SETUP*****
  lcd.init(); //initialize the lcd
  lcd.backlight(); //open the backlight
  //*****MOTOR PIN SETUP*****
  pinMode(stepPin, OUTPUT);
  pinMode(dirPin, OUTPUT);
  pinMode(resetPin, OUTPUT);
  pinMode(sleepPin, OUTPUT);
  pinMode(ms1, OUTPUT);
  pinMode(ms2, OUTPUT);
  pinMode(ms3, OUTPUT);
  pinMode(enablePin, OUTPUT);
  digitalWrite(resetPin, HIGH);
  digitalWrite(sleepPin, HIGH); // do not sleep
  digitalWrite(ms1, LOW);
  digitalWrite(ms2, LOW);
  digitalWrite(ms3, LOW);
  digitalWrite(enablePin, LOW); // enable the output pins
  //*****START/STOP BUTTON SETUP*****
  pinMode(buttonPin, INPUT_PULLUP);
  //*****ENCODER SETUP*****
  encoder = new ClickEncoder(A1, A0, A2, 4);
  encoder->setAccelerationEnabled(true);
  Timer1.initialize (1000); // set the timer for 0.1 second (1000 micro-sec)
  Timer1.attachInterrupt (inputFn); // check the Start/Stop button and encoder on timer interval
  Serial.print("Initialized...");
}

void loop() {
  enc_val_last = enc_val; // save the encoder position
  b = encoder->getButton();  // get the button value
  enc_val += encoder->getValue(); // get the encoder value

  //*****STATE MACHINE*****
  switch (mode) {
    case 0: // menu mode
      menu_mode();
      break;
    case 1: // edit mode
      edit_mode();
      break;
    case 2: // run mode
      run_mode();
      break;
    default: // error mode
      // statements
      break;
  }
  LCD(); //print to the LCD
}

void inputFn() {
  // check the encoder on the timer interval to detect new inputs
  encoder->service();

  // read the state of the switch into a local variable:
  int reading = digitalRead(buttonPin); // check to see if you just pressed the button
  if (reading != lastButtonState) {  // If the switch changed, due to noise or pressing:
    lastDebounceTime = millis();// reset the debouncing timer
  }
  if ((millis() - lastDebounceTime) > debounceDelay) { // if reading is steady for longer than the debounce
    if (reading != buttonState) { // if the button state has changed:
      buttonState = reading; // toggle the state
      if (buttonState == LOW) {// only toggle the machine state if the new button state is LOW
        is_running = !is_running;
        delay(100);
      }
    }
  }
  lastButtonState = reading;
}

void menu_mode() {
  digitalWrite(sleepPin, LOW); // sleep the a4988
  if (is_running) { // if the start switch has been toggled
    mode = 2; // switch to 'run' mode
  } else if (b == ClickEncoder::Clicked) { // encoder single click
    mode = 1; // switch to 'edit' mode
  } else if (b == ClickEncoder::DoubleClicked) { // encoder double click
    reset();
  } else if (enc_val > enc_val_last) {
    if (setting < sizeof(settings) / sizeof(settings[0]) - 1) {
      setting += 1;// go to the next setting (in range)
    }
  } else if (enc_val < enc_val_last) {
    if (setting > 0) setting -= 1;// go to the previous setting (in range)
  }
  // print to LCD
  line1 = "<" + settings[setting] + ">";
  line2 = String(settings_vals[setting]);
}

void edit_mode() {
  digitalWrite(sleepPin, LOW); // sleep the a4988
  if (b == ClickEncoder::Clicked) { // encoder single click
    mode = 0; // switch to menu mode
  } else if (b == ClickEncoder::DoubleClicked) { // encoder double click
    settings_vals[setting] = settings_vals_default[setting]; // reset to default value
  } else if (enc_val > enc_val_last) {
    settings_vals[setting] += 1; // turn up the value
  } else if (enc_val < enc_val_last) {
    if (settings_vals[setting] > 0) {
      settings_vals[setting] -= 1; // turn down the value
    }
  }
  // print to the lcd
  line1 = settings[setting];
  line2 = "<" + String(settings_vals[setting]) + ">";
}

void run_mode() {
  digitalWrite(sleepPin, HIGH); // do not sleep a4988
  if (b == ClickEncoder::Clicked) { // encoder single click
    pause = !pause; // play/pause
  }
  if (pause) {
    // show options on the LCD
    line1 = "Paused...";
    line2 = "Click: Run";
  } else if (is_running) {
    rotate();
    frames_done += 1;
    if (settings_vals[0] > 0) {
      take_photo();
      progress = 100 * frames_done / settings_vals[0];
      if (progress == 100) is_running = false;
    }
    // show the progress on the LCD
    line1 = "Progress: " + String(progress) + "%";
    line2 = "Click: Pause";
  } else {
    frames_done = 0;
    progress = 0;
    mode = 0; // return to menu
  }
}

void reset() {
  int settings_vals[] = {settings_vals_default[0], settings_vals_default[1], settings_vals_default[2]};
  is_running = false;
  mode = 0;
  frames_done = 0;
}

void take_photo() {
  // take a picture
  Eos7D.shotNow();
  if (debug) Serial.println("photo: " + String(frames_done));
  delay(settings_vals[2] * 1000);
}

void LCD() {
  while (line1.length() < 15) { // pad the first line
    line1 += " ";
  }
  while (line2.length() < 15) { // pad the second line
    line2 += " ";
  }
  lcd.setCursor(0, 0); // set the cursor to column 15, line 0
  lcd.print(line1); // print the first line
  lcd.setCursor(0, 1); // set the cursor to column 15, line 0
  lcd.print(line2);
}

void rotate() {
  if (is_running) {
    digitalWrite(dirPin, HIGH); // Enables the motor to move in a particular direction
    if (settings_vals[0] > 0) {
      //degs/frames
      degPerTurn = (settings_vals[1] / settings_vals[0]) * gearRatio;
    } else degPerTurn = settings_vals[1] * gearRatio;
    microSteps = degPerTurn / FuncStepAngle;
    Serial.println(microSteps);
    // Makes 200 pulses for making one full cycle rotation
    for (int x = 0; x < microSteps; x++) {
      digitalWrite(stepPin, HIGH);
      delay(1);
      digitalWrite(stepPin, LOW);
      delay(1);
    }
  }
}
