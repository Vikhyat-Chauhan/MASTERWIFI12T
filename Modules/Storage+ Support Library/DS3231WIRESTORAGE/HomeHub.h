
#include "Arduino.h"
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <Wire.h>
#include <DS3231.h>
#include "eepromi2c.h"
#include <vector>

#ifdef ESP8266
#include <functional>

#define ROM_ADDRESS 0x57

typedef struct{
  bool current_state;
  int8_t hour;
  int8_t minute;
  int8_t week;
}ONTIMER;

typedef struct{
  bool current_state;
  int8_t hour;
  int8_t minute;
  int8_t week;
}OFFTIMER;

typedef struct{
    bool type;
    bool current_state;
    bool previous_state;
    bool longpress;
    bool lastmqttcommand = false;
    bool lastslavecommand = false;
    int pin;
    int press_counter;
    bool previous_pressed;
    unsigned long pressed_millis;
}BUTTON;

typedef struct{
    bool current_state;
    bool previous_state;
    int8_t current_value;
    int8_t previous_value;
    bool change = false;
    bool lastmqttcommand = false;
    bool lastslavecommand = false;
    ONTIMER ontimer[2];
    OFFTIMER offtimer[2];
}RELAY;

typedef struct{
    bool current_state;
    bool previous_state;
    unsigned int current_value = 22;
    unsigned int previous_value;
    bool change = false;
    bool lastmqttcommand = false;
    bool lastslavecommand = false;
}FAN;

typedef struct{
    const char* type;
    unsigned int current_value;
    unsigned int previous_value;
    bool change = false;
    bool lastmqttcommand = false;
    bool lastslavecommand = false;
}SENSOR;

typedef struct{
    const char* NAME;
    unsigned int BUTTON_NUMBER;
    unsigned int RELAY_NUMBER;
    unsigned int FAN_NUMBER;
    unsigned int SENSOR_NUMBER;
    bool all_relay_change = false;
    bool all_fan_change = false;
    bool all_sensor_change = false;
    bool all_relay_lastslavecommand = false;
    bool all_fan_lastslavecommand = false;
    bool all_sensor_lastslavecommand = false;
    bool all_relay_lastmqttcommand = false;
    bool all_fan_lastmqttcommand = false;
    bool all_sensor_lastmqttcommand = false;
    BUTTON button[10];
    RELAY relay[10];
    FAN fan[10];
    SENSOR sensor[10];
    bool change = false;
}SLAVE;

typedef struct{
  //Time Sensor Variables
  int date,month,year,week,hour,minute,second;
  //Time Sensor Variables
  int temp;
  int error_code;
  bool error;
  bool h12 = false;
  bool century = false;
  bool pmam;
  byte memspace[3] = {NULL,260,261};
}CLOCK;

typedef struct{
    //async Task control variables
    bool check_update = false;
    bool slave_handshake= false;
    bool boot_check_update = true;
    bool saved_wifi_present = false;
    bool rom_external = true;
    bool set_time = false;
}FLAG;

typedef struct{
  bool initiate_memory = false;
  bool wifi_handler = false;
  bool initiate_mqtt = false;
}FUNCTIONSTATE;

typedef struct{
  CLOCK clock;
  FLAG flag;
}SYSTEM;

typedef struct{
    const char* NAME = "STANDALONE2R2BT";
    const char* VERSION = "0.1";
    bool change = false;
    SYSTEM system;
    SLAVE slave;
}MASTER;

class HomeHub{
  public:
    MASTER master;
    HomeHub(){
      Wire.begin();
      Serial.begin(115200);
      Serial.println();
      Serial.println("HomeHub Library Started");
      Serial.println(master.slave.relay[0].current_value);
      //master.slave.relay[0].current_value = 22;
      //eeWrite(0,master.slave.relay[0]);
      eeRead(0,master.slave.relay[0]);
      Serial.println(master.slave.relay[0].current_value);
    }
  private:
};
#endif
