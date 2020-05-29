/**
 * sketch to read the wind data from a VDO
 * (C) Andreas Vogel, www.wellenvogel.de
 * License: MIT
 */
#ifndef SETTINGS_H
#define SETTINGS_H

#include <Arduino.h>
#include <EEPROM.h>
//measured min and max voltages at green and yellow lines
//will auto adjust to higher/lower up to 0.3V
#define INITIAL_MIN 2.145
#define INITIAL_MAX 6.0123 

//will be added to final result (in °)
#define FINAL_OFFSET 0.0 

#define HZTOKN (1/1.63)
//see https://www.segeln-forum.de/board194-boot-technik/board35-elektrik-und-elektronik/board195-open-boat-projects-org/75527-reparaturhilfe-f%C3%BCr-vdo-windmessgeber/

//moving average
#define MAF 0.5
#define OFFSET(val,base) ((const char *)(&val)-(const char *)(&base)+sizeof(int))
#define NUMVALUES 9
class CurrentValues{
    public:
    byte          numValues;
    float         minValue;
    float         maxValue;
    float         offset;
    float         hztokn;
    float         maf;
    unsigned int  interval;
    byte          showText;
    char          talker[2];
    byte          minCount;
    CurrentValues(){
        numValues=NUMVALUES;
        minValue=INITIAL_MIN;
        maxValue=INITIAL_MAX;
        offset=FINAL_OFFSET;
        hztokn=HZTOKN;
        maf=MAF;
        interval=1000;
        showText=1;
        talker[0]='G';
        talker[1]='P';
        minCount=20;
    }
  };
class Settings{
  private:
  static const int MAGIC=0xfafd;
  CurrentValues saved;
  public:
  CurrentValues currentValues;
  Settings(){};
  void loadValues(){    
    int magic=0;
    EEPROM.get(0,magic);
    if (magic != MAGIC) {
      Serial.println("***MAGIC not found ***");
      return;
    }
    EEPROM.get(OFFSET(currentValues.numValues,currentValues),currentValues.numValues);
    float v;
    EEPROM.get(OFFSET(currentValues.minValue,currentValues),v);  
    currentValues.minValue=v;
    EEPROM.get(OFFSET(currentValues.maxValue,currentValues),v);
    currentValues.maxValue=v;
    EEPROM.get(OFFSET(currentValues.offset,currentValues),v);
    currentValues.offset=v;
    EEPROM.get(OFFSET(currentValues.hztokn,currentValues),v);
    currentValues.hztokn=v;
    EEPROM.get(OFFSET(currentValues.maf,currentValues),v);
    currentValues.maf=v;
    EEPROM.get(OFFSET(currentValues.interval,currentValues),currentValues.interval);
    EEPROM.get(OFFSET(currentValues.showText,currentValues),currentValues.showText);
    char x;
    EEPROM.get(OFFSET(currentValues.talker,currentValues),x);
    currentValues.talker[0]=x;
    EEPROM.get(OFFSET(currentValues.talker,currentValues)+1,x);
    currentValues.talker[1]=x;
    if (currentValues.numValues >= 9){
      EEPROM.get(OFFSET(currentValues.minCount,currentValues),currentValues.minCount);
    }
    saved=currentValues;
  }


  void write(){
    int magic=MAGIC;
    EEPROM.put(0,magic);
    EEPROM.put(OFFSET(currentValues.numValues,currentValues),currentValues.numValues);   
    EEPROM.put(OFFSET(currentValues.minValue,currentValues),currentValues.minValue);   
    EEPROM.put(OFFSET(currentValues.maxValue,currentValues),currentValues.maxValue);
    EEPROM.put(OFFSET(currentValues.offset,currentValues),currentValues.offset);
    EEPROM.put(OFFSET(currentValues.hztokn,currentValues),currentValues.hztokn);
    EEPROM.put(OFFSET(currentValues.maf,currentValues),currentValues.maf);
    EEPROM.put(OFFSET(currentValues.interval,currentValues),currentValues.interval);
    EEPROM.put(OFFSET(currentValues.showText,currentValues),currentValues.showText);
    EEPROM.put(OFFSET(currentValues.talker,currentValues),currentValues.talker[0]);
    EEPROM.put(OFFSET(currentValues.talker,currentValues)+1,currentValues.talker[1]);
    EEPROM.put(OFFSET(currentValues.minCount,currentValues),currentValues.minCount);
    saved=currentValues;
  }
  void reset(bool writeOut=true){
    currentValues=CurrentValues();
    if (writeOut) write();
  }
  void reload(){
    currentValues=CurrentValues();
    loadValues();
  }

  bool isDirty(){
    return memcmp(&currentValues,&saved,sizeof(CurrentValues)) != 0;
  }

  void printValues(){
    Serial.print("CV");
    if (isDirty()) Serial.print("*");
    Serial.print(": minValue=");
    Serial.print(currentValues.minValue);
    Serial.print(", maxValue=");
    Serial.print(currentValues.maxValue);
    Serial.print(", offset=");
    Serial.print(currentValues.offset);
    Serial.print(", averageFactor=");
    Serial.print(currentValues.maf);
    Serial.print(", knotsPerHz=");
    Serial.print(currentValues.hztokn);
    Serial.print(", minPules=");
    Serial.print(currentValues.minCount);
    Serial.print(", interval=");
    Serial.print(currentValues.interval);
    char buf[3];
    buf[0]=currentValues.talker[0];
    buf[1]=currentValues.talker[1];
    buf[2]=0;
    Serial.print(", talker=");
    Serial.print(buf);
    Serial.println();
  }
};

#endif
