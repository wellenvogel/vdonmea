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

#define OFFSET(val,base) ((const char *)(&val)-(const char *)(&base)+sizeof(int))
class CurrentValues{
    public:
    float   minValue;
    float   maxValue;
    float   offset;
    float   hztokn;
    CurrentValues(){
        minValue=INITIAL_MIN;
        maxValue=INITIAL_MAX;
        offset=FINAL_OFFSET;
        hztokn=HZTOKN;
    }
  };
class Settings{
  private:
  static const int MAGIC=0xfafb;
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
    float v;
    EEPROM.get(OFFSET(currentValues.minValue,currentValues),v);  
    currentValues.minValue=v;
    EEPROM.get(OFFSET(currentValues.maxValue,currentValues),v);
    currentValues.maxValue=v;
    EEPROM.get(OFFSET(currentValues.offset,currentValues),v);
    currentValues.offset=v;
    EEPROM.get(OFFSET(currentValues.hztokn,currentValues),v);
    currentValues.hztokn=v;
  }


  void write(){
    int magic=MAGIC;
    EEPROM.put(0,magic);
    EEPROM.put(OFFSET(currentValues.minValue,currentValues),currentValues.minValue);   
    EEPROM.put(OFFSET(currentValues.maxValue,currentValues),currentValues.maxValue);
    EEPROM.put(OFFSET(currentValues.offset,currentValues),currentValues.offset);
    EEPROM.put(OFFSET(currentValues.hztokn,currentValues),currentValues.hztokn);
  }
  void reset(bool writeOut=true){
    currentValues=CurrentValues();
    if (writeOut) write();
  }
  void reload(){
    currentValues=CurrentValues();
    loadValues();
  }
};

#endif
