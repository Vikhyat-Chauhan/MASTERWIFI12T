/*
*  Name: MASTERWIFI12T
*  ChipId : NONE
*  Date Created: 24/11/2020
*  Date Latest Modified : 08/01/2021
*  Version : 5.0
*  Description: 
*  Updates : Added Sinric Alexa Script via MQTT Credentials.
*  Fixes : Increased Scene and Timer data to max 4
*  See: https://www.thenextmove.in
*  The Relay writing function is not in this code. NO is on master.slave.relay[i].current_state = 0
*/

#ifndef HomeHub_h
#define HomeHub_h

/* Setup debug printing macros. */
#define HomeHub_DEBUG

#define HomeHub_DEBUG_PORT Serial
#define HomeHub_DEBUG_PORT_BAUD 74880

#define HomeHub_SLAVE_DATA_PORT Serial
#define HomeHub_SLAVE_DATA_PORT_BAUD 74880

//Button special features
#define BUTTON_LONGPRESSDELAY 5000
#define BUTTON_MULTIPRESSTIME 3000
#define BUTTON_MULTIPRESSCOUNT 7
#define BUTTON_TYPE true

//timer
#define TIMER_NUMBER 4
#define TIMER_MEMSPACE 124

//Scene
#define SCENE_NUMBER 4
#define SCENE_MEMSPACE 104  //250

//Relay
#define RELAY_FIX_NUMBER 4
#define RELAY_MAX_NUMBER 5
#define RELAY_MEMSPACE 100

//Fan
#define FAN_FIX_NUMBER 0
#define FAN_MAX_NUMBER 5

//Sensor
#define SENSOR_FIX_NUMBER 0
#define SENSOR_MAX_NUMBER 0

//Button
#define BUTTON_FIX_NUMBER 1
#define BUTTON_MAX_NUMBER 1

//Sinric data
#define HEARTBEAT_INTERVAL 300000 // 5 Minutes 

#include "Arduino.h"
#include <ArduinoJson.h>      //Sinric and Groundnet API Dependancy
#include <WebSocketsClient.h> //Sinric Dependancy 
#include <StreamString.h>     //Sinric Dependancy
#include <ESP8266mDNS.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266httpUpdate.h>
#include <WiFiClient.h>
#include <PubSubClient.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <ESP8266WiFi.h>
#include <EEPROM.h>
#include <pgmspace.h>
#include <Wire.h>
#include <DS3231.h>
#include "FS.h"
#include "eepromi2c.h"
#include <vector>

#ifdef ESP8266
#include <functional>

#define MQTT_CALLBACK_SIGN std::function<void(String)> mqttCallback
#else
#define MQTT_CALLBACK_SIGN void (*mqttCallback)(String)
#endif

#ifdef HomeHub_DEBUG
#define HomeHub_DEBUG_PRINT(...) do {/*HomeHub_DEBUG_PORT.print("[HomeHub] : "); HomeHub_DEBUG_PORT.print( __VA_ARGS__ );HomeHub_DEBUG_PORT.println("");*/} while (0)
#else
#define HomeHub_DEBUG_PRINT(...)
#endif

#define ROM_ADDRESS 0x57


typedef struct{
  bool current_state;
  bool active = false;
  int8_t hour;
  int8_t minute;
  int8_t week;
}ONTIMER;

typedef struct{
  bool current_state;
  bool active = false;
  int8_t hour;
  int8_t minute;
  int8_t week;
}OFFTIMER;

typedef struct{
    bool current_state; 
    ONTIMER ontimer;
    OFFTIMER offtimer;
    bool relay[RELAY_MAX_NUMBER];
    bool fan[FAN_MAX_NUMBER];
    bool change = false;
}TIMER;                                 

typedef struct{
    bool current_state;       
    bool trigger = false;
    bool relay[RELAY_MAX_NUMBER];
    bool fan[FAN_MAX_NUMBER];
    bool change = false;
}SCENE;                                

typedef struct{
    bool type;
    bool current_state;
    bool previous_state;
    bool longpress;
    bool lastmqttcommand = false;
    bool lastslavecommand = false;
    int8_t pin;
    int8_t press_counter;
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
}RELAY;

typedef struct{
    bool current_state;
    bool previous_state;
    int8_t current_value;
    int8_t previous_value;
    bool change = false;
    bool lastmqttcommand = false;
    bool lastslavecommand = false;
}FAN;

typedef struct{
    const char * type;
    int current_value;
    int previous_value;
    bool change = false;
    bool lastmqttcommand = false;
    bool lastslavecommand = false;
}SENSOR;

typedef struct{
    const char* NAME;
    int8_t BUTTON_NUMBER = BUTTON_FIX_NUMBER;
    int8_t RELAY_NUMBER = RELAY_FIX_NUMBER;
    int8_t FAN_NUMBER = FAN_FIX_NUMBER;
    int8_t SENSOR_NUMBER = SENSOR_FIX_NUMBER;
    //int8_t BUTTON_PIN[BUTTON_FIX_NUMBER] = {12};
    //int8_t RELAY_PIN[RELAY_FIX_NUMBER] = {13,12,14,0};
    const char* RELAY_SINRIC_ID[RELAY_MAX_NUMBER] ;
    bool all_relay_change = false;
    bool all_fan_change = false;
    bool all_sensor_change = false;
    bool all_relay_lastslavecommand = false;
    bool all_fan_lastslavecommand = false;
    bool all_sensor_lastslavecommand = false;
    bool all_relay_lastmqttcommand = false;
    bool all_fan_lastmqttcommand = false;
    bool all_sensor_lastmqttcommand = false;
    BUTTON button[BUTTON_MAX_NUMBER];
    RELAY relay[RELAY_MAX_NUMBER];
    FAN fan[FAN_MAX_NUMBER];
    SENSOR sensor[SENSOR_MAX_NUMBER];
    bool change = false;
}SLAVE;

typedef struct{
  //Time Sensor Variables
  int8_t date,month,year,week,hour,minute,second;
  //Time Sensor Variables
  int8_t temp;
  int8_t error_code;
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
    bool receiving_json = false;
    bool received_json = false;
    int8_t sinric_restart = 0;
}FLAG;

typedef struct{
  bool initiate_memory = false;
  bool wifi_handler = false;
  bool initiate_mqtt = false;
}FUNCTIONSTATE;

typedef struct{
  CLOCK clock;
  FLAG flag;
  TIMER timer[TIMER_NUMBER];
  SCENE scene[SCENE_NUMBER];
  String SINRICAPI;//Fill it with a dummy value to prevent any error when mqtt sinric handshake is not done
  String SINRICRELAYID[RELAY_MAX_NUMBER];
}SYSTEM;

typedef struct{
    const char* NAME = "MASTERWIFI12T";
    const char* VERSION = "5.0";
    bool change = false;
    SYSTEM system;
    SLAVE slave;
}MASTER;

//const char HTTP_EN[] PROGMEM = "</div></body></html>";

class HomeHub{
  public:
    HomeHub();
    FUNCTIONSTATE functionstate;
    //Master struct variable
    MASTER master;
        //Used for setting up MQTT callback functions
        typedef void (*callback_with_arg_t)(void*);
        typedef std::function<void(void)> callback_function_t;
        //Public functions are defined here
        void asynctasks();
        //Data Management Fucntions
        void rom_write_page(unsigned int eeaddresspage, byte* data, byte length);
        void rom_write(unsigned int eeaddress, byte data, bool external);
        byte rom_read(unsigned int eeaddress, bool external);
        //Wifi AP Setup Functions
        bool initiate_wifi_setup();
        bool end_wifi_setup();
        //Wifi Functions
        void wifi_handler();
        bool saved_wifi_connect();
        bool manual_wifi_connect(const char* wifi, const char* pass);
        void saved_wifi_dump();
        void save_wifi_data(String ssid,String password);
        //Mqtt Functions
        bool initiate_mqtt();
        void end_mqtt();
        void mqttcallback(char* topic, byte* payload, unsigned int length);
        HomeHub& setCallback(MQTT_CALLBACK_SIGN); HomeHub& setInterrupt();
        void publish_mqtt(String message, bool retain);
        void publish_mqtt(String topic, String message, bool retain);
        void update_device();
        void update_device(String host_update);
        
        void button_handler();

  private:
        //std::unique_ptr<MDNSResponder> mdns;
        //std::unique_ptr<WiFiServer> server;
        MDNSResponder* mdns;
        WiFiServer* server;
    
    
        //Wifi Variables
        String _ssid_string = "TNM" + String(ESP.getChipId());
        String _wifi_data = "";
        String _esid = "";
        String _epass = "";
        int _wifi_data_memspace = 0;
        
    
        //Mqtt Variables
        String error = "000";
        String LastCommand = "";
        String Device_Id_As_Subscription_Topic = (String)ESP.getChipId()+"ESP/#";
        String Device_Id_As_Publish_Topic = (String)ESP.getChipId()+"/";
        char Device_Id_In_Char_As_Subscription_Topic[20];
        char Device_Id_In_Char_As_Publish_Topic[22];
        unsigned long previous_millis = 0;
        const char* mqtt_server = "15.206.160.77";//"m12.cloudmqtt.com";//"api.sensesmart.in";
        const char* mqtt_clientname;
        const char* mqtt_serverid = "thenextmove";//"wbynzcri"; //"u_global";
        const char* mqtt_serverpass = "t1n2m3@TNM";//"uOIqIxMgf3Dl"; //"p_global";
        const int mqtt_port = 1883;//12233;//1883;
        MQTT_CALLBACK_SIGN;

        //Sinric variables 
        uint64_t heartbeatTimestamp = 0;
        bool isConnected = false;
    
        //Device Update
        const String _host_update = "http://us-central1-sense-smart.cloudfunctions.net/update";
        const String _firmware_version = "3";
    
        //Slave Handling Variables
        unsigned int _SLAVE_DATA_PORT_counter = 0;
        String _SLAVE_DATA_PORT_command = "";
        String _slave_command_buffer = "";          //Has limited information capacity(Name,role,command,4R2F1S)
    
        //millis variables for async tasks
        unsigned long _slave_handshake_millis = 0;

        //Private Functions
        void initiate_structures();
        void initiate_memory();
        void initiate_devices();
        String mdns_input_handler(String req);
        int mdns_handler();
        bool mqtt_input_handler(String topic,String payload);
        void mqtt_output_handler();
        void mqtt_handshake_handler();
        void device_handler();
        void start_server();
        bool stop_server();
        bool retrieve_wifi_data();
        bool test_wifi();
        String scan_networks();
        void timesensor_handler();
        void timer_handler();
        void scene_handler();
        void notification_handler(String topic,String payload);

        //Sinric functions
        void webSocketEvent(WStype_t type, uint8_t * payload, size_t length); 
        void sinric_handler();
        void sinric_SPIFFS_read();
        void sinric_SPIFFS_write();
        
        //Slave Handling Functions
        void serial_reading();
        void slave_input_handler();
        void slave_output_handler();
        bool slave_handshake_handler();
        void salve_serial_capture();
        void slave_receive_command(const char* command);
};

#endif
