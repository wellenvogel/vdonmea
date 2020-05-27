/**
 * sketch to read the wind data from a VDO
 * (C) Andreas Vogel, www.wellenvogel.de
 * License: MIT
 */

#define IPIN 2
#define TALKER_ID "GP"
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

void setup() {
  icount=0;
  lastCount=0;
  // put your setup code here, to run once:
  attachInterrupt(digitalPinToInterrupt(IPIN),isr,RISING);
  Serial.begin(4800);
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


void loop() {
  // put your main code here, to run repeatedly:
  delay(1000);
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
  Serial.print("tdiff=");
  Serial.print(diff);
  Serial.print(", freq=");
  Serial.print(freq);
  Serial.print(", kn=");
  Serial.print(windKn);
  Serial.println();
  Serial.print(formatNmea(windKn,0));
  Serial.println();
  
}
