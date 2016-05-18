/*
 * This scrips triggers a laser using PWM to control the average power output
 * and to adjust the Pulse Repitition Rate (PRR). The PRR and duty cycle are 
 * set by two external dials, read into the system using inbuild ADC's. An LCD
 * screen is used to display the PRR, DC and pulse high time values to the user.
 * 
 * The IO connections from the teensy3.1 for this project is as follows:
 * LCD--
 * RS: 8 (test = 22)
 * EN: 9 (test = 20)
 * D4: 4 (test = 19)
 * D5: 5 (test = 18)
 * D6: 6 (test = 17)
 * D7: 7 (test = 16)
 * Laser--
 * Trigger: 3
 * Power Level: 23 (A9) -INPUT
  * Dials--
 * PRR: 14 (A0) -INPUT
 * Duty Cycle: 15 (A1) -INPUT
 * System--
 * Trigger Selection Pin: 1 (HIGH = ext; LOW = int)
 * Temperature safety cutoff input: 20 (active high)
 * E-stop: 19 (active high)
 * 
 * The dials and power level must sweep between 0 and 3.3 volts.
 * 
 * PRR ranges from 20kHz - 600kHz. The dial should be logarithmic-scaled?
 * Duty cycle ranges from 0.2 - 36%. The dial should be linear.
 */

// ----------- includes --------------------------
#include <LiquidCrystal.h> //includes the IO library for the LCD display
#include <Metro.h> //library used for timing for restarting of the laser timer
#include <math.h> //library for calculating logarithmic dial parameters

// ------- Pin macros ----------------
#define LaserPin 3 //Laser control is via Teensy 3.1 pin 3
#define PRRdialPin 14 //PRR is set using an analog signal on pin 14
#define DCdialPin 15 //DC is set using an analog signal on pin 15
#define PwrLvlPin 23 //Power level is read from A9 on pin 23
#define _lcdRS 8 //LCD register select pin
#define _lcdEN 9 //LCD enable pin
#define _lcdD4 4 //LCD data 4 pin
#define _lcdD5 5 //LCD data 5 pin
#define _lcdD6 6 //LCD data 6 pin
#define _lcdD7 7 //LCD data 7 pin
#define TriggerSourcePin 1 //Trigger source selector switch: internal=LOW external=HIGH
#define TemperatureSensorPin 20 //Temperature protection enabled when HIGH
#define EstopPin 19 //Estop when HIGH

//--------- variable definitions ----------
LiquidCrystal lcd( _lcdRS, _lcdEN, _lcdD4, _lcdD5, _lcdD6, _lcdD7 ); //lcd(27,28,29,30,31,32,33);
Metro refreshMetro = Metro(250); //the dials and pins are checked 4 times a second
float PRR =300.0; //default value: ie, 300 kHz.
float Duty = 5.0; //default value: ie, 5%.
float PwrLvl = 0; //default value: no power

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
  Serial.println("Initalising..."); //send a generic startup message to the serial port

  // setup pin directions
  pinMode(LaserPin,OUTPUT); //LaserPin pin as ouput
  pinMode(PRRdialPin,INPUT); // PRRdialPin as input
  pinMode(DCdialPin,INPUT); // PRRdialPin as input
  pinMode(PwrLvlPin,INPUT); // PwrLvl as input
  pinMode(TriggerSourcePin,INPUT); //Trig source selector as input
  pinMode(TemperatureSensorPin,INPUT); //Temperature sensor as input
  pinMode(EstopPin,INPUT); //Estop sensor as input

  // set up the ADC system
  analogReadResolution(12); //12-bit input resolution for dials
  analogReference(DEFAULT); //uses the 3.3v as a reference.

  // set up the hardware pulse generator
  analogWriteResolution(12); //12-bit settings for duty cycle
  analogWriteFrequency(LaserPin,PRR*1000); //set the frequency to PRR * 1000 (ie, PRR in Hz).

  // small delay for effect - serves no other purpose
  delay(1000); //about 1 seconds

  //Get the values and start the pulses
  PRR = GetPRRvalue(); //read the PRR value from the dials
  Duty = GetDutyValue(); //read the Duty value from the dials
  ConfigurePulseGenerator(); //configures the pulse generator
}

void loop() {
  if(refreshMetro.check() == 1){
    PRR = GetPRRvalue(); //read the PRR value from the dials
    Duty = GetDutyValue(); //read the Duty value from the dials
    PwrLvl = GetPwrLevel(); //reads the laser power level from the system
    ConfigurePulseGenerator(); //configures the pulse generator
    SendSerialUpdate(); //sends some messages to the serial port for debugging
    UpdateLCD(); //updates the LCD display
  }
}

float GetPRRvalue(void){
  uint16_t DialValue = analogRead(PRRdialPin); //returns a 12-bit value for PRR
  /* 0 = 20kHz
   * 4095 = 600kHz
   * Described as an exponential relationship Y = A^(BX) + C where
   *    Y = PRR (in kHz)
   *    A = 584
   *    B = 0.000244
   *    C = 19
   *    X = DialValue in DAX
    */
  const float A = 584;
  const float B = 0.000244;
  const float C = 19;
    
    if (DialValue >4095) //check if the value is greter than 600kHz (should not be possible)
      DialValue = 4095; //limit it to 600khz if it is greater
  return ((float)pow(A , (B * (float)DialValue)) + C); //return the khz value for PRR
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
  /*
   * Duty cycle depends entirely on the PRR!
   * min pulse time = 100ns
   * max pulse time = 600ns
   */
  const float minT = 100.0; //minimum ON time in nanoseconds
  const float maxT = 600.0; //maximum ON time in nanoseconds
  float PeriodPRR = 1000000/PRR; //the period in nanoseconds
  float minDuty = (100 * minT) / PeriodPRR; //the minimum duty cycle in %
  float maxDuty = (100 * maxT) / PeriodPRR; //the maximum duty cycle in % 
  uint16_t DialValue = analogRead(DCdialPin); //returns a 12-bit value for Duty
  /* 0 = 0.2 %
   * 4095 = 36.0 %
   * Described as linear relationship Y = MX + C where
   *    Y = DC (in %)
   *    M = 0.008742 ... this value is now variable
   *    X = DialValue 
   *    C = 0.2 ... this value is now variable
    */
  float M = (maxDuty - minDuty) / 4095; // M = rise / run = (max - min) / DAC bit range.
  float C = minDuty;
  return ((M * (float)DialValue) + C);
}

void ConfigurePulseGenerator(void){
  if(isUpdateNeeded()){ //set the PRR if the update is needed
    analogWriteFrequency(LaserPin,PRR*1000); //set the frequency to PRR * 1000 (ie, PRR in Hz).
  }
  //set the Duty every time (no jitter issues here)
  analogWrite(LaserPin,(uint16_t)((Duty * 40.95)+0.5)); //Duty cycle outputs to 4095 digital (ie, 100% = 4095)
  //therefore, Duty/40.95 = duty cycle in digital units. Adding 0.5 at the end helps with rounding.
}

void SendSerialUpdate(void){
  Serial.print("Trigger: ");
  if (!digitalReadFast(TriggerSourcePin))
    Serial.print("Int");
  else
    Serial.print("Ext");
  Serial.print(" ,PRR: ");
  Serial.print(PRR);
  Serial.print(" ,Duty: ");
  Serial.print(Duty);
  Serial.print(" ,Pulse Time (ns): ");
  Serial.print(((1000000/PRR) * (Duty / 100)));
  Serial.print(" ,PwrLvl: ");
  Serial.print(PwrLvl);
  Serial.print(" ,Temperature Alarm: ");
  Serial.print(digitalReadFast(TemperatureSensorPin));
  Serial.print(" ,Estop Active: ");
  Serial.println(digitalReadFast(EstopPin));
}

void UpdateLCD(void){
  lcd.clear(); //clear the display
  if (digitalReadFast(EstopPin)){ //check the emergency stop
    //emergency stop button is active. Tell the user!
    lcd.setCursor(2,0); //set the cursor position to row 0, column 2
    lcd.print("ESTOP ACTIVE");
    lcd.setCursor(4,1); //set the cursor position to row 1, column 4
    lcd.print("no laser");
    return; //exit the LCD routine
  }
  if (digitalReadFast(TemperatureSensorPin)){ //the laser is too hot
    //laser is too hot. Tell the user!
    lcd.setCursor(1,0); //set the cursor position to row 0, column 1
    lcd.print("!TEMPERATURE!!");
    lcd.setCursor(4,1); //set the cursor position to row 1, column 4
    lcd.print("no laser");
    return; //exit the LCD routine
  }
  //only print the dial values to the display when the trigger is set for internal:
  if (!digitalReadFast(TriggerSourcePin)){
    //The location of the number changes depending on its value!
    if (PRR >= 10){
      lcd.setCursor(3,0); //set the cursor position to row 0, column 3
    }
    else{
      lcd.setCursor(4,0); //set the cursor position to row 0, column 4
    }
    lcd.print((PRR+0.049),1); //4 or 5 characters
    lcd.print(" kHz");
    //The location of the pulse time is fixed (always in the hundreds range)
    float pulseTime = ((1000000/PRR) * (Duty / 100));
    lcd.setCursor(0,1); //set the cursor position to row 1, column 0
    lcd.print((pulseTime+0.049),1); //always 5 characters
    lcd.print(" Pulse (ns)");
  }
  else{
    lcd.setCursor(4,0); //set the cursor position to row 0, column 4
    lcd.print("EXTERNAL"); //print 'external' to the lcd
  }
}

bool isUpdateNeeded(void){
  /*
   * Using an integrator to measure the error between current PRR and set PRR.
   * The integrator should be good for filtering noise on the set PRR value
   * and redude the need to continually set the frequency (which would cause jitters in the pulses)
   * 
   * The process goes like this:
   * 1: currentError = setPRR - currentPRR
   * 2: integratedError += currentError
   * 3: if integrated Error >< some error window (10%?) then change the current PRR and reset integratedError: return TRUE
   *         else return false
   */
   const float errorThreshold = 1.5; //the error threshold is 1.5 kHz (integrated errors larger than this will trigger an update)
   static float integratedError = 0; //initalise the integrated error to Zero
   static float currentPRR = 0; //initalising it to zero means the first read will always produce a big error (a good thing!)

   float currentError = PRR - currentPRR; //this is the error in kHz
   integratedError = integratedError + currentError; //this is the accumulative error over time (once ever 250ms)

   if ((integratedError > errorThreshold) | (integratedError < (-1 * errorThreshold))){
     //error is outside threshold. initate an update!
     integratedError = 0; //reset the error integrator
     currentPRR = PRR; //assign the setPRR to the currentPRR
     return true; //return TRUE to initiate the update
   }
   else
     return false;
}
