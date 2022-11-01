/*
 * Arduino Guitar Tuner
 * by Nicole Grimwood
 *
 * For more information please visit:
 * https://www.instructables.com/id/Arduino-Guitar-Tuner/
 *
 * Based upon:
 * Arduino Frequency Detection
 * created October 7, 2012
 * by Amanda Ghassaei
 *
 * This code is in the public domain. 
*/
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define OLED_RESET 4
Adafruit_SSD1306 display(OLED_RESET);

//data storage variables
byte newData = 0;
byte prevData = 0;
unsigned int time = 0;//keeps time and sends vales to store in timer[] occasionally
int timer[10];//storage for timing of events
int slope[10];//storage for slope of events
unsigned int totalTimer;//used to calculate period
unsigned int period;//storage for period of wave
byte index = 0;//current storage index
float frequency;//storage for frequency calculations
int maxSlope = 0;//used to calculate max slope as trigger point
int newSlope;//storage for incoming slope data

//variables for deciding whether you have a match
byte noMatch = 0;//counts how many non-matches you've received to reset variables if it's been too long
byte slopeTol = 3;//slope tolerance- adjust this if you need
int timerTol = 10;//timer tolerance- adjust this if you need

//variables for amp detection
unsigned int ampTimer = 0;
byte maxAmp = 0;
byte checkMaxAmp;
byte ampThreshold = 20;//raise if you have a very noisy signal

//variables for tuning
float correctFrequency;//the correct frequency for the string being played

//bitmaps das setas
const unsigned char setacima [] PROGMEM = {
  0x01, 0x80, 0x03, 0xc0, 0x07, 0xe0, 0x07, 0xe0, 0x0f, 0xf0, 0x1f, 0xf8, 0x3f, 0xfc, 0x07, 0xe0, 
  0x07, 0xe0, 0x07, 0xe0, 0x07, 0xe0, 0x07, 0xe0, 0x07, 0xe0, 0x07, 0xe0, 0x07, 0xe0, 0x07, 0xe0
};
const unsigned char setabaixo [] PROGMEM = {
  0x07, 0xe0, 0x07, 0xe0, 0x07, 0xe0, 0x07, 0xe0, 0x07, 0xe0, 0x07, 0xe0, 0x07, 0xe0, 0x07, 0xe0, 
  0x07, 0xe0, 0x3f, 0xfc, 0x1f, 0xf8, 0x0f, 0xf0, 0x07, 0xe0, 0x07, 0xe0, 0x03, 0xc0, 0x01, 0x80
};

void setup(){
  
  Serial.begin(9600);
  
  //controlam o motor
  pinMode(3,OUTPUT);
  pinMode(2,OUTPUT);
  
  cli();//disable interrupts
  
  //set up continuous sampling of analog pin 0 at 38.5kHz
 
  //clear ADCSRA and ADCSRB registers
  ADCSRA = 0;
  ADCSRB = 0;
  
  ADMUX |= (1 << REFS0); //set reference voltage
  ADMUX |= (1 << ADLAR); //left align the ADC value- so we can read highest 8 bits from ADCH register only
  
  ADCSRA |= (1 << ADPS2) | (1 << ADPS0); //set ADC clock with 32 prescaler- 16mHz/32=500kHz
  ADCSRA |= (1 << ADATE); //enabble auto trigger
  ADCSRA |= (1 << ADIE); //enable interrupts when measurement complete
  ADCSRA |= (1 << ADEN); //enable ADC
  ADCSRA |= (1 << ADSC); //start ADC measurements
  
  sei();//enable interrupts
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.setTextSize(1);
  display.setTextColor(WHITE);

  display.setCursor(0,0);
  display.clearDisplay();
  display.println("acople o motor a uma tarracha de seu instrumento e toque a corda para afinar");
  //display.println("oi");
  display.display();
  delay(2000);
  display.clearDisplay();
}

ISR(ADC_vect) {//when new ADC value ready
  
  PORTB &= B11101111;//set pin 12 low
  prevData = newData;//store previous value
  newData = ADCH;//get value from A0
  if (prevData < 127 && newData >=127){//if increasing and crossing midpoint
    newSlope = newData - prevData;//calculate slope
    if (abs(newSlope-maxSlope)<slopeTol){//if slopes are ==
      //record new data and reset time
      slope[index] = newSlope;
      timer[index] = time;
      time = 0;
      if (index == 0){//new max slope just reset
        PORTB |= B00010000;//set pin 12 high
        noMatch = 0;
        index++;//increment index
      }
      else if (abs(timer[0]-timer[index])<timerTol && abs(slope[0]-newSlope)<slopeTol){//if timer duration and slopes match
        //sum timer values
        totalTimer = 0;
        for (byte i=0;i<index;i++){
          totalTimer+=timer[i];
        }
        period = totalTimer;//set period
        //reset new zero index values to compare with
        timer[0] = timer[index];
        slope[0] = slope[index];
        index = 1;//set index to 1
        PORTB |= B00010000;//set pin 12 high
        noMatch = 0;
      }
      else{//crossing midpoint but not match
        index++;//increment index
        if (index > 9){
          reset();
        }
      }
    }
    else if (newSlope>maxSlope){//if new slope is much larger than max slope
      maxSlope = newSlope;
      time = 0;//reset clock
      noMatch = 0;
      index = 0;//reset index
    }
    else{//slope not steep enough
      noMatch++;//increment no match counter
      if (noMatch>9){
        reset();
      }
    }
  }
  
  time++;//increment timer at rate of 38.5kHz
  
  ampTimer++;//increment amplitude timer
  if (abs(127-ADCH)>maxAmp){
    maxAmp = abs(127-ADCH);
  }
  if (ampTimer==1000){
    ampTimer = 0;
    checkMaxAmp = maxAmp;
    maxAmp = 0;
  }

  
}

void reset(){//clean out some variables
  index = 0;//reset index
  noMatch = 0;//reset match couner
  maxSlope = 0;//reset slope
}

//Determine the correct frequency and light up 
//the appropriate LED for the string being played 
void stringCheck(){
  display.setCursor(0, 0);
  display.setTextSize(1);
  if(frequency>70&frequency<90){
    /*printar no display "corda E"*/
    display.println("corda E");
    correctFrequency = 82.4;
  }
  else if(frequency>100&frequency<120){
    /*printar no display "corda A"*/
    display.println("corda A");
    correctFrequency = 110;
  }
  else if(frequency>135&frequency<155){
    /*printar no display "corda D"*/
    display.println("corda D");
    correctFrequency = 146.8;
  }
  else if(frequency>186&frequency<205){
    /*printar no display "corda G"*/
    display.println("corda G");
    correctFrequency = 196;
  }
  else if(frequency>235&frequency<255){
    /*printar no display "corda B"*/
    display.println("corda B");
    correctFrequency = 246.9;
  }
  else if(frequency>320&frequency<340){
    /*printar no display "corda e"*/
    display.println("corda e");
    correctFrequency = 329.6;
  }
  else//detectou um valor de frequencia discrepante
  {
    correctFrequency = 0;
  }
}

/*void tempoDeGiro()
{
  double danilo = fabs(correctFrequency-frequency)/correctFrequency;
  delay(1000*danilo);
  allLEDsOff();
}
*/

//Compare the frequency input to the correct 
//frequency and light up the appropriate LEDS
void frequencyCheck(){
    if (correctFrequency == 0)
    {
      allLEDsOff();
      //Serial.println("frequencia discrepante");
    }
    //verifica se a diferença entre a frequencia da guitarra e a frequencia alvo é maior que 1
    else if (frequency > correctFrequency + 2){
      digitalWrite(2,1);
      Serial.println("2");
      display.drawBitmap(54, 10, setabaixo, 16, 16, WHITE);
      //tempoDeGiro();
    }
    else if (frequency < correctFrequency - 2){
      digitalWrite(3,1);
      Serial.println("3");
      display.drawBitmap(54, 10, setacima, 16, 16, WHITE);
      //tempoDeGiro();
    }  
    else {  //if(frequency>correctFrequency-1&frequency<correctFrequency+1)
      /*Está afinada*/
      display.setCursor(10, 10);
      display.setTextSize(2);
      display.println("afinada!");
      Serial.println("afinada");
      allLEDsOff();
    }
}

void allLEDsOff(){
  digitalWrite(2,0);
  digitalWrite(3,0);
}

void loop(){
  display.clearDisplay();

  allLEDsOff();
  frequency = 0;
  
  if (checkMaxAmp>ampThreshold){
    frequency = 38462/float(period);//calculate frequency timer rate/period
  }
  
  stringCheck();

  frequencyCheck();
  Serial.print(frequency);
  Serial.println();
  
  display.display();
  delay(1000);
 
}
