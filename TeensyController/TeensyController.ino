/*
 * This scrips triggers a laser using PWM to control the average power output
 * and to adjust the Pulse Repitition Rate (PRR). The PRR and duty cycle are 
 * set by two external dials, read into the system using inbuild ADC's. An LCD
 * screen is used to display the PRR and DC values to the user.
 * 
 * No parameters needs to be memorised for this project. 
 * The IO connections from the teensy3.1 for this project (the 100W hand held 
 * laser) is as follows:
 * LCD--
 * RS: 8 (test = 22)
 * EN: 9 (test = 20)
 * D4: 4 (test = 19)
 * D5: 5 (test = 18)
 * D6: 6 (test = 17)
 * D7: 7 (test = 16)
 * Laser--
 * Trigger: 12
 * Power Level: 23 (A9)
 * Dials--
 * PRR: 14 (A0)
 * Duty Cycle: 15 (A1)
 * 
 * The dials and power level must sweep between 0 and 3.3 volts.
 * 
 * The relationship between PRR, Power and DC is not simple. The image 
 * '100w operating mode.png' shows this relationship graphically.
 */

// ----------- includes --------------------------
#include <LiquidCrystal.h> //includes the IO library for the LCD display
#include <Metro.h> //library used for timing for restarting of the laser timer

// ------- Pin macros ----------------
#define LaserPin 12 //Laser control is via Teensy 3.1 pin 12
#define LaserOn digitalWriteFast(LaserPin,HIGH) //The laser is active HIGH
#define LaserOff digitalWriteFast(LaserPin,LOW) //The laser is disabled LOW
#define PRRdialPin 14 //PRR is set using an analog signal on pin 14
#define DCdialPin 15 //DC is set using an analog signal on pin 15
#define PwrLvlPin 23 //Power level is read from A9 on pin 23
#define _lcdRS 8 //LCD register select pin
#define _lcdEN 9 //LCD enable pin
#define _lcdD4 4 //LCD data 4 pin
#define _lcdD5 5 //LCD data 5 pin
#define _lcdD6 6 //LCD data 6 pin
#define _lcdD7 7 //LCD data 7 pin

//--------- variable definitions ----------
IntervalTimer timerLaser; //timer object used for Laser pulsing
LiquidCrystal lcd( _lcdRS, _lcdEN, _lcdD4, _lcdD5, _lcdD6, _lcdD7 ); //LCD pin initilise //lcd(22,21,20,19,18,17,16);//
Metro refreshMetro = Metro(250); //the laser timer is restarted 4 times a second
float PRR = 1.0; //ie, 1 kHz.
float Duty = 50.0; //ie, 50%.
float PwrLvl = 0; //no power
float _onTime = 0; //the time the laser is on for
float _offTime = 0; //the time the laser is off for
uint8_t Mode = 1; //This is to identify if the laser is in CW - pulsed - off mode (2-1-0)

//-------- Setup functions --------------
void setup() {
  
  //display startup message on LCD
  lcd.begin(16,2);
  lcd.clear(); //clear the display
  lcd.setCursor(3,0); //set the cursor position to row 0, column 3
  lcd.print("Welcome"); //7 characters
  lcd.setCursor(0,1); //set the cursor position to row 1, column 0
  lcd.print("System starting"); //15 characters
  
  // setup the serial port
  Serial.begin(9600); //serial port used for sending debug messages only
  Serial.println("Initalising...");

  // setup pin directions
  pinMode(LaserPin,OUTPUT); //LaserPin pin as ouput
  pinMode(PRRdialPin,INPUT); // PRRdialPin as input
  pinMode(DCdialPin,INPUT); // PRRdialPin as input
  pinMode(PwrLvlPin,INPUT); // PwrLvl as input

  // set up the pin states
  LaserOff;

  // set up the ADC system
  analogReadResolution(12); //12-bit input resolution for dials
  analogReference(DEFAULT); //uses the 3.3v as a reference.

  // small delay for effect - serves no other purpose
  delay(1000); //about 1 seconds

  //Get the values and start the pulses
  PRR = GetPRRvalue(); //read the PRR value from the dials
  Duty = GetDutyValue(); //read the Duty value from the dials
  RestartTimer(); //start the main laser timer
}

void loop() {
  if(refreshMetro.check() == 1){
    PRR = GetPRRvalue(); //read the PRR value from the dials
    Duty = GetDutyValue(); //read the Duty value from the dials
    PwrLvl = GetPwrLevel(); //reads the laser power level from the system
    RestartTimer(); //restarts the timer based on the new values
    SendSerialUpdate(); //sends some messages to the serial port for debugging
    UpdateLCD(); //updates the LCD display
  }
}

float GetPRRvalue(void){
  uint16_t DialValue = analogRead(PRRdialPin); //returns a 12-bit value for PRR
  /* 0 = 1kHz
   * 4095 = 30kHz
   * Described as linear relationship Y = MX + C where
   *    Y = PRR (in kHz)
   *    M = 0.007082
   *    X = DialValue
   *    C = 1
    */
    if (DialValue >4095)
      DialValue = 4095;
  return ((0.007082 * (float)DialValue) + 1);
}

float GetPwrLevel(void){
  uint16_t InputValue = analogRead(PwrLvlPin); //returns a 12-bit value for PwrLvl
  /* 0 = 0%
   * 3587 = 110%
   * Described as linear relationship Y = MX + C where
   *    Y = PwrLvl (in %)
   *    M = 0.031007
   *    X = InputValue
   *    C = 0
    */
    if (InputValue >4095)
      InputValue = 4095;
  return ((0.031007 * (float)InputValue));
}

float GetDutyValue(void){
  uint16_t DialValue = analogRead(DCdialPin); //returns a 12-bit value for Duty
  /* 0 = 0 %
   * 4095 = 100 %
   * Described as linear relationship Y = MX + C where
   *    Y = DC (in %)
   *    M = 0.02442
   *    X = DialValue
   *    C = 0
    */
  return (0.02442 * (float)DialValue);
}

void RestartTimer(void){
  //always restart the laser on the off cycle!
  CalculateModeValues(); //calcualtes the new timing values
  timerLaser.end(); //ends the laser (if it wasnt already)
  if (Mode == 2){
    LaserOn; //mode 2 is CW - turn the laser on!
  }
  else if (Mode == 1){
    LaserOff;
    timerLaser.begin(LaserToggle,_offTime);
  }
  else {
    LaserOff; //mode 0 is laser off - turn the laser off!
  }
}

void LaserToggle(){
  if (digitalReadFast(LaserPin) == HIGH){
    LaserOff;
    timerLaser.begin(LaserToggle,_offTime);
  }
  else {
    LaserOn;
    timerLaser.begin(LaserToggle,_onTime);
  }
}

void SendSerialUpdate(void){
  Serial.print("Mode: ");
  Serial.print(Mode);
  Serial.print(", PRR: ");
  Serial.print(PRR);
  Serial.print(" ,Duty: ");
  Serial.print(Duty);
  Serial.print(" ,PwrLvl: ");
  Serial.println(PwrLvl);
}

void UpdateLCD(void){
  if (Mode == 2){
    lcd.clear();
    lcd.setCursor(1,0);
    lcd.print("Laser CW Mode");
  }
  else if (Mode == 1){
    lcd.clear(); //clear the display
    //The location of the number changes depending on its value!
    if (PRR >= 10){
      lcd.setCursor(3,0); //set the cursor position to row 0, column 3
    }
    else{
      lcd.setCursor(4,0); //set the cursor position to row 0, column 4
    }
    lcd.print(PRR); //4 or 5 characters
    lcd.print(" kHz");
    //The location of the Duty number changes depending on its value!
    if (Duty >= 10){
      lcd.setCursor(2,1); //set the cursor position to row 1, column 2
    }
    else{
      lcd.setCursor(3,1); //set the cursor position to row 1, column 3
    }
    lcd.print(Duty); //4 or 5 characters
    lcd.print(" % Duty");
    }
    else {
      lcd.clear();
      lcd.setCursor(3,0);
      lcd.print("Laser Off");
    }
}

void CalculateModeValues(void){
/*
 * These calculations are based off the diagram send in emails called
 * '100w operating mode.png'.
 * 
 * At 30kHz, the max DC is 75% and min DC is 25%
 * At 10kHz and less, the max DC is 95% and the min DC is 5%
 * 
 * All values higher than MAX DC end up as mode 2 (Laser ON in CW mode)
 * All values less than MIN DC end up as mode 0 (Laser OFF)
 * 
 * Additionally: - NOT IN DIAGRAM!
 * At PwrLvl 110%, the max DC is 45%
 * At PwrLvl 50%, the max DC is 100%
 * 
 * Any value in between is calcualted _onTime and _offTime
 */

 //First step would be to calculate Dmin and Dmax for the current frequency
 float Dmin = 5; //default for anything less than 10kHz
 float Dmax = 95; //default for anyything less than 10kHz
 if (PRR > 10){ // anything more than 10kHz needs to be adjusted
  /* 10 kHz = 95 Dmax
   * 30 kHz = 75 Dmax
   * Described as linear relationship Y = MX + C where
   *    Y = Dmax (in %)
   *    M = -1
   *    X = PRR
   *    C = 105
   */
    Dmax = ((-1 * PRR) + 105);
  /* 10 kHz = 5 Dmin
   * 30 kHz = 25 Dmin
   * Described as linear relationship Y = MX + C where
   *    Y = Dmin (in %)
   *    M = 1
   *    X = PRR
   *    C = -5
   */
    Dmin = ((1 * PRR) - 5);
 }
 //Check if higher than Dmax
 if (Duty > Dmax){
  //It is higher, so make it mode 2 : laser CW
  Mode = 2;
  timerLaser.end();
  _onTime = 0;
  _offTime = 0;
 }
 //check if its is lower than Dmin
 else if (Duty < Dmin){
  //It is lower, so make it mode 0 : laser off
  Mode = 0;
  timerLaser.end();
  _onTime = 0;
  _offTime = 0;
 }
 //If its not bigger or small that the DC limits, calculate the ON/OFF times
 else{
  Mode = 1;
  _onTime = ((1000 / PRR) * (Duty / 100));
  _offTime =  ((1000 / PRR) * (1 - (Duty / 100)));
 }

 //Additional checks here. Only if the laser is on and the power is set above 50%
 if ((Mode != 0) && (PwrLvl > 50)){
  //Calculate the DC limit for this Power level
  float Dlim = (-1 * 0.9166 * PwrLvl) + 145.83;
  if (Duty > Dlim){
    //The duty cycle must be limited!
    Mode = 1;
    Duty = Dlim;
    _onTime = ((1000 / PRR) * (Duty / 100));
    _offTime =  ((1000 / PRR) * (1 - (Duty / 100)));
  }
 }
}

