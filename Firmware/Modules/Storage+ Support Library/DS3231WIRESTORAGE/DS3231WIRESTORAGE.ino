//#include <Wire.h>
//#include "eepromi2c.h"
#include "homehub.h"

struct config
{
  long targetLat;
  long targetLon;
  float fNum;
  bool first;
  int attempts[3];
} config;

HomeHub homehub;

void setup(){
  //Serial.begin(115200);
  //Serial.println("STARTED");
  
  /*
  //config some of the variables during to be saved. 
  config.targetLat = 4957127;
  config.targetLon = 6743421;
  config.fNum = 2.23;
  config.first = false;
  config.attempts[0] = 0;config.attempts[1] = 1;config.attempts[2] = 2;
  eeWrite(0,config);*/
  
  //delay (100);
  //eeWrite(0,homehub.master);
}

void loop(){
  //homehub.master.test = 22;
  //Serial.println(homehub.master.test);
  //eeRead(0,homehub.master);
  //Serial.println(homehub.master.test);
  delay(500000);
}

void testing(){
  //change the variables within the strucutre to show the read worked
  config.targetLat =0;
  config.targetLon = 0;
  config.first = true;
  config.fNum = 0.0;
  config.attempts[0] = 0;config.attempts[1] = 0;config.attempts[2] = 0;
  
  Serial.print("lat = ");
  Serial.println(config.targetLat);
  Serial.print("lon =");
  Serial.println(config.targetLon);

  Serial.print("first");
  Serial.println(config.first);

  Serial.print("float");
  Serial.println(config.fNum);

  Serial.print("attempts[0] = ");
  Serial.println(config.attempts[0]);

  Serial.print("attempts[1] = ");
  Serial.println(config.attempts[1]);
  
  Serial.print("attempts[2] = ");
  Serial.println(config.attempts[2]);

  eeRead(0,config);
  delay(30);
  
  Serial.print("lat = ");
  Serial.println(config.targetLat);
  Serial.print("lon =");
  Serial.println(config.targetLon);

  Serial.print("first");
  Serial.println(config.first);

  Serial.print("float");
  Serial.println(config.fNum);

  Serial.print("attempts[0] = ");
  Serial.println(config.attempts[0]);

  Serial.print("attempts[1] = ");
  Serial.println(config.attempts[1]);
  
  Serial.print("attempts[2] = ");
  Serial.println(config.attempts[2]);

  delay(50000);
}
