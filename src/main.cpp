
/*Making Spirits Bright Traffic Counter and Collection Area Light Control
Created November 2013 by Andrew Bubb - andrewmbubb@gmail.com
Version 2014.1.0 (Based off Version 6.5) (11/13/2014 8:04pm)
Removed all reference to manual counter button.  Added second beam. Temporarily commented out startup messages to speed testing.
Revised for Integration to ESP32 for real time data logging Greg Liebig 12/7/23 9:25 pm with countSuccessPin 8

/*-----( Import needed libraries )-----*/
#include <Arduino.h>
#include <Wire.h>  // Comes with Arduino IDE
#include <LiquidCrystal_I2C.h>
#include "RTClib.h"
#include "SD.h"

// SD chip select pin
const uint8_t chipSelect = SS;

// file system object
SdFat sd;

// create Serial stream
ArduinoOutStream cout(Serial);

// store error strings in flash to save RAM
#define error(s) sd.errorHalt_P(PSTR(s));


RTC_DS1307 RTC;

/*-----( Declare Constants )-----*/
#define firstDetectorPin 6
#define secondDetectorPin 7
#define countSuccessPin 8

#define redArchPin 02
#define greenArchPin 03
// set the LCD address to 0x27 for a 20 chars 4 line display
// Set the pins on the I2C chip used for LCD connections:
//                    addr, en,rw,rs,d4,d5,d6,d7,bl,blpol
LiquidCrystal_I2C lcd(0x27, 2, 1, 0, 4, 5, 6, 7, 3, POSITIVE);  // Set the LCD I2C address


/*-----( Declare Variables )-----*/
int firstDetectorState;  // Holds the current state of the FIRST IR receiver/detector
int secondDetectorState;  // Holds the current state of the SECOND IR receiver/detector

int previousFirstDetectorState; // Holds the previous state of the FIRST IR receiver/detector
int previousSecondDetectorState; // Holds the previous state of the SECOND IR receiver/detector


unsigned long detectorMillis = 0;
// unsigned long buttonMillis = 0;
int detectorTrippedCount = 0;
unsigned long noCarTimer = 0;
unsigned int totalCars;
unsigned int dailyTotal;  
int displayMode = 0;
unsigned long displayModeMillis = 0;
unsigned long dayMillis = 0;
int currentDay;
int daysRunning;
int alternateColorMode = 0;
int patternMode = 0;
unsigned long patternModeMillis = 0;

void getInitialTotal()   // open TOTAL.txt to get initial totalCars value
  {
  ifstream sdin("TOTAL.TXT");
  // check for an open failure
  if (!sdin) sd.errorHalt("sdin failed");
  sdin >> totalCars;
  sdin.close();
  }

void getInitialDailyTotal()   // open DAILYTOT.txt to get initial dailyTotal value
  {
  ifstream sdin("DAILYTOT.TXT");
  // check for an open failure
  if (!sdin) sd.errorHalt("sdin failed");
  sdin >> dailyTotal;
  sdin.close();
  }

void getInitialDaysRunning()   // open TOTAL.txt to get initial number of days the system has been running value
  {
  ifstream sdin("RUNNING.TXT");
  // check for an open failure
  if (!sdin) sd.errorHalt("sdin failed");
  sdin >> daysRunning;
  sdin.close();
  }
  


void incrementTotalFile()
/*------- Increment the grand total cars file ------------ */
{
  if (!sd.begin(chipSelect, SPI_HALF_SPEED)) sd.initErrorHalt();
  // open the input file
  ifstream sdin("TOTAL.TXT");
  // check for an open failure
  if (!sdin) sd.errorHalt("sdin failed");
  sdin >> totalCars;
  totalCars++;
  sdin.close();
  
  // open the output file
  ofstream sdout("TOTAL.TXT");
  // Replace the total vehicle count
  sdout << totalCars << endl;
  
  if (!sdout) sd.errorHalt("sdout failed");

  sdout.close();
}

void incrementDailyTotal()
  {
  if (!sd.begin(chipSelect, SPI_HALF_SPEED)) sd.initErrorHalt();
  // open the input file
  ifstream sdin("DAILYTOT.TXT");
  // check for an open failure
  if (!sdin) sd.errorHalt("sdin failed");
  sdin >> dailyTotal;
  dailyTotal++;
  sdin.close();
  
  // open the output file
  ofstream sdout("DAILYTOT.TXT");
  // Replace the total vehicle count
  sdout << dailyTotal << endl;
  
  if (!sdout) sd.errorHalt("sdout failed");

  sdout.close();
  }

void displayGrandTotal()
  {
    lcd.clear();
    lcd.setCursor(0,0); //Start at character 4 on line 0
    lcd.print("Total Cars Since");
    lcd.setCursor(0,1);
    lcd.print("1st Night: ");
    lcd.print(totalCars);
  }

void displayDailyTotal()
  {
    lcd.clear();
    lcd.setCursor(0,0); //Start at character 4 on line 0
    lcd.print("Total Cars");
    lcd.setCursor(0,1);
    lcd.print("Today: ");
    lcd.print(dailyTotal);
  }
  
/* void displayDailyAverage()
  {
    lcd.clear();
    lcd.setCursor(0,0); //Start at character 1 on line 0
    lcd.print("Daily Average");
    lcd.setCursor(0,1);
    lcd.print(totalCars / daysRunning);
  }

void displayHourlyAverage()
  {
    lcd.clear();
    lcd.setCursor(0,0); //Start at character 1 on line 0
    lcd.print("Hourly Average");
    lcd.setCursor(0,1);
    lcd.print(totalCars / (daysRunning * 4));
  }
Averages no longer displayed.  Removing subroutines from code in next version (6.4) */

void displayDate()
  {
    DateTime now = RTC.now();
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("The Current Date");
    lcd.setCursor(0,1);
    lcd.print("   ");
    lcd.print(now.month());
    lcd.print("/");
    lcd.print(now.day());
    lcd.print("/");
    lcd.print(now.year());
  }
  
void displayTime()
  {
    DateTime now = RTC.now();
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("The Current Time");
  lcd.setCursor(0,1);
  lcd.print("    ");
  if (now.hour() <= 12)
    {  
    lcd.print(now.hour(), DEC);
    }
  else
    {
    lcd.print(now.hour()-12, DEC);  
    }
  lcd.print(':');
  if (now.minute() < 10)
    {
    lcd.print("0");  
    lcd.print(now.minute(), DEC);
    }
  else
    {
     lcd.print(now.minute(), DEC);
    }
  if (now.hour() >= 12)
    {
    lcd.print(" PM");
    }
  else
    {
    lcd.print(" AM");  
    }
  }
  
 void resetDailyTotal()
  {
  
    if (!sd.begin(chipSelect, SPI_HALF_SPEED)) sd.initErrorHalt();
   
    // open the output file
    ofstream sdout("DAILYTOT.TXT");
    // Reset the total vehicle count
    sdout << dailyTotal << endl;
    
    if (!sdout) sd.errorHalt("sdout failed");
  
    sdout.close();
    
  }

void incrementDaysRunning()
    {
  if (!sd.begin(chipSelect, SPI_HALF_SPEED)) sd.initErrorHalt();
  // open the input file
  ifstream sdin("RUNNING.TXT");
  // check for an open failure
  if (!sdin) sd.errorHalt("sdin failed");
  sdin >> daysRunning;
  daysRunning++;
  sdin.close();
  
  // open the output file
  ofstream sdout("RUNNING.TXT");
  // Replace the number of days the program has been running
  sdout << daysRunning << endl;
  
  if (!sdout) sd.errorHalt("sdout failed");

  sdout.close();
  }
void checkDay() // See if it's a new day and if so, reset int dailyTotal
  {
    if (!sd.begin(chipSelect, SPI_HALF_SPEED)) sd.initErrorHalt();
    // open the input file
    ifstream sdin("NOW.TXT");
    // check for an open failure
    if (!sdin) sd.errorHalt("sdin failed");
    sdin >> currentDay; //---------------------------------------------------------------------------------------------------------!!!!!!!!!!!!!!!!!!!!!!!!!!!!!111
    sdin.close();
    DateTime now = RTC.now();
    now = RTC.now();
    if (currentDay != now.day())
    {
      Serial.println("New Day");
      currentDay = now.day();
      // open the output file
      ofstream sdout("NOW.TXT");
      // Replace existing with current date
      sdout << currentDay << endl;
      
      if (!sdout) sd.errorHalt("sdout failed");
    
      sdout.close();
      dailyTotal=0;
      resetDailyTotal();
      incrementDaysRunning();
    }
  }  





void beamCarDetect () // If a car is detected by a beam break, then increment the counter by 1 and add an entry to the Master.csv log file on the SD card
{
    DateTime now = RTC.now();
    digitalWrite (countSuccessPin, HIGH);
    detectorTrippedCount++;                // add 1 to the counter, this prevents the code from being run more than once after tripped for 3 seconds.
    Serial.print("Cars Today:  ");
    Serial.println(dailyTotal);
    lcd.noBacklight();
    delay(125);
    lcd.backlight();
    delay(125);    
//    displayGrandTotal();
    digitalWrite(redArchPin, LOW);
    digitalWrite(greenArchPin, HIGH);
    noCarTimer = millis();

/*------Append to log file*/

  now = RTC.now();
  // initialize the SD card at SPI_HALF_SPEED to avoid bus errors with
  // breadboards.  use SPI_FULL_SPEED for better performance.
  if (!sd.begin(chipSelect, SPI_HALF_SPEED)) sd.initErrorHalt();


//    cout << pstr("Appending to: ") << "MASTER.CSV" << endl;
  
    // open stream for append
    ofstream sdout("MASTER.CSV", ios::out | ios::app);
    if (!sdout) error("open failed");

    int currentMonth = now.month();
    int currentDay = now.day();
    int currentYear = now.year();
    int currentHour = now.hour();
    int currentMinute = now.minute();
    int currentSecond = now.second();

    sdout << currentMonth << "/" << currentDay << "/" << currentYear << "," << currentHour << ":" << currentMinute << ":" << currentSecond <<endl; // Append MM/DD/YYYY,HH:MM:SS event to Master log on SD
//    cout << currentMonth << "/" << currentDay << "/" << currentYear << "," << currentHour << ":" << currentMinute << ":" << currentSecond <<endl; // Display MM/DD/YYYY,HH:MM:SS event to Serial
    // close the stream
    sdout.close();

    if (!sdout) error("append data failed");
//    cout << endl << "Done" << endl;

  incrementTotalFile();
  incrementDailyTotal();
  digitalWrite (countSuccessPin, LOW);
}

void playPattern() // Flash an alternating pattern on the arches (called if a car hasn't been detected for over 30 seconds)
{ 
  if (patternMode == 0 && (millis() - patternModeMillis) >= 500)
    {
      digitalWrite(redArchPin, HIGH); // Turn Red Arch Off
      digitalWrite(greenArchPin, HIGH); // Turn Green Arch Off
      patternMode++;
      patternModeMillis = millis();
    }
  if (patternMode == 1 && (millis() - patternModeMillis) >= 500)
    {
        digitalWrite(redArchPin, LOW); // Turn Red Arch On
        digitalWrite(greenArchPin, HIGH); // Turn Green Arch Off
      patternMode++;
      patternModeMillis = millis();
    }
  if (patternMode == 2 && (millis() - patternModeMillis) >= 500)
    {
        digitalWrite(redArchPin, HIGH); // Turn Red Arch Off
        digitalWrite(greenArchPin, LOW); // Turn Green Arch On
      patternMode++;
      patternModeMillis = millis();
    }
  if (patternMode == 3 && (millis() - patternModeMillis) >= 500)
    {
        digitalWrite(redArchPin, LOW); // Turn Red Arch On
        digitalWrite(greenArchPin, LOW); // Turn Green Arch On
      patternMode++;
      patternModeMillis = millis();
    }
  if (patternMode == 4 && (millis() - patternModeMillis) >= 500)
    {
        digitalWrite(redArchPin, HIGH); // Turn Red Arch Off
        digitalWrite(greenArchPin, HIGH); // Turn Green Arch Off
      patternMode++;
      patternModeMillis = millis();
    }
  if (patternMode == 5 && (millis() - patternModeMillis) >= 500)
    {
        digitalWrite(redArchPin, HIGH); // Turn Red Arch Off
        digitalWrite(greenArchPin, LOW); // Turn Green Arch On
      patternMode++;
      patternModeMillis = millis();
    }
  if (patternMode == 6 && (millis() - patternModeMillis) >= 500)
    {
        digitalWrite(redArchPin, LOW); // Turn Red Arch On
        digitalWrite(greenArchPin, HIGH); // Turn Green Arch Off
      patternMode++;
      patternModeMillis = millis();
    }
  if (patternMode == 7 && (millis() - patternModeMillis) >= 500)
    {
        digitalWrite(redArchPin, LOW); // Turn Red Arch On
        digitalWrite(greenArchPin, LOW); // Turn Green Arch On
      patternMode = 0;
      patternModeMillis = millis();
    }
}

void setup()   /*----( SETUP: RUNS ONCE )----*/
{
  firstDetectorState = 0;
  secondDetectorState = 0;
  digitalWrite(redArchPin, HIGH);
  digitalWrite(greenArchPin, HIGH);
  pinMode(firstDetectorPin, INPUT);
  pinMode(secondDetectorPin, INPUT);
  pinMode(redArchPin, OUTPUT);
  pinMode(greenArchPin, OUTPUT);
  pinMode(countSuccessPin, OUTPUT);
  digitalWrite(countSuccessPin, LOW);

  
  // initialize serial communication:
  Serial.begin(9600);
  Wire.begin();
  RTC.begin();
  lcd.begin(16,2);   // initialize the lcd for 16 chars 2 lines, turn on backlight
  
  // ------- Quick 3 blinks of LCD backlight  -------------
  for(int i = 0; i< 3; i++)
  {
    lcd.backlight();
    delay(250);
    lcd.noBacklight();
    delay(250);
  }
  lcd.backlight(); // finish with LCD backlight on  
  


//-------- Write the startup messages on the LCD display ------------------
// NOTE: Cursor Position: (CHAR, LINE) start at 0,0 for leftmost line 1 start at 0,1 for leftmost line 2  
  
/*  lcd.setCursor(0,0);
  lcd.print(" Making Spirits");
  lcd.setCursor(0,1);
  lcd.print("     Bright");
  delay(4000);
  lcd.clear();  
  lcd.setCursor(0,0);
  lcd.print("Traffic Counter");
  lcd.setCursor(0,1);
  lcd.print("& Light Control");
  delay(4000);  
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print(" WARNING - HIGH");
  lcd.setCursor(0,1);
  lcd.print(" VOLTAGE INSIDE!");
  delay(4000);  
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("DISCONNECT POWER");
  lcd.setCursor(0,1);
  lcd.print("BEFORE OPENING!!");
  delay(4000);  
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Instructions On");
  lcd.setCursor(0,1);
  lcd.print("SD Card in Unit");
  delay(4000);  
  displayDate(); //Display the current Date on the LCD display
  delay(4000);
  displayTime(); //Display the current Time on the LCD display
  delay(4000); 
  
//  Removed startup information display section to speed up testing, found it was not needed for normal operation and will leave commented out */ 
  
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("    Ready to");
  lcd.setCursor(0,1);
  lcd.print("   Count Cars!");
  digitalWrite(greenArchPin, LOW); // Turn Green Arch On

  
  // Init SD Card
  // initialize the SD card at SPI_HALF_SPEED to avoid bus errors with
  // breadboards.  use SPI_FULL_SPEED for better performance.
  if (!sd.begin(chipSelect, SPI_HALF_SPEED)) sd.initErrorHalt();

getInitialTotal();
getInitialDailyTotal();
getInitialDaysRunning();
checkDay();
displayModeMillis = millis();
noCarTimer = millis();

}/*--(end setup)---*/




void  loop ()  /*----( LOOP: RUNS CONSTANTLY )----*/
{
  //reset daily total at 4:55 pm when park opens to eliminate traffic that happens while car counter is on and there is non-show traffic 
    DateTime now = RTC.now();
    now = RTC.now();

  if ((now.hour() == 16) && (now.minute() == 55) && (now.second() == 0)){
     dailyTotal = 0;
   }
  firstDetectorState = digitalRead (firstDetectorPin); //Read the current state of the FIRST IR beam receiver/detector
  secondDetectorState = digitalRead (secondDetectorPin); //Read the current state of the SECOND IR beam receiver/detector

  if (secondDetectorState == LOW && previousSecondDetectorState == HIGH && millis()- detectorMillis > 200) // Bounce check, if the beam is broken start the timer and turn on only the green arch
    {
    digitalWrite(redArchPin, HIGH); // Turn Red Arch Off
    digitalWrite(greenArchPin, LOW); // Turn Green Arch On
    detectorMillis = millis();    // if the beam is broken remember the time 
    }

  if (secondDetectorState == LOW && ((millis() - detectorMillis) % 500) < 20 && millis() - detectorMillis > 500 && detectorTrippedCount == 0 && firstDetectorState == LOW) // If the SECOND beam has been broken for more than 0.50 second (1000= 1 Second) & the FIRST beam is broken
    {
      beamCarDetect();  //Call the subroutine that increments the car counter and appends the log with an entry for the vehicle when the IR beam is broken
    }
    
/*--------- Reset the counter if the beam is not broken --------- */    
 if (secondDetectorState == HIGH)  //Check to see if the beam is not broken, this prevents the green arch from never turning off)
   {
      if (detectorTrippedCount != 0)
      digitalWrite(redArchPin, HIGH);// Turn Red Arch Off
      if(millis() - noCarTimer >= 30000) // If the beam hasn't been broken by a vehicle for over 30 seconds then start a pattern on the arches.
        {
        playPattern();
        }
      else
      {
        digitalWrite(greenArchPin, LOW); // Turn Green Arch On
      }
      detectorTrippedCount = 0;
   
   
   }

/*-------- Rotate through LCD Displays for Grand Total, Total Today, Current Date & Current Time - change every 5 seconds ---------*/
if (displayMode == 0 && (millis() - displayModeMillis) >= 5000)
  {
  displayGrandTotal();
  displayMode++;
  displayModeMillis = millis();
  }
if (displayMode == 1 && (millis() - displayModeMillis) >= 5000)
  {
  displayDailyTotal();
  displayMode++;
  displayModeMillis = millis();
  }
if (displayMode == 2 && (millis() - displayModeMillis) >= 5000)
  {
  displayDate();
  displayMode++;
  displayModeMillis = millis();
  }      
if (displayMode == 3 && (millis() - displayModeMillis) >= 5000)
  {
  displayTime();
  displayMode = 0;
  displayModeMillis = millis();
  }    


/*-------- Reset the detector and button state to 0 for the next go-around of the loop ---------*/    
previousSecondDetectorState = secondDetectorState; // Reset detector state

}/* --(end main loop )-- */

/* ( THE END ) */