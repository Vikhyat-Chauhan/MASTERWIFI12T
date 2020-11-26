#include "HomeHub.h"

HomeHub homehub;
//Saving relay data for sleep wakeup
bool relay_state[RELAY_FIX_NUMBER];
unsigned long millis_stamp;

void setup(){
  //attachInterrupt(digitalPinToInterrupt(homehub.master.slave.BUTTON_PIN[0]), buttonhandler, FALLING);
} 

void loop(){
  // Run the on function
  homehub.asynctasks();
}

ICACHE_RAM_ATTR void buttonhandler(){
  if((millis() - millis_stamp) > 150){ Serial.println("Button Pressed");
  //button his now left after pressing, can change device state here
  int total_relays_on = 0;
  for(int i=0;i<homehub.master.slave.RELAY_NUMBER;i++){  //check how many relays are turned on.
    if(homehub.master.slave.relay[i].current_state){
      total_relays_on++;
    }
   }
  if(total_relays_on == 0){                     //none of the relays are turned i.e device is already off
    for(int i=0;i<homehub.master.slave.RELAY_NUMBER;i++){ Serial.println("no relays on");
      homehub.master.slave.relay[i].current_state = relay_state[i];
      //digitalWrite(homehub.master.slave.RELAY_PIN[i],homehub.master.slave.relay[i].current_state);
    }
  }
  else{                                         //some relays are on, turn all of them off
    for(int i=0;i<homehub.master.slave.RELAY_NUMBER;i++){ Serial.println("some were on");
      relay_state[i] = homehub.master.slave.relay[i].current_state;
      homehub.master.slave.relay[i].current_state = false;
      //digitalWrite(homehub.master.slave.RELAY_PIN[i],homehub.master.slave.relay[i].current_state);
    }
  }
  millis_stamp = millis();
  }
}
