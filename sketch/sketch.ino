/**
 * sketch to read the wind data from a VDO
 * (C) Andreas Vogel, www.wellenvogel.de
 * License: MIT
 */
#include "Settings.h"
#define FH(x) (const __FlashStringHelper *)(x)
#define PM(n,value) static const PROGMEM char n[]=value


/*#define DEBUG*/
#define IPIN 2
//pins - grey: vref (~8V), green,yellow: direction
#define GREY_PIN A2
#define YELLOW_PIN A1
#define GREEN_PIN  A0
//NMEA output speed
#define SPEED 19200


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
unsigned long lastInput;
char nmeabuf[100];

class TimeCount{
  public:
  unsigned long ts;
  unsigned long count;
  TimeCount(unsigned long count, unsigned long ts){
    this->count=0;
    this->ts=ts;
  }
  TimeCount(){
    count=0;
    ts=0;
  }
};

//how many counters we store
#define MAXCOUNT 10
TimeCount counterValues[MAXCOUNT];
int currentCounterValue;
int maxCounterValue;

#define MAXLINE 100
char receive[MAXLINE+1];
int receivedints=0;

void isr(){
  icount++;
}

#define ANALOG_RESOLUTION_BITS 10

float analogResolution = 1 << ANALOG_RESOLUTION_BITS;
float factor=(R1+R2)/R2/analogResolution;
float factorGrey=(RGREY1+RGREY2)/RGREY2/analogResolution;

float currentAngle;
//ignore voltages lower then this
#define MIN_MEASURE 0.5

//balance for averageing between cos and sin
//this is the weight if the value is at 0, if the value ist at 1 the weight is 1
//this honors that values around 0 are more precise that at 1
#define AVMAX 10

//the amplitude of sin/cos
float amp=1;

#define PROG_DELAY      5000
#define PROG_INTERVAL   1000
//programming mode
bool progMode;
bool progOutput;
bool minMaxMode;


Settings settings;



float hzToKn(float hz){
  return hz*settings.currentValues.hztokn;
}




int toHex(int c){
  c=c & 0xf;
  if (c <= 9) return '0'+c;
  else return 'A'+c-10;
}

const char * formatNmea(float windKn,float angle){
  char speeds[10];
  char angles[10];
  dtostrf(windKn,1,1,speeds);
  dtostrf(angle,1,1,angles);
  sprintf(nmeabuf,"$%c%cMWV,%s,R,%s,N,A",
    settings.currentValues.talker[0],
    settings.currentValues.talker[1],
    angles,speeds);
  int crc=0;
  for (int i=1;i<strlen(nmeabuf);i++){
    crc = crc ^ nmeabuf[i];
  }
  sprintf(nmeabuf+strlen(nmeabuf),"*%c%c",toHex(crc>>4),toHex(crc));
  return nmeabuf;
}


void computeAmplitude(){
  amp=(settings.currentValues.maxValue-settings.currentValues.minValue)/2;
}


float handleMinMax(float v){
  bool hasChanged=false;
  if (v > settings.currentValues.maxValue){
    if (! minMaxMode ){
      v=settings.currentValues.maxValue;
    }
    else{
      settings.currentValues.maxValue=v;
      hasChanged=true;
    }
  }
  if (v < settings.currentValues.minValue){
    if (! minMaxMode || v < MIN_MEASURE){
      v=settings.currentValues.minValue;
    }
    else{
      settings.currentValues.minValue=v;
      hasChanged=true;
    }
  }
  if (hasChanged){
    computeAmplitude();
  }
  return v;
}


float getWeight(float v){
  if (v < 0) v = -v;
  v= (1-v) * (AVMAX-1) +1;
  return v;
}
#define MAXVAL 0.99999
float computeAngle(bool doAverage=true){
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
  float diffraw=wsin-wcos;
  if ((wsin < -170 || wsin > 170) && (wcos < -170 || wcos > 170) && (diffraw < -180 || diffraw > 180)){
    //sin is the better one in this area
    wcos=-wcos;
  }
  float wfinal=(weighty *wsin+ weightg*wcos)/(weighty+weightg) + settings.currentValues.offset;
  float delta=wfinal-currentAngle;
  if ((wfinal > 170 || wfinal < -170) && (currentAngle > 170 || currentAngle < -170) && (delta > 180 || delta < -180) ){
    //special handling around +/-180
    currentAngle=-currentAngle;
  }
  currentAngle=currentAngle + settings.currentValues.maf*(wfinal-currentAngle);
  if (settings.currentValues.showText || progMode){
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
    Serial.print(", angle=");
    Serial.print(currentAngle);
    Serial.println();
  }
  float rt=doAverage?currentAngle:wfinal;
  if (rt > 180) rt=-360+rt;
  if (rt < -180) rt=360+rt;
  return rt;
}

static const PROGMEM char HELP[]="Help:\n"
  "XXPROG             : start prog mode\n"
  "    --- other only in prog mode ---\n"
  "CANCEL             : leave prog mode\n"
  "RELOAD             : reload settings\n"
  "RESET              : load default settings\n"
  "SAVE               : save settings\n"
  "MINMAX             : start/stop min/max detection, slowly rotate vane\n"
  "INTERVAL      ms   : set ouput interval\n"
  "KNOTSPERHZ    fact : set the factor from hz to knots\n"
  "MINPULSE      val  : average to get at least that many pulses (max 10 periods back)\n"
  "AVERAGEFACTOR fact : set the moving average factor for angle\n"
  "SHOWTEXT      0|1  : show debug output in normal mode\n"
  "TALKER        XY   : set talker ID\n"
  "OUTPUT             : toggle output in prog mode\n";

void handleSerialLine(const char *receivedData) {
  char * tok = strtok(receivedData, DELIMITER);
  if (! tok) {
    if (!progMode) return;
    settings.printValues();
    return;
  }
  int num=0;
  if (strcasecmp(tok, "XXPROG") == 0) {
    progMode=true;
    minMaxMode=false;
    progOutput=false;
    Serial.println("PROG started");
    settings.printValues();
    return;
  }
  if (! progMode) return;
  if (strcasecmp(tok,"ZERO") == 0){
      Serial.print("set as offset ");
      float current=computeAngle(false);
      settings.currentValues.offset+=-current;
  }
  if (strcasecmp(tok,"CANCEL") == 0){
    if (settings.isDirty()){
      Serial.println("settings changed, use RELOAD before");
    }
    else{
      progMode=false;
      minMaxMode=false;
    }
  }
  if (strcasecmp(tok,"MINMAX") == 0){
    Serial.print("MINMAX MODE=");
    minMaxMode=!minMaxMode;
    Serial.println(minMaxMode);
    if (minMaxMode){
      settings.currentValues.minValue=PROG_MIN;
      settings.currentValues.maxValue=PROG_MAX;    
    }
  }
  if (strcasecmp(tok,"RELOAD") == 0){
    Serial.println("RELOAD");
    minMaxMode=false;
    settings.reload();
    computeAmplitude();    
  }
  if (strcasecmp(tok,"RESET") == 0){
    Serial.println("RESET");
    
    settings.reset(false);
    computeAmplitude();    
  }
  if (strcasecmp(tok, "SAVE") == 0) {
    Serial.println("SAVE");
    minMaxMode=false;
    settings.write();
  }
  if (strcasecmp(tok,"INTERVAL") == 0 ){
    char *val=strtok(NULL, DELIMITER);
    if (val){
      settings.currentValues.interval=atoi(val);
      Serial.println("INTERVAL");
    }
  }
  if (strcasecmp(tok,"KNOTSPERHZ") == 0 ){
    char *val=strtok(NULL, DELIMITER);
    if (val){
      settings.currentValues.hztokn=atof(val);
      Serial.println("KNOTSPERHZ");
    }
  }
  if (strcasecmp(tok,"MINPULSE") == 0 ){
    char *val=strtok(NULL, DELIMITER);
    if (val){
      settings.currentValues.minCount=atoi(val);
      Serial.println("MINPULSE");
    }
  }
  if (strcasecmp(tok,"AVERAGEFACTOR") == 0 ){
    char *val=strtok(NULL, DELIMITER);
    if (val){
      float fac=atof(val);
      if (fac < 0.1 || fac > 1){
        Serial.println("FACTOR out of range 0.1...1");
      }
      else{
        settings.currentValues.maf=fac;
        Serial.println("AVERAGEFACTOR");
      }
    }
  }
  if (strcasecmp(tok,"SHOWTEXT") == 0 ){
    char *val=strtok(NULL, DELIMITER);
    if (val){
      settings.currentValues.showText=atoi(val) != 0;
      Serial.println("SHOWTEXT");
    }
  }
  if (strcasecmp(tok,"TALKER") == 0 ){
    char *val=strtok(NULL, DELIMITER);
    if (val && strlen(val) >=2){
      settings.currentValues.talker[0]=*val &0x5f;
      settings.currentValues.talker[1]=*(val+1) & 0x5f;
      Serial.println("TALKER");
    }
  }
  if (strcasecmp(tok,"OUTPUT") == 0){
    progOutput=!progOutput;
    Serial.print("OUTPUT=");
    Serial.println(progOutput);
  }
  if (strcasecmp(tok,"HELP") == 0 || strcasecmp(tok,"?") == 0){
    Serial.print(FH(HELP));
  }
  settings.printValues();
}

void addCount(unsigned long count, unsigned long ts){
#ifdef DEBUG
    Serial.print("ADDC c=");
    Serial.print(count);
    Serial.print(", ts=");
    Serial.println(ts);
#endif    
    counterValues[currentCounterValue].count=count;
    counterValues[currentCounterValue].ts=ts;
    currentCounterValue++;
    if (currentCounterValue >= MAXCOUNT) currentCounterValue=0;
    if (currentCounterValue > maxCounterValue) maxCounterValue++;
}

/**
 * get a nicely averaged frequency
 * we would like to have a minimal amount of pulses
 * otherwise we go back in history and average over multiple periods
 */
float getAveragedHz(unsigned long minCount){
    if (maxCounterValue < 2) return 0.0;
    int numEntries=maxCounterValue;
    int i=currentCounterValue-1;
    if (i<0) i=MAXCOUNT-1;
    unsigned long lastCount=counterValues[i].count;
    unsigned long lastTs=counterValues[i].ts;    
    int usedIndex=i;
    while (numEntries >0){        
        i--;
        if (i<0) i=MAXCOUNT-1;
        numEntries--;
        usedIndex=i;
        unsigned long diff=lastCount - counterValues[usedIndex].count;        
#ifdef DEBUG
        Serial.print("AVHZ diff=");
        Serial.print(diff);
        Serial.print(", idx=");
        Serial.println(i);
#endif        
        if (diff >= minCount) break;
    }
    long counterDiff=lastCount-counterValues[usedIndex].count;
    long timeDiff=lastTs-counterValues[usedIndex].ts;
    if (counterDiff <0 || timeDiff <= 0) return 0.0;
    return (float)counterDiff*1000.0/(float)timeDiff; 
}


void setup() {
  icount=0;
  lastCount=0;
  attachInterrupt(digitalPinToInterrupt(IPIN),isr,RISING);
  Serial.begin(SPEED);
  ts=millis();
  lastOutput=ts;
  lastInput=ts;
  progMode=false;
  minMaxMode=false;
  progOutput=false;
  settings.loadValues();
  amp=(settings.currentValues.maxValue-settings.currentValues.minValue)/2;
  currentCounterValue=0;
  maxCounterValue=0;
  addCount(lastCount,ts);
  currentAngle=0;
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
    if (c < 0x20 || receivedints >= MAXLINE){
      lastInput=ts;
      receive[receivedints]=0;
      handleSerialLine(receive);
      receivedints=0;
      break;
    }
    receive[receivedints]=c;
    receivedints++;
  }
  if (! progMode){
    if ((currentTime-lastOutput) < settings.currentValues.interval){
      delay(2);
      return;
    }
  }
  else{
    if ((currentTime-lastInput) < PROG_DELAY){
      delay(2);
      return;
    }
    if ((currentTime - lastOutput) < PROG_INTERVAL || ! (progOutput || minMaxMode)){
      delay(2);
      return;
    }
  }
  lastOutput=currentTime;
  unsigned long currentCount=icount;
  if (currentCount < lastCount){
    //overflow - skip
    lastCount=currentCount;
    return;
  }
  addCount(currentCount,currentTime);
  unsigned long diff=currentTime-ts;
  ts=currentTime;
  lastCount=currentCount;
  float freq=getAveragedHz(settings.currentValues.minCount);
  float windKn=hzToKn(freq);
  if (settings.currentValues.showText || progMode){
    if (progMode) Serial.print("[PROG] ");
    Serial.print("tdiff=");
    Serial.print(diff);
    Serial.print(", count=");
    Serial.print(currentCount);
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
