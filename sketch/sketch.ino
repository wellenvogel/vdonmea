/**
 * sketch to read the wind data from a VDO
 * (C) Andreas Vogel, www.wellenvogel.de
 * License: MIT
 */


#define IPIN 2
//pins - gry: vref (~8V), green,yellow: direction
#define GREY_PIN A2
#define YELLOW_PIN A1
#define GREEN_PIN  A0
#define TALKER_ID "GP"
#define SPEED 19200
#define INTERVAL 500
#define SHOW_TEXT 1
volatile unsigned long icount;
unsigned long lastCount;
unsigned long ts;
char nmeabuf[100];

void isr(){
  icount++;
}

#define HZTOKN (1/1.63)
//see https://www.segeln-forum.de/board194-boot-technik/board35-elektrik-und-elektronik/board195-open-boat-projects-org/75527-reparaturhilfe-f%C3%BCr-vdo-windmessgeber/
float hzToKn(float hz){
  return hz*HZTOKN;
}

#define R1 4.7
#define R2 4.7

#define RGREY1 6.6
#define RGREY2 3.3
#define ANALOG_RESOLUTION_BITS 10
//expected voltage at grey pin - relate all measures against this
#define VREF 8.23
float analogResolution = 1 << ANALOG_RESOLUTION_BITS;
float factor=(R1+R2)/R2/analogResolution;
float factorGrey=(RGREY1+RGREY2)/RGREY2/analogResolution;



void setup() {
  icount=0;
  lastCount=0;
  // put your setup code here, to run once:
  attachInterrupt(digitalPinToInterrupt(IPIN),isr,RISING);
  Serial.begin(SPEED);
  ts=millis();
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

/**
 * computation of angles
 * we start with some min/max values - but the will adapt
 * yellow is sin, green is cos
 * from the voltages we compute an angle and afterwards we average
 */
#define INITIAL_MIN 2.12
#define INITIAL_MAX 6.0 
//will be added to final result
#define FINAL_OFFSET -5.0 


#define ALLOWED_CHANGE 0.3
#define CHG_MA 0.1

float minValue=INITIAL_MIN;
float maxValue=INITIAL_MAX;
float amp=(maxValue-minValue)/2*1.002;

float handleMinMax(float v){
  bool hasChanged=false;
  if (v > maxValue){
    if (maxValue >= (INITIAL_MAX + ALLOWED_CHANGE) || v > (INITIAL_MAX + ALLOWED_CHANGE) ){
      v=maxValue;
    }
    else{
      maxValue=maxValue+CHG_MA*(v-maxValue);
      hasChanged=true;
      if (v>maxValue) v=maxValue;
    }
  }
  if (v < minValue){
    if (minValue <= (INITIAL_MIN - ALLOWED_CHANGE) || v < (INITIAL_MIN - ALLOWED_CHANGE)){
      v=minValue;
    }
    else{
      minValue=minValue - CHG_MA*(minValue-v);
      hasChanged=true;
      if (v < minValue) v=minValue;
    }
  }
  if (hasChanged){
    amp=(maxValue-minValue)/2*1.002;
  }
  return v;
}

#define MAXVAL 0.99999
void loop() {
  // put your main code here, to run repeatedly:
  delay(INTERVAL);
  unsigned long currentTime=millis();
  if (currentTime <= ts){
    //overflow, skip this
    ts=currentTime;
    return;
  }
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
  float grey=(float)analogRead(GREY_PIN)*factorGrey;
  float windAngle=0;
  float scale=1.0;
  if (grey < 0.01){
    Serial.print("grey to small");
    Serial.println();
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
  float vgscaled=(vgreenC-minValue)/amp-1;
  if (vgscaled >= 1) vgscaled=MAXVAL;
  if (vgscaled <= -1) vgscaled=-MAXVAL;
  float vyscaled=(vyellowC-minValue)/amp-1;
  if (vyscaled >= 1) vyscaled=MAXVAL;
  if (vyscaled <= -1) vyscaled=-MAXVAL;
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
  float wfinal=(wsin+wcos)/2 + (FINAL_OFFSET);
  
  if (SHOW_TEXT){
    Serial.print("tdiff=");
    Serial.print(diff);
    Serial.print(", freq=");
    Serial.print(freq);
    Serial.print(", kn=");
    Serial.print(windKn);
    Serial.print(", greyBase=");
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
    Serial.print(minValue);
    Serial.print(", max=");
    Serial.print(maxValue);
    Serial.print(", amp=");
    Serial.print(amp);
    Serial.print(", asin=");
    Serial.print(wsin);
    Serial.print(", acos=");
    Serial.print(wcos);
    Serial.println();
  }
  Serial.print(formatNmea(windKn,wfinal));
  Serial.println();
  
}
