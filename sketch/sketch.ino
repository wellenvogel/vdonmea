/**
 * sketch to read the wind data from a VDO
 * (C) Andreas Vogel, www.wellenvogel.de
 * License: MIT
 */
#include "Settings.h"

#define IPIN 2
//pins - grey: vref (~8V), green,yellow: direction
#define GREY_PIN A2
#define YELLOW_PIN A1
#define GREEN_PIN  A0
//used NMEA talker id
#define TALKER_ID "GP"
//NMEA output speed
#define SPEED 19200
//delay between 2 send outs
#define INTERVAL 500
//show debug info on the serial interface (should be no problem normally)
#define SHOW_TEXT 1

//resistors used to divide the input voltage
//we assume similar ones for green and yellow
//the division should consider a low power on the arduino
//so in this set up input 6v will give us 3V at the arduino pins
#define R1 4.7
#define R2 4.7

//grey has different resistors to bringe the voltage more down
//with this set up we lower it to 1/3 - app. 2.75V for our voltage
#define RGREY1 6.6
#define RGREY2 3.3

//the measured reference voltage (grey line) - this is used as the reference for our voltages
#define VREF 8.23
/**
 * computation of angles
 * we start with some min/max values - but the will adapt
 * yellow is sin, green is cos
 * from the voltages we compute an angle and afterwards we average
 */



//values for min max when going to prog
#define PROG_MAX 3.1
#define PROG_MIN 3.0

const char DELIMITER[] = " ";
volatile unsigned long icount;
unsigned long lastCount;
unsigned long ts;
unsigned long lastOutput;
char nmeabuf[100];
#define MAXLINE 100
char receive[MAXLINE+1];
int receivedBytes=0;

void isr(){
  icount++;
}

#define ANALOG_RESOLUTION_BITS 10

float analogResolution = 1 << ANALOG_RESOLUTION_BITS;
float factor=(R1+R2)/R2/analogResolution;
float factorGrey=(RGREY1+RGREY2)/RGREY2/analogResolution;

bool progMode=true;

//ignore voltages lower then this
#define MIN_MEASURE 0.5

//balance for averageing between cos and sin
//this is the weight if the value is at 0, if the value ist at 1 the weight is 1
//this honors that values around 0 are more precise that at 1
#define AVMAX 10


float amp=1;

Settings settings;



float hzToKn(float hz){
  return hz*settings.currentValues.hztokn;
}


void setup() {
  icount=0;
  lastCount=0;
  attachInterrupt(digitalPinToInterrupt(IPIN),isr,RISING);
  Serial.begin(SPEED);
  ts=millis();
  lastOutput=ts;
  progMode=false;
  settings.loadValues();
  amp=(settings.currentValues.maxValue-settings.currentValues.minValue)/2;
}

byte toHex(byte c){
  c=c & 0xf;
  if (c <= 9) return '0'+c;
  else return 'A'+c-10;
}

const char * formatNmea(float windKn,float angle){
  char speeds[10];
  char angles[10];
  dtostrf(windKn,1,1,speeds);
  dtostrf(angle,1,1,angles);
  sprintf(nmeabuf,"$%sMWV,%s,R,%s,N,A",TALKER_ID,angles,speeds);
  byte crc=0;
  for (byte i=1;i<strlen(nmeabuf);i++){
    crc = crc ^ nmeabuf[i];
  }
  sprintf(nmeabuf+strlen(nmeabuf),"*%c%c",toHex(crc>>4),toHex(crc));
  return nmeabuf;
}




float handleMinMax(float v){
  bool hasChanged=false;
  if (v > settings.currentValues.maxValue){
    if (! progMode ){
      v=settings.currentValues.maxValue;
    }
    else{
      settings.currentValues.maxValue=v;
      hasChanged=true;
    }
  }
  if (v < settings.currentValues.minValue){
    if (! progMode || v < MIN_MEASURE){
      v=settings.currentValues.minValue;
    }
    else{
      settings.currentValues.minValue=v;
      hasChanged=true;
    }
  }
  if (hasChanged){
    amp=(settings.currentValues.maxValue-settings.currentValues.minValue)/2;
  }
  return v;
}



float getWeight(float v){
  if (v < 0) v = -v;
  v= (1-v) * (AVMAX-1) +1;
  return v;
}
#define MAXVAL 0.99999
float computeAngle(){
  float grey=(float)analogRead(GREY_PIN)*factorGrey;
  float windAngle=0;
  float scale=1.0;
  if (grey < 0.01){
    Serial.print("grey to small");
    Serial.println();
    return 0.0;
  }
  else{
    scale=VREF/grey;
  }
  int green=analogRead(GREEN_PIN);
  float vgreen=factor*green*scale;
  int yellow=analogRead(YELLOW_PIN);
  float vyellow=factor*yellow*scale;

  float vyellowC=handleMinMax(vyellow);
  float vgreenC=handleMinMax(vgreen);
  float vgscaled=(vgreenC-settings.currentValues.minValue)/amp-1;
  if (vgscaled >= 1) vgscaled=MAXVAL;
  if (vgscaled <= -1) vgscaled=-MAXVAL;
  float vyscaled=(vyellowC-settings.currentValues.minValue)/amp-1;
  if (vyscaled >= 1) vyscaled=MAXVAL;
  if (vyscaled <= -1) vyscaled=-MAXVAL;

  float weighty=getWeight(vyscaled);
  float weightg=getWeight(vgscaled);
  //now the quadrants 
  //asin from -90...90
  //acos from 0...180
  float wsin=180/PI*asin(vyscaled);
  float wcos=180/PI*acos(vgscaled);
  //1. 0...90 wsin ok, wcos ok (both 0...90)
  if (vyscaled >=0 && vgscaled >=0){
    if (wcos >= 90) wcos=180-wcos;
  } else
  //2. 90...180 wcos ok, wsin correct 
  if (vyscaled >=0 && vgscaled <=0) {
    if (wsin < 90) wsin=180.0-wsin;
  } else
  //3. -180...-90
  if (vyscaled < 0 && vgscaled < 0){
     wsin=-180-wsin;
     wcos=-wcos;
  }else
  //4. -90...0 wsin ok, wcos correct
  if (vyscaled <0 && vgscaled >=0){
    wcos=-wcos;
  }
  float wfinal=(weighty *wsin+ weightg*wcos)/(weighty+weightg) + settings.currentValues.offset;
  if (SHOW_TEXT){
    Serial.print(grey);
    Serial.print(", scale=");
    Serial.print(scale);
    Serial.print(", yellow=");
    Serial.print(vyellow);
    Serial.print(", green=");
    Serial.print(vgreen);
    Serial.print(" ,");
    Serial.print(green);
    Serial.print(", min=");
    Serial.print(settings.currentValues.minValue);
    Serial.print(", max=");
    Serial.print(settings.currentValues.maxValue);
    Serial.print(", amp=");
    Serial.print(amp);
    Serial.print(", asin=");
    Serial.print(wsin);
    Serial.print(", acos=");
    Serial.print(wcos);
    Serial.print(", wfinal=");
    Serial.print(wfinal);
    Serial.println();
  }
}


void handleSerialLine(const char *receivedData) {
  char * tok = strtok(receivedData, DELIMITER);
  if (! tok) return;
  int num=0;
  if (strcasecmp(tok, "XXPROG") == 0) {
    progMode=true;
    settings.currentValues.minValue=PROG_MIN;
    settings.currentValues.maxValue=PROG_MAX;
    Serial.println("PROG started");
    return;
  }
  if (strcasecmp(tok,"XX0") == 0){
    if (progMode){
      Serial.print("CURRENT ");
      float current=computeAngle();
      settings.currentValues.offset=-current;
    }
  }
  if (strcasecmp(tok,"XXRST") == 0){
    Serial.println("RELOAD");
    progMode=false;
    settings.reload();    
  }
  if (strcasecmp(tok, "XXSAVE") == 0) {
    Serial.println("SAVE");
    settings.write();
    progMode=false;
    return;
  }
}

void loop() {
  unsigned long currentTime=millis();
  if (currentTime <= ts){
    //overflow, skip this
    ts=currentTime;
    lastOutput=ts;
    return;
  }
  while (Serial.available() > 0) {
    int c = Serial.read();
    if (c < 0) break;
    if (c < 0x20 || receivedBytes >= MAXLINE){
      receive[receivedBytes]=0;
      handleSerialLine(receive);
      receivedBytes=0;
      break;
    }
    receive[receivedBytes]=c;
    receivedBytes++;
  }
  if ((currentTime-lastOutput) < INTERVAL){
    return;
  }
  lastOutput=currentTime;
  unsigned long diff=currentTime-ts;
  ts=currentTime;
  unsigned long currentCount=icount;
  if (currentCount < lastCount){
    //overflow - skip
    lastCount=currentCount;
    return;
  }
  unsigned long countDiff=currentCount-lastCount;
  lastCount=currentCount;
  float freq=(float)countDiff*1000/(float)diff;
  float windKn=hzToKn(freq);
  if (SHOW_TEXT){
    if (progMode) Serial.print("[PROG] ");
    Serial.print("tdiff=");
    Serial.print(diff);
    Serial.print(", freq=");
    Serial.print(freq);
    Serial.print(", kn=");
    Serial.print(windKn);
    Serial.print(", greyBase=");    
  }
  float wfinal=computeAngle();
  Serial.print(formatNmea(windKn,wfinal));
  Serial.println();
  
}
