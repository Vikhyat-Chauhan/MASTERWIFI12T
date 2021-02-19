#include "HomeHub.h"

HomeHub homehub;

int8_t inputButton_pin = 12;
int8_t statusLed_pin = 2;

void setup(){
  //pinMode(inputButton_pin,INPUT_PULLUP);
  pinMode(statusLed_pin,OUTPUT);digitalWrite(statusLed_pin,LOW);//delay(100);digitalWrite(statusLed_pin,LOW);delay(100);digitalWrite(statusLed_pin,HIGH);
  //attachInterrupt(digitalPinToInterrupt(inputButton_pin), buttonhandler, FALLING);
} 

void loop(){
  // Run the on function
  homehub.asynctasks();
  if(homehub.master.system.flag.slave_handshake){
    digitalWrite(statusLed_pin,HIGH);
  }
  else{
    digitalWrite(statusLed_pin,LOW);
  }
}

ICACHE_RAM_ATTR void buttonhandler(){
  ESP.reset();
}
