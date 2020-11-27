#include "HomeHub.h"
#include <Arduino.h>

WiFiClient _Client;
PubSubClient mqttclient(_Client);
DS3231 Clock;
WebSocketsClient webSocket;
WiFiUDP ntpUDP;
// You can specify the time server pool and the offset (in seconds, can be
// changed later with setTimeOffset() ). Additionaly you can specify the
// update interval (in milliseconds, can be changed using setUpdateInterval() ).
NTPClient timeClient(ntpUDP, "pool.ntp.org", 19800,60000);

HomeHub::HomeHub(){
    //Initilize Serial Lines
    HomeHub_DEBUG_PORT.begin(HomeHub_DEBUG_PORT_BAUD);
    //Initiate memory system
    //initiate_structures();
    initiate_memory();
    initiate_devices(); //If Master has no Hardware dont initiate
    //Time Client
    timeClient.begin();
    // Initlize sinric data
    webSocket.begin("iot.sinric.com", 80, "/");
    // event handler
    webSocket.onEvent([this] (WStype_t type, uint8_t * payload, size_t length) { this->webSocketEvent(type, payload, length); });
    webSocket.setAuthorization("apikey", MyApiKey);
    // try again every 5000ms if connection has failed
    webSocket.setReconnectInterval(5000);

    HomeHub_DEBUG_PRINT("");
    HomeHub_DEBUG_PRINT("STARTED");
}

void HomeHub::asynctasks(){
  timesensor_handler();
  timer_handler();
  scene_handler();
  wifi_handler();
  device_handler(); // Should be only placed at end of each loop since it monitors changes 
}

void HomeHub::wifi_handler(){
  if(functionstate.wifi_handler == false){        //Wifi Handler first started, handling setup.
    functionstate.wifi_handler = true;
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(_ssid_string);
    start_server();
    manual_wifi_connect("OnePlus 3","0987654321");
  }
  else{
    if(WiFi.status() != 3){
        if(functionstate.initiate_memory == false){
          initiate_memory();
        }
      if((millis() - previous_millis) > 10000){
          previous_millis = millis();
          retrieve_wifi_data();
          _wifi_data = scan_networks();
        if(master.system.flag.saved_wifi_present == true){
            HomeHub_DEBUG_PRINT("Saved Wifi Present");
            saved_wifi_connect();
          }
      }
    }
    else{ //All network based fucntions here.
        if(!(mqttclient.connected())){
          HomeHub_DEBUG_PRINT("Initiate Mqtt connection");
          initiate_mqtt();               
          //Since the client was disconnected we are resseting slave handshake
          master.system.flag.slave_handshake = false;        
        }
        else{
            slave_input_handler();
            mqttclient.loop();
            slave_output_handler();
            mqtt_output_handler();
        }
        //Sinric handler
        sinric_handler();
        timeClient.update();
      }
    }
  mdns_handler();
}

void HomeHub::sinric_handler(){
  webSocket.loop();
  if(isConnected) {
    uint64_t now = millis();
    // Send heartbeat in order to avoid disconnections during ISP resetting IPs over night. Thanks @MacSass
    if((now - heartbeatTimestamp) > HEARTBEAT_INTERVAL) {
        heartbeatTimestamp = now;
        webSocket.sendTXT("H");          
    }
    //Look for changes and send them to sinric server
    if(master.slave.all_relay_change == true){
      for(int i=0;i<master.slave.RELAY_NUMBER;i++){
        if(master.slave.relay[i].change == true){
          DynamicJsonDocument root(1024);
          root["deviceId"] = master.slave.RELAY_SINRIC_ID[i];
          root["action"] = "setPowerState";
          if(master.slave.relay[i].current_state == true){
            root["value"] = "ON";
          }
          else{
            root["value"] = "OFF";
          }
          StreamString databuf;     
          serializeJson(root, databuf); 
          HomeHub_DEBUG_PRINT("Sending data to Sinric cloud");HomeHub_DEBUG_PRINT(databuf);
          webSocket.sendTXT(databuf);
        }
      }
    }
  }
}

void HomeHub::device_handler(){
  //Check handshake flag to confirm that slave has actually been connected to the master module before monitoring slave changes
  if(master.system.flag.slave_handshake == true){
    int all_relay_change_counter = 0;
    int all_fan_change_counter = 0;
    int all_sensor_change_counter = 0;
    int all_relay_lastslavecommand_counter = 0;
    int all_fan_lastslavecommand_counter = 0;
    int all_sensor_lastslavecommand_counter = 0;
    int all_relay_lastmqttcommand_counter = 0;
    int all_fan_lastmqttcommand_counter = 0;
    int all_sensor_lastmqttcommand_counter = 0;
    
    //activate changes flag of individual relay devices
    for(int i=0;i<master.slave.RELAY_NUMBER;i++){
        if(master.slave.relay[i].lastslavecommand == true){ //Universtal counter for all relay lastslavecommand
            all_relay_lastslavecommand_counter++;
        }
        if(master.slave.relay[i].lastmqttcommand == true){ //Universtal counter for all relay lastslavecommand
            all_relay_lastmqttcommand_counter++;
        }//Universal counter for all relay changes
        if(master.slave.relay[i].current_state != master.slave.relay[i].previous_state){
          master.slave.relay[i].change = true;
          //digitalWrite(master.slave.RELAY_PIN[i],(master.slave.relay[i].current_state));   //Change the system according to relay hardware
          //eeWrite((RELAY_MEMSPACE+(30*i)),master.slave.relay[0+i],master.system.flag.rom_external);
          all_relay_change_counter++;
        }
        else if(master.slave.relay[i].current_value != master.slave.relay[i].previous_value){
          master.slave.relay[i].change = true;
          all_relay_change_counter++;
        }
        else{
            master.slave.relay[i].change = false;
        }
        master.slave.relay[i].previous_state = master.slave.relay[i].current_state;
        master.slave.relay[i].previous_value = master.slave.relay[i].current_value;
    }
    //activate changes flag of individual fan devices
    for(int i=0;i<master.slave.FAN_NUMBER;i++){
        if(master.slave.fan[i].lastslavecommand == true){ //Universtal counter for all fan lastslavecommand
            all_fan_lastslavecommand_counter++;
        }
        if(master.slave.fan[i].lastmqttcommand == true){ //Universtal counter for all fan lastslavecommand
            all_fan_lastmqttcommand_counter++;
        }
        if(master.slave.fan[i].current_state != master.slave.fan[i].previous_state){
            master.slave.fan[i].change = true;
            all_fan_change_counter++;
        }
        else if(master.slave.fan[i].current_value != master.slave.fan[i].previous_value){
            all_fan_change_counter++;
        }
        else{
            master.slave.fan[i].change = false;
        }
        master.slave.fan[i].previous_state = master.slave.fan[i].current_state;
        master.slave.fan[i].previous_value = master.slave.fan[i].current_value;
    }
    //activate changes flag of individual sensor devices
    for(int i=0;i<master.slave.SENSOR_NUMBER;i++){
        if(master.slave.sensor[i].lastslavecommand == true){ //Universtal counter for all sensor lastslavecommand
            all_sensor_lastslavecommand_counter++;
        }
        if(master.slave.sensor[i].lastmqttcommand == true){ //Universtal counter for all sensor lastmqttcommand
            all_sensor_lastmqttcommand_counter++;
        }
        if(master.slave.sensor[i].current_value != master.slave.sensor[i].previous_value){
            master.slave.sensor[i].change = true;
            all_sensor_change_counter++;
        }
        else{
            master.slave.sensor[i].change = false;
        }
        master.slave.sensor[i].previous_value = master.slave.sensor[i].current_value;
    }
    //activate lastslavecommand flag for relay structure wide
    if(all_relay_lastslavecommand_counter == 0){
        master.slave.all_relay_lastslavecommand = false;
    }
    else{
        master.slave.all_relay_lastslavecommand = true;
    }
    //activate lastslavecommand flag for fan structure wide
    if(all_fan_lastslavecommand_counter == 0){
        master.slave.all_fan_lastslavecommand = false;
    }
    else{
        master.slave.all_fan_lastslavecommand = true;
    }
    //activate lastslavecommand flag for sensor structure wide
    if(all_sensor_lastslavecommand_counter == 0){
        master.slave.all_sensor_lastslavecommand = false;
    }
    else{
        master.slave.all_sensor_lastslavecommand = true;
    }
    //activate lastmqttcommand flag for relay structure wide
    if(all_relay_lastmqttcommand_counter == 0){
        master.slave.all_relay_lastmqttcommand = false;
    }
    else{
        master.slave.all_relay_lastmqttcommand = true;
    }
    //activate lastmqttcommand flag for fan structure wide
    if(all_fan_lastslavecommand_counter == 0){
        master.slave.all_fan_lastmqttcommand = false;
    }
    else{
        master.slave.all_fan_lastmqttcommand = true;
    }
    //activate lastmqttcommand flag for sensor structure wide
    if(all_sensor_lastmqttcommand_counter == 0){
        master.slave.all_sensor_lastmqttcommand = false;
    }
    else{
        master.slave.all_sensor_lastmqttcommand = true;
    }
    //activate changes flag for full relay structure
    if(all_relay_change_counter == 0){
        master.slave.all_relay_change = false;
    }
    else{
        master.slave.all_relay_change = true;
    }
    //activate changes flag for full fan structure
    if(all_fan_change_counter == 0){
        master.slave.all_fan_change = false;
    }
    else{
        master.slave.all_fan_change = true;
    }
    //activate changes flag for full sensor structure
    if(all_sensor_change_counter == 0){
        master.slave.all_sensor_change = false;
    }
    else{
        master.slave.all_sensor_change = true;
    }
    //activate slave wide change variable if any device structure changes ie relay, fan or sensor
    if((master.slave.all_relay_change == false) && (master.slave.all_fan_change == false) && (master.slave.all_sensor_change == false)){
        master.slave.change = false;
    }
    else{
      master.slave.change = true;
    }
    }
  for(int i=0;i<TIMER_NUMBER;i++){
    if(master.system.timer[i].change == true){
      master.system.timer[i].change = false;
    }
  }
  for(int i=0;i<SCENE_NUMBER;i++){
    if(master.system.scene[i].change == true){
      master.system.scene[i].change = false;
    }
  }
}

//Responsidble for handling timer in each devices
void HomeHub::timer_handler(){
  //activate changes flag of individual relay devices
  for(int i=0;i<TIMER_NUMBER;i++){                                                                                                                        //HomeHub_DEBUG_PRINT("[DEBUG] Inside Timer");
    if(master.system.timer[i].current_state == true){                                                                                                     //HomeHub_DEBUG_PRINT("[DEBUG] Inside Timer current state = true");          
      //On Timer Handler
      if(master.system.timer[i].ontimer.current_state == true){                                                                                           //HomeHub_DEBUG_PRINT("[DEBUG] Inside Timer Ontimer current state = true");
        if(master.system.timer[i].ontimer.hour == master.system.clock.hour){                                                                              //HomeHub_DEBUG_PRINT("[DEBUG] Inside Timer Ontimer Hour matched");
          if(master.system.timer[i].ontimer.minute == master.system.clock.minute){                                                                        //HomeHub_DEBUG_PRINT("[DEBUG] Inside Timer Ontimer Minute matched");
            if(master.system.timer[i].ontimer.active == false){                                                                                           //HomeHub_DEBUG_PRINT("[DEBUG] Inside Timer Ontimer active = true");
              notification_handler("system/timer/"+String(i)+"/ontimer/","1");
              for(int j=0;j<master.slave.RELAY_NUMBER;j++){                             //Running loop to cycle through relay list in timer[i]
                if(master.system.timer[i].relay[j] == true){                            //Select the relay to implement timer[i] on, from its structure
                  if(master.slave.relay[j].current_state != true){                      //Reconfirm to see if the specified relay is already on or not.
                    master.slave.relay[j].current_state = true;
                    //master.slave.relay[j].lastmqttcommand = true;
                  }
                }
              }                                                                         //Running loop to cycle through fan list in timer[i]
              for(int k=0;k<master.slave.FAN_NUMBER;k++){                               //Select the fan to implement timer[i] on, from its structure
                if(master.slave.fan[k].current_state != true){                          //Reconfirm to see if the specified fan is already on or not.
                  master.slave.fan[k].current_state = true;
                  //master.slave.fan[k].lastmqttcommand = true;
                }
              }
              master.system.timer[i].ontimer.active = true;
            }
          }
          else{
            master.system.timer[i].ontimer.active = false;
          }
        }
      }
      //Off Timer Handler
      if(master.system.timer[i].offtimer.current_state == true){                                                                                           //HomeHub_DEBUG_PRINT("[DEBUG] Inside Timer Ontimer current state = true");
        if(master.system.timer[i].offtimer.hour == master.system.clock.hour){                                                                              //HomeHub_DEBUG_PRINT("[DEBUG] Inside Timer Ontimer Hour matched");
          if(master.system.timer[i].offtimer.minute == master.system.clock.minute){                                                                        //HomeHub_DEBUG_PRINT("[DEBUG] Inside Timer Ontimer Minute matched");
            if(master.system.timer[i].offtimer.active == false){                                                                                           //HomeHub_DEBUG_PRINT("[DEBUG] Inside Timer Ontimer active = true");
              notification_handler("system/timer/"+String(i)+"/offtimer/","1");
              for(int j=0;j<master.slave.RELAY_NUMBER;j++){                             //Running loop to cycle through relay list in timer[i]
                if(master.system.timer[i].relay[j] == true){                            //Select the relay to implement timer[i] on, from its structure
                  if(master.slave.relay[j].current_state != false){                      //Reconfirm to see if the specified relay is already on or not.
                    master.slave.relay[j].current_state = false;  
                    //master.slave.relay[j].lastmqttcommand = true;                      
                  }
                }
              }                                                                         //Running loop to cycle through fan list in timer[i]
              for(int k=0;k<master.slave.FAN_NUMBER;k++){                               //Select the fan to implement timer[i] on, from its structure
                if(master.slave.fan[k].current_state != false){                          //Reconfirm to see if the specified fan is already on or not.
                  master.slave.fan[k].current_state = false;
                  //master.slave.fan[k].lastmqttcommand = true;
                }
              }
              master.system.timer[i].offtimer.active = true;
            }
          }
          else{
            master.system.timer[i].offtimer.active = false;
          }
        }
      }
    }
  }
}

//Responsidble for handling timer in each devices
void HomeHub::scene_handler(){
  //activate changes flag of individual relay devices
  for(int i=0;i<SCENE_NUMBER;i++){                                                                                                                   //HomeHub_DEBUG_PRINT("[DEBUG] Inside Timer");
    if(master.system.scene[i].current_state == true){                                                                                                //HomeHub_DEBUG_PRINT("[DEBUG] Inside Timer Ontimer Minute matched");                                                                                                  //HomeHub_DEBUG_PRINT("[DEBUG] Inside Timer Ontimer active = true");
        if(master.system.scene[i].trigger == true){
        notification_handler("system/scene/"+String(i)+"/","1");
        for(int j=0;j<master.slave.RELAY_NUMBER;j++){                                                                                                //Running loop to cycle through relay list in scene[i]
          if(master.slave.relay[j].current_state != master.system.scene[i].relay[j]){                                                                //Select the relay to implement scene[i] on, from its structure
            master.slave.relay[j].current_state = master.system.scene[i].relay[j];
          }
        }                                                                                                                                            //Running loop to cycle through fan list in timer[i]
        for(int k=0;k<master.slave.FAN_NUMBER;k++){                                                                                                  //Select the fan to implement timer[i] on, from its structure
          if(master.slave.fan[k].current_state != master.system.scene[i].fan[k]){                                                                    //Reconfirm to see if the specified fan is already on or not.
            master.slave.fan[k].current_state = master.system.scene[i].fan[k];
          }
        }
       master.system.scene[i].trigger = false; 
      }
    }
    else{
      master.system.scene[i].trigger = false; 
    }
  }
}

//Handles MQTT and Offline based notification system
void HomeHub::notification_handler(String topic, String payload){
  publish_mqtt((Device_Id_As_Publish_Topic+"notification/"+topic),payload,true);
}

//Initiate Structures
void HomeHub::initiate_structures(){
  
}

//Initiate Memory
void HomeHub::initiate_memory(){
  functionstate.initiate_memory = true;
    if(master.system.flag.rom_external == false){
      EEPROM.begin(512);
    }
    else{
      Wire.begin();
      for(int i=0;i<master.slave.RELAY_NUMBER;i++){
        //put button pins in button structures for use throught the program
        //eeRead(RELAY_MEMSPACE+(i*30),master.slave.relay[i],master.system.flag.rom_external);
        //relay_state[i] = master.slave.relay[i].current_state;
      }
      for(int i=0;i<TIMER_NUMBER;i++){
        //put button pins in button structures for use throught the program
        eeRead(TIMER_MEMSPACE+(i*30),master.system.timer[i],master.system.flag.rom_external);
      }
      for(int i=0;i<SCENE_NUMBER;i++){
        //put button pins in button structures for use throught the program
        eeRead(SCENE_MEMSPACE+(i*30),master.system.scene[i],master.system.flag.rom_external);
      }
    } 
}

//Initiate Devies
void HomeHub::initiate_devices(){
  for(int i=0;i<master.slave.RELAY_NUMBER;i++){
    //put button pins in button structures for use throught the program
    //pinMode(master.slave.RELAY_PIN[i],OUTPUT);
    //digitalWrite(master.slave.RELAY_PIN[i],(master.slave.relay[i].current_state)); // Change here according to relay hardware
  }
  for(int i=0;i<master.slave.BUTTON_NUMBER;i++){
    //put button pins in button structures for use throught the program
    //pinMode(master.slave.BUTTON_PIN[i],INPUT_PULLUP);
  }
}

//Custom EEPROM replacement functions
void HomeHub::rom_write(unsigned int eeaddress, byte data, bool external) {
    if(external == false){
        int rdata = data;
        EEPROM.write(eeaddress,data);
        delay(20);
        EEPROM.commit();
    }else{
        int rdata = data;
        Wire.beginTransmission(ROM_ADDRESS);
        Wire.write((int)(eeaddress >> 8));      // MSB
        Wire.write((int)(eeaddress & 0xFF));    // LSB
        Wire.write(rdata);
        Wire.endTransmission();
        delay(20);                              //Delay introduction solved the data save unreliability
    }
}

//Custom EEPROM replacement functions
byte HomeHub::rom_read(unsigned int eeaddress,bool external) {
    if(external == false){
        byte rdata = EEPROM.read(eeaddress);
        return rdata;
    }else{
        byte rdata = 0xFF;
        Wire.beginTransmission(ROM_ADDRESS);
        Wire.write((int)(eeaddress >> 8)); // MSB
        Wire.write((int)(eeaddress & 0xFF)); // LSB
        Wire.endTransmission();
        Wire.requestFrom(ROM_ADDRESS, 1);
        if (Wire.available()) rdata = Wire.read();
            return rdata;
    }
}

void HomeHub::rom_write_page(unsigned int eeaddresspage, byte* data, byte length ) {
    if(master.system.flag.rom_external == false){
        
    }else{
        Wire.beginTransmission(ROM_ADDRESS);
        Wire.write((int)(eeaddresspage >> 8)); // MSB
        Wire.write((int)(eeaddresspage & 0xFF)); // LSB
        byte c;
        for ( c = 0; c < length; c++)
            Wire.write(data[c]);
        Wire.endTransmission();
    }
}

bool HomeHub::retrieve_wifi_data(){
  _esid = "";
  _epass = "";
  for (int i = 0;i < 32;++i)
    {
      _esid += char(rom_read((_wifi_data_memspace+i),master.system.flag.rom_external)); //_esid += char(EEPROM.read(i));
    }
  HomeHub_DEBUG_PRINT("SSID:");HomeHub_DEBUG_PRINT(String(_esid));
  _epass = "";
  for(int i = 32;i < 96;++i)
    {
      _epass += char(rom_read((_wifi_data_memspace+i),master.system.flag.rom_external)); //_epass += char(EEPROM.read(i));
    }
  HomeHub_DEBUG_PRINT("PASS:");HomeHub_DEBUG_PRINT(String(_epass));
  if(_esid.length() > 1){
    return true;
  }
  else{
    return false;
  }
}

bool HomeHub::test_wifi() {
  HomeHub_DEBUG_PRINT("Waiting for Wifi to connect");
  for(int i=0;i<50;i++){
    delay(100);
    if(WiFi.status() == 3){
      break ;
    }
  }
  if (WiFi.status() == WL_CONNECTED){
    HomeHub_DEBUG_PRINT("Connected to Wifi Successfully");
    return true;
  }
  else{
    HomeHub_DEBUG_PRINT("Connect timed out");
    return false;
  }
}

String HomeHub::scan_networks(){
  //WiFi.mode(WIFI_STA);
  //WiFi.disconnect();
  String wifi_data = "";
  int network_available_number = WiFi.scanNetworks();
  //Serial.println("scan done");
  
  if(network_available_number == 0){
    //Serial.println("no networks found");
  }
  else
  {
    //Serial.print(network_available_number);
    //Serial.println(" networks found");
  }
  
  //Serial.println("");
  
  //wifi_data = "<ul>";
  wifi_data = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n<!DOCTYPE html><html lang=\"en\"><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1, user-scalable=no\" /><title>TNM Wifi Setup</title><script>function c(l,e) {console.log(l);document.getElementById('s').value = l.innerText || l.textContent;p = l.nextElementSibling.classList.contains(\"l\");document.getElementById('p').disabled = !p; if(p)document.getElementById('p').focus();return false;}</script><style>.c,body {text-align: center}div,input {padding: 5px;font-size: 1em}input {width: 95%}body {font-family: verdana}button {border: 0;border-radius: .3rem;background-color: #1fa3ec;color: #fff;line-height: 2.4rem;font-size: 1.2rem;width: 100%}a {color: #000;font-weight: 700;text-decoration: none}a:hover {color: #1fa3ec;text-decoration: underline}.q {height: 16px;margin: 0;padding: 0 5px;text-align: right;min-width: 38px}.q.q-0:after {background-position-x: 0}.q.q-1:after {background-position-x: -16px}.q.q-2:after {background-position-x: -32px}.q.q-3:after {background-position-x: -48px}.q.q-4:after {background-position-x: -64px}.q.l:before {background-position-x: -80px;padding-right: 5px}.ql .q {float: left}.qr .q {float: right}.qinv .q {-webkit-filter: invert(1);filter: invert(1)}.q:after,.q:before {content: '';width: 16px;height: 16px;display: inline-block;background-repeat: no-repeat;background-position: 16px 0;background-image: url('data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAGAAAAAQCAMAAADeZIrLAAAAJFBMVEX///8AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAADHJj5lAAAAC3RSTlMAIjN3iJmqu8zd7vF8pzcAAABsSURBVHja7Y1BCsAwCASNSVo3/v+/BUEiXnIoXkoX5jAQMxTHzK9cVSnvDxwD8bFx8PhZ9q8FmghXBhqA1faxk92PsxvRc2CCCFdhQCbRkLoAQ3q/wWUBqG35ZxtVzW4Ed6LngPyBU2CobdIDQ5oPWI5nCUwAAAAASUVORK5CYII=');}@media (-webkit-min-device-pixel-ratio: 2),(min-resolution: 192dpi) {.q:before,.q:after {background-image: url('data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAALwAAAAgCAMAAACfM+KhAAAALVBMVEX///8AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAADAOrOgAAAADnRSTlMAESIzRGZ3iJmqu8zd7gKjCLQAAACmSURBVHgB7dDBCoMwEEXRmKlVY3L//3NLhyzqIqSUggy8uxnhCR5Mo8xLt+14aZ7wwgsvvPA/ofv9+44334UXXngvb6XsFhO/VoC2RsSv9J7x8BnYLW+AjT56ud/uePMdb7IP8Bsc/e7h8Cfk912ghsNXWPpDC4hvN+D1560A1QPORyh84VKLjjdvfPFm++i9EWq0348XXnjhhT+4dIbCW+WjZim9AKk4UZMnnCEuAAAAAElFTkSuQmCC');background-size: 95px 16px;}}input:disabled {opacity: 0.5;}</style></head><body><!-- classes, left/right invert --><div class=\"qr\" style='text-align:left;display:inline-block;min-width:260px;'>";
  for (int i = 0; i < network_available_number; ++i)
    {
      // Print SSID and RSSI for each network found
      /*Serial.print(i + 1);Serial.print(": ");Serial.print(WiFi.SSID(i));
      Serial.print(" (");Serial.print(WiFi.RSSI(i));Serial.print(")");
      Serial.println((WiFi.encryptionType(i) == ENC_TYPE_NONE)?" ":"*");
      */
      // Print SSID and RSSI for each network found
      wifi_data += "<div><a href='#p' onclick='c(this)'>";
      wifi_data += String(WiFi.SSID(i)); 
      int strength = (int((WiFi.RSSI(i)))*(-1)); 
      if((0<=strength) && (strength<20)){
        if((WiFi.encryptionType(i) == ENC_TYPE_NONE)){
          wifi_data += "</a><div role='img' aria-label='88%' title='88%' class='q q-4'></div></div><br/>";
        }
        else{
          wifi_data += "</a><div role='img' aria-label='88%' title='88%' class='q q-4 l'></div></div><br/>";
        }
      }
      else if((20<=strength) && (strength<40)){
        if((WiFi.encryptionType(i) == ENC_TYPE_NONE)){
          wifi_data += "</a><div role='img' aria-label='88%' title='88%' class='q q-3'></div></div><br/>";
        }
        else{
          wifi_data += "</a><div role='img' aria-label='88%' title='88%' class='q q-3 l'></div></div><br/>";
        }
      }
      else if((40<=strength) && (strength<60)){
        if((WiFi.encryptionType(i) == ENC_TYPE_NONE)){
          wifi_data += "</a><div role='img' aria-label='88%' title='88%' class='q q-2'></div></div><br/>";
        }
        else{
          wifi_data += "</a><div role='img' aria-label='88%' title='88%' class='q q-2 l'></div></div><br/>";
        }
      }
      else if((60<=strength) && (strength<80)){
        if((WiFi.encryptionType(i) == ENC_TYPE_NONE)){
          wifi_data += "</a><div role='img' aria-label='88%' title='88%' class='q q-1'></div></div><br/>";
        }
        else{
          wifi_data += "</a><div role='img' aria-label='88%' title='88%' class='q q-1 l'></div></div><br/>";
        }
      }
      else if((80<=strength) && (strength<=100)){
        if((WiFi.encryptionType(i) == ENC_TYPE_NONE)){
          wifi_data += "</a><div role='img' aria-label='88%' title='88%' class='q q-0'></div></div><br/>";
        }
        else{
          wifi_data += "</a><div role='img' aria-label='88%' title='88%' class='q q-0 l'></div></div><br/>";
        }
      }
      
      /*
      wifi_data += "<li>";
      wifi_data +=i + 1;
      wifi_data += ": ";
      wifi_data += WiFi.SSID(i);
      wifi_data += " (";
      wifi_data += WiFi.RSSI(i);
      wifi_data += ")";
      wifi_data += (WiFi.encryptionType(i) == ENC_TYPE_NONE)?" ":"*";
      wifi_data += "</li>"; */
      //Comparator
      int test_counter = 0;
      String current_wifi = WiFi.SSID(i);
      for(int j=0;j<current_wifi.length();j++){
        if(current_wifi.charAt(j) == _esid.charAt(j)){
          test_counter++;
        }
      }
      if(test_counter == current_wifi.length()){
        HomeHub_DEBUG_PRINT("Saved Wifi present");
        master.system.flag.saved_wifi_present = true;
      }
    }
    wifi_data += "<form id=\"wmform\" method='get' action='a'><input id='s' name='ssid' length=32 placeholder='SSID'><br/><input id='p' name='pass' length=64 type='password' placeholder='password'><br/><br/><br/><button type='submit'>save</button></form><br/><div class=\"c\"><a href=\"/wifisetup\">Scan</a></div></div></body></html>";
    //wifi_data += "</ul>";
    return wifi_data;
}

void HomeHub::start_server(){
  server = new WiFiServer(80);
  mdns = new MDNSResponder();
    
  mdns->begin("esp8266", WiFi.softAPIP());
  HomeHub_DEBUG_PRINT("mDNS responder started");
    
  server->begin(); // Web server start
  HomeHub_DEBUG_PRINT("Web Server started");
}

bool HomeHub::stop_server(){
  mdns->close();
  HomeHub_DEBUG_PRINT("mDNS responder ended");
  // Start the server
  server->stop();
  server->close();
  HomeHub_DEBUG_PRINT("Web Server ended");
  return true;
}

bool HomeHub::saved_wifi_connect(){ HomeHub_DEBUG_PRINT("Saved Wifi Connect");
  if(retrieve_wifi_data()){
      WiFi.begin(_esid.c_str(), _epass.c_str());
      bool connection_result = test_wifi();
      if(connection_result){
          if(master.system.flag.boot_check_update == true){
              update_device();
          }
      }
      return (connection_result);
  }
}

bool HomeHub::manual_wifi_connect(const char* wifi, const char* pass){
    _esid = wifi;
    _epass = pass;
    WiFi.begin(wifi, pass);
    bool connection_result = test_wifi();
    if(connection_result){
        if(master.system.flag.boot_check_update == true){
            update_device();
        }
    }
    return (connection_result);
}

void HomeHub::save_wifi_data(String ssid, String password){
    HomeHub_DEBUG_PRINT("Saved Wifi data");
    for (int i = 0; i < ssid.length(); ++i)
      {
        rom_write(_wifi_data_memspace+i, ssid[i],master.system.flag.rom_external); //EEPROM.write(i, qsid[i]);
        HomeHub_DEBUG_PRINT("Wrote: ");
        //HomeHub_DEBUG_PORT.println(ssid[i]);
        _esid = ssid;
      }
    HomeHub_DEBUG_PRINT("writing eeprom pass:");
    for (int i = 0; i < password.length(); ++i)
      {
        rom_write(_wifi_data_memspace+32+i, password[i],master.system.flag.rom_external); //EEPROM.write(32+i, qpass[i]);
        HomeHub_DEBUG_PRINT("Wrote: ");
        //HomeHub_DEBUG_PORT.println(password[i]);
        _epass = password;
      }
}

void HomeHub::saved_wifi_dump(){
  HomeHub_DEBUG_PRINT("clearing eeprom");
  for (int i = 0; i < 96; ++i) {
      rom_write(_wifi_data_memspace+i,0,master.system.flag.rom_external);// EEPROM.write(i, 0);
  }
  //EEPROM.commit();
}

void HomeHub::mqtt_handshake_handler(){
  if(master.system.flag.slave_handshake == true){
    publish_mqtt((Device_Id_As_Publish_Topic+"information/slavename/"),String(master.slave.NAME),true);
    publish_mqtt((Device_Id_As_Publish_Topic+"information/relay/"),String(master.slave.RELAY_NUMBER),true);
    publish_mqtt((Device_Id_As_Publish_Topic+"information/fan/"),String(master.slave.FAN_NUMBER),true);
    publish_mqtt((Device_Id_As_Publish_Topic+"information/sensor/"),String(master.slave.SENSOR_NUMBER),true);
    //Relay data
    for(int i=0;i<master.slave.RELAY_NUMBER;i++){
      publish_mqtt((Device_Id_As_Publish_Topic+"relay/"+String(i+1)+"/value/"),String(master.slave.relay[i].current_state),true);
    }
    //Fan data
    for(int i=0;i<master.slave.FAN_NUMBER;i++){
      publish_mqtt((Device_Id_As_Publish_Topic+"relay/"+String(i+1)+"/state/"),String(master.slave.relay[i].current_state),true);
      publish_mqtt((Device_Id_As_Publish_Topic+"fan/"+String(i+1)+"/value/"),String(master.slave.fan[i].current_value),true);
    }
    //Sensor data
    for(int i=0;i<master.slave.SENSOR_NUMBER;i++){
      publish_mqtt((Device_Id_As_Publish_Topic+"sensor/"+String(i+1)+"/type/"),String(master.slave.sensor[i].type),true);
      publish_mqtt((Device_Id_As_Publish_Topic+"sensor/"+String(i+1)+"/value/"),String(master.slave.sensor[i].current_value),true);
    }
    for(int i=0;i<TIMER_NUMBER;i++){
    publish_mqtt((Device_Id_As_Publish_Topic+"system/timer/"+String(i+1)+"/"),String(master.system.timer[i].current_state),true);
    if(master.system.timer[i].current_state == true){
      //On Timer
      publish_mqtt((Device_Id_As_Publish_Topic+"system/timer/"+String(i+1)+"/ontimer/"),String(master.system.timer[i].ontimer.current_state),true);
      publish_mqtt((Device_Id_As_Publish_Topic+"system/timer/"+String(i+1)+"/ontimer/hour/"),String(master.system.timer[i].ontimer.hour),true);
      publish_mqtt((Device_Id_As_Publish_Topic+"system/timer/"+String(i+1)+"/ontimer/minute/"),String(master.system.timer[i].ontimer.minute),true);
      //Off Timer
      publish_mqtt((Device_Id_As_Publish_Topic+"system/timer/"+String(i+1)+"/offtimer/"),String(master.system.timer[i].offtimer.current_state),true);
      publish_mqtt((Device_Id_As_Publish_Topic+"system/timer/"+String(i+1)+"/offtimer/hour/"),String(master.system.timer[i].offtimer.hour),true);
      publish_mqtt((Device_Id_As_Publish_Topic+"system/timer/"+String(i+1)+"/offtimer/minute/"),String(master.system.timer[i].offtimer.minute),true);
      //Timer Selected Devices
      for(int k=0;k<master.slave.RELAY_NUMBER;k++){
        publish_mqtt((Device_Id_As_Publish_Topic+"system/timer/"+String(i+1)+"/relay/"+String(k+1)+"/"),String(master.system.timer[i].relay[k]),true);
      }
      for(int l=0;l<master.slave.FAN_NUMBER;l++){
        publish_mqtt((Device_Id_As_Publish_Topic+"system/timer/"+String(i+1)+"/fan/"+String(l+1)+"/"),String(master.system.timer[i].fan[l]),true);
      }
    }
  }
    for(int i=0;i<SCENE_NUMBER;i++){
    publish_mqtt((Device_Id_As_Publish_Topic+"system/scene/"+String(i+1)+"/"),String(master.system.scene[i].current_state),true);
    if(master.system.scene[i].current_state == true){
      //Timer Selected Devices
      for(int k=0;k<master.slave.RELAY_NUMBER;k++){
        publish_mqtt((Device_Id_As_Publish_Topic+"system/scene/"+String(i+1)+"/relay/"+String(k+1)+"/"),String(master.system.scene[i].relay[k]),true);
      }
      for(int l=0;l<master.slave.FAN_NUMBER;l++){
        publish_mqtt((Device_Id_As_Publish_Topic+"system/scene/"+String(i+1)+"/fan/"+String(l+1)+"/"),String(master.system.scene[i].fan[l]),true);
      }
    }
  }
  }
  else{
    publish_mqtt((Device_Id_As_Publish_Topic+"information/name/"),master.NAME,true);
    publish_mqtt((Device_Id_As_Publish_Topic+"information/version/"),master.VERSION,true);
 }
}

bool HomeHub::initiate_mqtt(){
    functionstate.initiate_mqtt = true;
    //Mqtt String to char* Conversions
    Device_Id_As_Publish_Topic.toCharArray(Device_Id_In_Char_As_Publish_Topic, 22);
    Device_Id_As_Subscription_Topic.toCharArray(Device_Id_In_Char_As_Subscription_Topic, 20);
    //Mqtt connection setup
    mqtt_clientname = Device_Id_In_Char_As_Publish_Topic;
    char topicaschar[25];
    String topic = Device_Id_As_Publish_Topic+"offline/";
    topic.toCharArray(topicaschar, 25);
    mqttclient.setServer(mqtt_server, mqtt_port);
    mqttclient.setCallback([this] (char* topic, byte* payload, unsigned int length) { this->mqttcallback(topic, payload, length); });
    mqttclient.connect(mqtt_clientname,mqtt_serverid, mqtt_serverpass,topicaschar,1,1,"1");
    if (mqttclient.connected()) {
        mqttclient.subscribe(Device_Id_In_Char_As_Subscription_Topic);
        publish_mqtt(topic,"0",true);
        mqtt_handshake_handler();
        return 's';
    } else {
        HomeHub_DEBUG_PRINT("Failed to connect to mqtt server, rc=");
        HomeHub_DEBUG_PORT.print(mqttclient.state());
        return 'f';
    }
}

void HomeHub::end_mqtt(){
  mqttclient.disconnect();
}

HomeHub& HomeHub::setCallback(MQTT_CALLBACK_SIGN){
    this->mqttCallback = mqttCallback;
    return *this;
}

void HomeHub::mqttcallback(char* topic, byte* payload, unsigned int length) {
  String pay = "";
  String Topic = topic;
  for (int i = 0; i < length; i++) {
    pay+=(char)payload[i];
  }
  LastCommand = pay;
  mqtt_input_handler(Topic, pay);
}
    
bool HomeHub::mqtt_input_handler(String topic,String payload){
    HomeHub_DEBUG_PRINT("Topic : "+topic);
    HomeHub_DEBUG_PRINT("Payload : "+payload);
    //Reset Last Slave Command flags to false
    for(int i=0;i<master.slave.RELAY_NUMBER;i++){
        master.slave.relay[i].lastmqttcommand = false;
    }
    for(int i=0;i<master.slave.FAN_NUMBER;i++){
        master.slave.fan[i].lastmqttcommand = false;
    }
    for(int i=0;i<master.slave.SENSOR_NUMBER;i++){
        master.slave.sensor[i].lastmqttcommand = false;
    }
    if(topic.charAt(topic.length()) != '/'){
        topic = topic + "/";
    }
    //Switching the lastmqttcommands to their default value
    //remove the chipid
    if(topic.length() > 0){
        String chipId = topic.substring(0,topic.indexOf('/',0));
        topic.remove(0,topic.indexOf('/',0)+1);
        if(topic.length() > 0){
            String sub1 = topic.substring(0,topic.indexOf('/',0));
            topic.remove(0,topic.indexOf('/',0)+1);
            if(sub1 == "relay"){
                if(topic.length() > 0){
                    String sub1 = topic.substring(0,topic.indexOf('/',0));
                    topic.remove(0,topic.indexOf('/',0)+1);
                    if(sub1 == "all"){
                                    if(topic.length() > 0){
                                        String sub1 = topic.substring(0,topic.indexOf('/',0));
                                        topic.remove(0,topic.indexOf('/',0)+1);
                                      if(sub1 == "value"){
                                            int value = payload.toInt();
                                            if((value == 0) || (value ==1)){
                                                for(int i=0;i<master.slave.RELAY_NUMBER;i++){
                                                    master.slave.relay[i].current_state = bool(value);
                                                    master.slave.relay[i].lastmqttcommand = true;
                                                }
                                            }
                                        }
                                    }
                                }
                    else{
                        for(int i=0;i<master.slave.RELAY_NUMBER;i++){
                            if((i+1) == sub1.toInt()){
                                if(topic.length() > 0){
                                    String sub1 = topic.substring(0,topic.indexOf('/',0));
                                    topic.remove(0,topic.indexOf('/',0)+1);
                                    if(sub1 == "value"){
                                        int value = payload.toInt();
                                        if((value == 0) || (value == 1)){
                                            master.slave.relay[i].current_state = bool(value);
                                            master.slave.relay[i].lastmqttcommand = true;
                                        }
                                    }
                                    }
                                  }
                                }
                            }
                        }
                    }                
            if(sub1 == "fan"){
                if(topic.length() > 0){
                    String sub1 = topic.substring(0,topic.indexOf('/',0));
                    topic.remove(0,topic.indexOf('/',0)+1);
                    if(sub1 == "all"){
                                    if(topic.length() > 0){
                                        String sub1 = topic.substring(0,topic.indexOf('/',0));
                                        topic.remove(0,topic.indexOf('/',0)+1);
                                        if(sub1 == "value"){
                                            int value = payload.toInt();
                                            if(value == 0){
                                                for(int i=0;i<master.slave.FAN_NUMBER;i++){
                                                    master.slave.fan[i].current_state = false;
                                                    master.slave.fan[i].lastmqttcommand = true;
                                                }
                                            }
                                            else if((value > 0) && (value<11)){
                                                for(int i=0;i<master.slave.FAN_NUMBER;i++){
                                                    master.slave.fan[i].current_state = true;
                                                    master.slave.fan[i].current_value = value;
                                                    master.slave.fan[i].lastmqttcommand = true;
                                                }
                                            }
                                        }
                                    }
                                }
                    else{
                        for(int i=0;i<master.slave.FAN_NUMBER;i++){
                            if((i+1) == sub1.toInt()){
                                if(topic.length() > 0){
                                    String sub1 = topic.substring(0,topic.indexOf('/',0));
                                    topic.remove(0,topic.indexOf('/',0)+1);
                                    int value = payload.toInt();
                                    if(value == 0){
                                            master.slave.fan[i].current_state = false;
                                            master.slave.fan[i].lastmqttcommand = true;
                                    }
                                    else if((value > 0) && (value<11)){
                                            master.slave.fan[i].current_state = true;
                                            master.slave.fan[i].current_value = value;
                                            master.slave.fan[i].lastmqttcommand = true;
                                    }
                                }
                            }
                        }
                    }
                }
            }
            if(sub1 == "system"){
                if(topic.length() > 0){
                    String sub1 = topic.substring(0,topic.indexOf('/',0));
                    topic.remove(0,topic.indexOf('/',0)+1);
                    if(sub1 == "flag"){
                        if(topic.length() > 0){
                        String sub1 = topic.substring(0,topic.indexOf('/',0));
                        topic.remove(0,topic.indexOf('/',0)+1);
                            if(sub1 == "handshake"){
                                publish_mqtt(Device_Id_As_Publish_Topic+"system/flag/handshake/",String(!(master.system.flag.slave_handshake)),false);
                                HomeHub_DEBUG_PRINT("Handshake Flag Sent.");
                            }
                        }
                    }
                    else if(sub1 == "update"){
                        HomeHub_DEBUG_PRINT("Checking Update.");
                        if(payload == ""){
                          update_device();
                        }
                        else{
                          String hostname = String(payload);
                          update_device(hostname);
                        }
                    }
                    else if(sub1 == "wifisetup"){
                        HomeHub_DEBUG_PRINT("Starting Wifi Setup.");
                    }
                    else if(sub1 == "clock"){
                      if(topic.length() > 0 && topic!=""){
                        String sub1 = topic.substring(0,topic.indexOf('/',0));
                        topic.remove(0,topic.indexOf('/',0)+1);
                        if(sub1 == "date"){
                          if(payload == ""){
                            publish_mqtt(Device_Id_As_Publish_Topic+"system/clock/date/",String(master.system.clock.date),false);
                          }
                          else{
                            master.system.flag.set_time = true;
                            master.system.clock.date = payload.toInt();
                          }
                        }
                        else if(sub1 == "month"){
                          if(payload == ""){
                            publish_mqtt(Device_Id_As_Publish_Topic+"system/clock/month/",String(master.system.clock.month),false);
                          }
                          else{
                            master.system.flag.set_time = true;
                            master.system.clock.month = payload.toInt();
                          }
                        }
                        else if(sub1 == "year"){
                          if(payload == ""){
                            publish_mqtt(Device_Id_As_Publish_Topic+"system/clock/year/",String(master.system.clock.year),false);
                          }
                          else{
                            master.system.flag.set_time = true;
                            master.system.clock.year = payload.toInt();
                          }
                        }
                        else if(sub1 == "week"){
                          if(payload == ""){
                            publish_mqtt(Device_Id_As_Publish_Topic+"system/clock/week/",String(master.system.clock.week),false);
                          }
                          else{
                            master.system.flag.set_time = true;
                            master.system.clock.week = payload.toInt();
                          }
                          
                        }
                        else if(sub1 == "hour"){
                          if(payload == ""){
                            publish_mqtt(Device_Id_As_Publish_Topic+"system/clock/hour/",String(master.system.clock.hour),false);
                          }
                          else{
                            master.system.flag.set_time = true;
                            master.system.clock.hour = payload.toInt();
                          }
                        }
                        else if(sub1 == "minute"){
                          if(payload == ""){
                            publish_mqtt(Device_Id_As_Publish_Topic+"system/clock/minute/",String(master.system.clock.minute),false);
                          }
                          else{
                            master.system.flag.set_time = true;
                            master.system.clock.minute = payload.toInt();
                          }
                        }
                        else if(sub1 == "second"){
                          if(payload == ""){
                            publish_mqtt(Device_Id_As_Publish_Topic+"system/clock/second/",String(master.system.clock.second),false);
                          }
                          else{
                            master.system.flag.set_time = true;
                            master.system.clock.second = payload.toInt();
                          }
                        }
                        else if(sub1 == "errorcode"){
                          publish_mqtt(Device_Id_As_Publish_Topic+"system/clock/errorcode/",String(master.system.clock.error_code),false);
                        }
                        else if(sub1 == "error"){
                          publish_mqtt(Device_Id_As_Publish_Topic+"system/clock/error/",String(master.system.clock.error),false);
                        }
                        else if(sub1 == "temp"){
                          publish_mqtt(Device_Id_As_Publish_Topic+"system/clock/temp/",String(master.system.clock.temp),false);
                        }
                        else{
                          
                        }
                      }
                      else{
                        publish_mqtt(Device_Id_As_Publish_Topic+"system/clock/date/",String(master.system.clock.date),false);
                        publish_mqtt(Device_Id_As_Publish_Topic+"system/clock/month/",String(master.system.clock.month),false);
                        publish_mqtt(Device_Id_As_Publish_Topic+"system/clock/year/",String(master.system.clock.year),false);
                        publish_mqtt(Device_Id_As_Publish_Topic+"system/clock/week/",String(master.system.clock.week),false);
                        publish_mqtt(Device_Id_As_Publish_Topic+"system/clock/hour/",String(master.system.clock.hour),false);
                        publish_mqtt(Device_Id_As_Publish_Topic+"system/clock/minute/",String(master.system.clock.minute),false);
                        publish_mqtt(Device_Id_As_Publish_Topic+"system/clock/second/",String(master.system.clock.second),false);
                        publish_mqtt(Device_Id_As_Publish_Topic+"system/clock/errorcode/",String(master.system.clock.error_code),false);
                        publish_mqtt(Device_Id_As_Publish_Topic+"system/clock/error/",String(master.system.clock.error),false);
                        publish_mqtt(Device_Id_As_Publish_Topic+"system/clock/temp/",String(master.system.clock.temp),false);
                      }
                    }
                    else if(sub1 == "timer"){                                                                          //Select the Timer in system
                        if(topic.length() > 0){
                          String sub1 = topic.substring(0,topic.indexOf('/',0));
                          topic.remove(0,topic.indexOf('/',0)+1);
                          for(int i=0;i<TIMER_NUMBER;i++){                                                            //Select the exact Timer number in the timers
                            if((i+1) == sub1.toInt()){        
                              master.system.timer[i].change = true;                                                   //Mark the change in timer for mqtt notification
                              int value = payload.toInt();
                                if((topic.length() > 0) && (topic != "/")){                                           //if the incomming data does not end at timer[i]/number also the remaining item should be non zero but not just equal to "/"
                                  String sub1 = topic.substring(0,topic.indexOf('/',0));
                                  topic.remove(0,topic.indexOf('/',0)+1);
                                  //Ontimer stack
                                  if(sub1 == "ontimer"){                                  
                                        if((topic.length() > 0) && (topic != "/")){                                   //the incomming data still has a value and isnt just "/"
                                            String sub1 = topic.substring(0,topic.indexOf('/',0));
                                            topic.remove(0,topic.indexOf('/',0)+1);
                                            if(sub1 == "minute"){                         
                                                int value = payload.toInt();
                                                if(payload == ""){
                                                    publish_mqtt((Device_Id_As_Publish_Topic+"system/timer/"+String(i+1)+"/ontimer/minute/"),String(master.system.timer[i].ontimer.minute),true);
                                                }
                                                else{
                                                    if((value <=60) && (value >= 0)){
                                                        master.system.timer[i].ontimer.minute = value; 
                                                        eeWrite((TIMER_MEMSPACE+(30*i)),master.system.timer[i],master.system.flag.rom_external);
                                                        master.system.timer[i].change = true;
                                                    }
                                                }
                                            }
                                            if(sub1 == "hour"){                          
                                                int value = payload.toInt();
                                                if(payload == ""){
                                                    publish_mqtt((Device_Id_As_Publish_Topic+"system/timer/"+String(i+1)+"/ontimer/hour/"),String(master.system.timer[i].ontimer.hour),true);
                                                }
                                                else{
                                                    if((value <=  24) || (value >= 0)){
                                                        master.system.timer[i].ontimer.hour = value; 
                                                        eeWrite((TIMER_MEMSPACE+(30*i)),master.system.timer[i],master.system.flag.rom_external);
                                                        master.system.timer[i].change = true;
                                                    }
                                                }
                                            }
                                            if(sub1 == "week"){                           
                                                int value = payload.toInt();
                                                if(payload == ""){
                                                    publish_mqtt((Device_Id_As_Publish_Topic+"system/timer/"+String(i+1)+"/ontimer/week/"),String(master.system.timer[i].ontimer.week),true);
                                                }
                                                else{
                                                    if((value <=  8) || (value >= 0)){
                                                        master.system.timer[i].ontimer.week = value;  
                                                        eeWrite((TIMER_MEMSPACE+(30*i)),master.system.timer[i],master.system.flag.rom_external);
                                                        master.system.timer[i].change = true;
                                                    }
                                                }
                                            }
                                        }
                                        else if(payload == ""){                                                       //check is the payload is empty, is so resent it to the cloud 
                                          publish_mqtt((Device_Id_As_Publish_Topic+"system/timer/"+String(i+1)+"/ontimer/"),String(master.system.timer[i].ontimer.current_state),true);
                                        }
                                        else{                                                                         //incomming payload isnt empty, hence check it for boolean and assign to variable 
                                          if((value == 0) || (value == 1)){                                           //validating is the data revieved is boolean only
                                            master.system.timer[i].ontimer.current_state = bool(value); 
                                            eeWrite((TIMER_MEMSPACE+(30*i)),master.system.timer[i],master.system.flag.rom_external);
                                            master.system.timer[i].change = true;
                                          } 
                                        }
                                  }
                                  //Offtimer stack
                                  if(sub1 == "offtimer"){                                  
                                        if((topic.length() > 0) && (topic != "/")){                                   //the incomming data still has a value and isnt just "/"
                                            String sub1 = topic.substring(0,topic.indexOf('/',0));
                                            topic.remove(0,topic.indexOf('/',0)+1);
                                            if(sub1 == "minute"){                         
                                                int value = payload.toInt();
                                                if(payload == ""){
                                                    publish_mqtt((Device_Id_As_Publish_Topic+"system/timer/"+String(i+1)+"/offtimer/minute/"),String(master.system.timer[i].offtimer.minute),true);
                                                }
                                                else{
                                                    if((value <=60) && (value >= 0)){
                                                        master.system.timer[i].offtimer.minute = value; 
                                                        eeWrite((TIMER_MEMSPACE+(30*i)),master.system.timer[i],master.system.flag.rom_external);
                                                        master.system.timer[i].change = true;
                                                    }
                                                }
                                            }
                                            if(sub1 == "hour"){                          
                                                int value = payload.toInt();
                                                if(payload == ""){
                                                    publish_mqtt((Device_Id_As_Publish_Topic+"system/timer/"+String(i+1)+"/offtimer/hour/"),String(master.system.timer[i].offtimer.hour),true);
                                                }
                                                else{
                                                    if((value <=  24) || (value >= 0)){
                                                        master.system.timer[i].offtimer.hour = value; 
                                                        eeWrite((TIMER_MEMSPACE+(30*i)),master.system.timer[i],master.system.flag.rom_external);
                                                        master.system.timer[i].change = true;
                                                    }
                                                }
                                            }
                                            if(sub1 == "week"){                           
                                                int value = payload.toInt();
                                                if(payload == ""){
                                                    publish_mqtt((Device_Id_As_Publish_Topic+"system/timer/"+String(i+1)+"/offtimer/week/"),String(master.system.timer[i].offtimer.week),true);
                                                }
                                                else{
                                                    if((value <=  8) || (value >= 0)){
                                                        master.system.timer[i].offtimer.week = value;  
                                                        eeWrite((TIMER_MEMSPACE+(30*i)),master.system.timer[i],master.system.flag.rom_external);
                                                        master.system.timer[i].change = true;
                                                    }
                                                }
                                            }
                                        }
                                        else if(payload == ""){                                                       //check is the payload is empty, is so resent it to the cloud 
                                          publish_mqtt((Device_Id_As_Publish_Topic+"system/timer/"+String(i+1)+"/offtimer/"),String(master.system.timer[i].offtimer.current_state),true);
                                        }
                                        else{                                                                         //incomming payload isnt empty, hence check it for boolean and assign to variable 
                                          if((value == 0) || (value == 1)){                                           //validating is the data revieved is boolean only
                                            master.system.timer[i].offtimer.current_state = bool(value); 
                                            eeWrite((TIMER_MEMSPACE+(30*i)),master.system.timer[i],master.system.flag.rom_external);
                                            master.system.timer[i].change = true;
                                          } 
                                        }
                                  }
                                  //relay configuration stack for each timer
                                  if(sub1 == "relay"){                                       
                                      if(topic.length() > 0){
                                        String sub1 = topic.substring(0,topic.indexOf('/',0));
                                        topic.remove(0,topic.indexOf('/',0)+1);
                                        if(payload == ""){                       
                                          publish_mqtt((Device_Id_As_Publish_Topic+"system/timer/"+String(i+1)+"/relay/"+String(sub1.toInt())),String(master.system.timer[i].relay[(sub1.toInt()-1)]),true);
                                        }
                                        else if((value == 0) || (value == 1)){   
                                          master.system.timer[i].relay[(sub1.toInt())-1] = bool(value);   
                                          eeWrite((TIMER_MEMSPACE+(30*i)),master.system.timer[i],master.system.flag.rom_external);
                                          master.system.timer[i].change = true;
                                        }
                                      }
                                    }
                                }
                                //response stack for ontimer state
                                else if(payload == ""){               //if the incomming data ends at timer[i]/number/ but has no payload
                                  publish_mqtt((Device_Id_As_Publish_Topic+"system/timer/"+String(i+1)+"/"),String(master.system.timer[i].current_state),true);
                                }
                                //input stack for ontimer state
                                else{                                 //if the incomming data ends at timer[i]/number/ but has true or false payload
                                  if((value == 0) || (value == 1)){   //validating is the data revieved is boolean only
                                    master.system.timer[i].current_state = bool(value);   
                                    eeWrite((TIMER_MEMSPACE+(30*i)),master.system.timer[i],master.system.flag.rom_external);
                                    master.system.timer[i].change = true;
                                  }
                                }
                              }
                            }
                        }
                    }
                    else if(sub1 == "scene"){                                                                          //Select the Timer in system
                      if(topic.length() > 0){
                        String sub1 = topic.substring(0,topic.indexOf('/',0));
                        topic.remove(0,topic.indexOf('/',0)+1);
                        for(int i=0;i<SCENE_NUMBER;i++){                                                            //Select the exact scene number in the timers
                          if((i+1) == sub1.toInt()){                                                         
                            master.system.scene[i].change = true;                                                   //Mark the change in timer for mqtt notification
                            int value = payload.toInt();
                            if((topic.length() > 0) && (topic != "/")){                                           //if the incomming data does not end at scene[i]/number also the remaining item should be non zero but not just equal to "/"
                              String sub1 = topic.substring(0,topic.indexOf('/',0));
                              topic.remove(0,topic.indexOf('/',0)+1);
                              //relay configuration stack for each timer
                              if(sub1 == "relay"){                                       
                                if(topic.length() > 0){
                                  String sub1 = topic.substring(0,topic.indexOf('/',0));
                                  topic.remove(0,topic.indexOf('/',0)+1);
                                  if(payload == ""){                       
                                    publish_mqtt((Device_Id_As_Publish_Topic+"system/scene/"+String(i+1)+"/relay/"+String(sub1.toInt())),String(master.system.scene[i].relay[(sub1.toInt()-1)]),true);
                                  }
                                  else if((value == 0) || (value == 1)){   
                                    master.system.scene[i].relay[(sub1.toInt())-1] = bool(value);   
                                    eeWrite((SCENE_MEMSPACE+(30*i)),master.system.scene[i],master.system.flag.rom_external);
                                    master.system.scene[i].change = true;
                                  }
                                }
                              }
                              else if(sub1 == "trigger"){ 
                                master.system.scene[i].trigger = true;
                              }
                            }
                            //response stack for ontimer state
                            else if(payload == ""){               //if the incomming data ends at scene[i]/number/ but has no payload
                              publish_mqtt((Device_Id_As_Publish_Topic+"system/scene/"+String(i+1)+"/"),String(master.system.scene[i].current_state),true);
                            }
                            //input stack for scene state
                            else{                                 //if the incomming data ends at scene[i]/number/ but has true or false payload
                              if((value == 0) || (value == 1)){   //validating is the data revieved is boolean only
                                master.system.scene[i].current_state = bool(value);   
                                eeWrite((SCENE_MEMSPACE+(30*i)),master.system.scene[i],master.system.flag.rom_external);
                                master.system.scene[i].change = true;
                              }
                            }
                          }
                        }
                      }
                    }
                    else{
                    }
                }
            }
            if(sub1 == "information"){
              mqtt_handshake_handler();
              HomeHub_DEBUG_PRINT("Handshake Flag Sent.");
            }
        }
    }
}

void HomeHub::publish_mqtt(String message, bool retain){
    mqttclient.publish(Device_Id_In_Char_As_Publish_Topic, message.c_str(), retain);
}

void HomeHub::publish_mqtt(String topic, String message, bool retain){
    mqttclient.publish(topic.c_str(), message.c_str(), retain);
}

void HomeHub::mqtt_output_handler(){
    if(master.slave.change == true){
        String topic = "";
        String payload = "";
        if(master.slave.all_relay_change == true){
            for(int i=0;i<master.slave.RELAY_NUMBER;i++){
                if(master.slave.relay[i].change == true){
                    topic = Device_Id_As_Publish_Topic + "relay/" + String(i+1) + "/value/";
                    payload = String(master.slave.relay[i].current_state);
                    publish_mqtt(topic,payload,true);
                }
            }
        }
        if(master.slave.all_fan_change == true){
          for(int i=0;i<master.slave.FAN_NUMBER;i++){
              if(master.slave.fan[i].change == true){
                  topic = Device_Id_As_Publish_Topic + "fan/" + String(i+1) + "/value/";
                  if(master.slave.fan[i].current_state == false){ // Fan is off by state sending a 0 value instead saved fan value
                      payload = "0";
                  }
                  else{ // value of fan state is true then send the current value
                      payload = String(master.slave.fan[i].current_value);
                  }
                  publish_mqtt(topic,payload,true);
              }
          }
      }
        if(master.slave.all_sensor_change == true){
          for(int i=0;i<master.slave.SENSOR_NUMBER;i++){
              if(master.slave.sensor[i].change == true){
                  topic = Device_Id_As_Publish_Topic + "sensor/" + String(i+1) + "/" +"value/";
                  payload = String(master.slave.sensor[i].current_value);
                  publish_mqtt(topic,payload,true);
              }
          }
      }
    }
    for(int i=0;i<TIMER_NUMBER;i++){
      if(master.system.timer[i].change == true){
        publish_mqtt((Device_Id_As_Publish_Topic+"system/timer/"+String(i+1)+"/"),String(master.system.timer[i].current_state),true);
        if(master.system.timer[i].current_state == true){
          //On Timer
          publish_mqtt((Device_Id_As_Publish_Topic+"system/timer/"+String(i+1)+"/ontimer/"),String(master.system.timer[i].ontimer.current_state),true);
          publish_mqtt((Device_Id_As_Publish_Topic+"system/timer/"+String(i+1)+"/ontimer/hour/"),String(master.system.timer[i].ontimer.hour),true);
          publish_mqtt((Device_Id_As_Publish_Topic+"system/timer/"+String(i+1)+"/ontimer/minute/"),String(master.system.timer[i].ontimer.minute),true);
          //Off Timer
          publish_mqtt((Device_Id_As_Publish_Topic+"system/timer/"+String(i+1)+"/offtimer/"),String(master.system.timer[i].offtimer.current_state),true);
          publish_mqtt((Device_Id_As_Publish_Topic+"system/timer/"+String(i+1)+"/offtimer/hour/"),String(master.system.timer[i].offtimer.hour),true);
          publish_mqtt((Device_Id_As_Publish_Topic+"system/timer/"+String(i+1)+"/offtimer/minute/"),String(master.system.timer[i].offtimer.minute),true);
          //Timer Selected Devices
          for(int k=0;k<master.slave.RELAY_NUMBER;k++){
            publish_mqtt((Device_Id_As_Publish_Topic+"system/timer/"+String(i+1)+"/relay/"+String(k+1)+"/"),String(master.system.timer[i].relay[k]),true);
          }
          for(int l=0;l<master.slave.FAN_NUMBER;l++){
            publish_mqtt((Device_Id_As_Publish_Topic+"system/timer/"+String(i+1)+"/fan/"+String(l+1)+"/"),String(master.system.timer[i].fan[l]),true);
        }
      }
    }
  }

  for(int i=0;i<SCENE_NUMBER;i++){
      if(master.system.scene[i].change == true){
      publish_mqtt((Device_Id_As_Publish_Topic+"system/scene/"+String(i+1)+"/"),String(master.system.scene[i].current_state),true);
        if(master.system.scene[i].current_state == true){
          //Timer Selected Devices
          for(int k=0;k<master.slave.RELAY_NUMBER;k++){
            publish_mqtt((Device_Id_As_Publish_Topic+"system/scene/"+String(i+1)+"/relay/"+String(k+1)+"/"),String(master.system.scene[i].relay[k]),true);
          }
          for(int l=0;l<master.slave.FAN_NUMBER;l++){
            publish_mqtt((Device_Id_As_Publish_Topic+"system/scene/"+String(i+1)+"/fan/"+String(l+1)+"/"),String(master.system.scene[i].fan[l]),true);
        }
      }
    }
  }
}

void HomeHub::timesensor_handler(){
  if(mqttclient.connected()){                                 //MQTT client is connected, we have a working internet connection lets use the NTP instead of the clock for time.
    //Serial.println(timeClient.getFormattedTime());
    master.system.clock.hour = timeClient.getHours();         //Get Hours from the NTP and put them into local Variables
    master.system.clock.minute = timeClient.getMinutes();     //Get Minutes from the NTP and put them into local Variables
    master.system.clock.second = timeClient.getSeconds();     //Get Seconds from the NTP and put them into local Variables
    if(master.system.clock.second == 0){                      //Check for loss of synchronization between DS3231 and Internal Variables at every oth second 
      if(Clock.getMinute() != master.system.clock.minute){    //If the system variable minute is not same as NTP when we are online, lets sync the DS3231 clock.
        //Serial.println("Setting time flag");
        notification_handler("system/clock/sync/","1");
        master.system.flag.set_time = true;
      }
    }
  }
  else{
    if(master.system.flag.set_time == false){ 
      master.system.clock.date = Clock.getDate();
      master.system.clock.month = Clock.getMonth(master.system.clock.century);
      master.system.clock.year = Clock.getYear();
      master.system.clock.week = Clock.getDoW();
      master.system.clock.hour = Clock.getHour(master.system.clock.h12,master.system.clock.pmam);
      master.system.clock.minute = Clock.getMinute();
      master.system.clock.second = Clock.getSecond();
      master.system.clock.temp = Clock.getTemperature();
      
    // 1: Date 2: Month 3: Year 4: Week 5:HouR 6:Minute 7: Second 8: temperature 9: Mode 10: Oscillator issue
    if((master.system.clock.date >31)||((master.system.clock.date <0))){
      master.system.clock.error_code = 1;
      master.system.clock.error = true;
    }
    else if((master.system.clock.month >12)||((master.system.clock.month <0))){
      master.system.clock.error_code = 2;
      master.system.clock.error = true;
    }
    else if((master.system.clock.year > 30)||((master.system.clock.year < 19))){
      master.system.clock.error_code = 3;
      master.system.clock.error = true;
    }
    else if((master.system.clock.week >7)||((master.system.clock.week <0))){
      master.system.clock.error_code = 4;
      master.system.clock.error = true;
    }
    else if((master.system.clock.hour >24)||((master.system.clock.hour <0))){
      master.system.clock.error_code = 5;
      master.system.clock.error = true;
    }
    else if((master.system.clock.minute >60)||((master.system.clock.minute <0))){
      master.system.clock.error_code = 6;
      master.system.clock.error = true;
    }
    else if((master.system.clock.second >60)||((master.system.clock.second <0))){
      master.system.clock.error_code = 7;
      master.system.clock.error = true;
    }
    else if((master.system.clock.temp >70)||((master.system.clock.temp <10))){
      master.system.clock.error_code = 8;
      master.system.clock.error = true;
    }
    //else if((Clock.oscillatorCheck())){
    //  Td_sensor.Error_code = 10;
    //  Td_sensor.Error = 0;
    //}
    else{
      master.system.clock.error_code = 0;
      master.system.clock.error = false;
    }/*
    if(EEPROM.read(timesensor_memspace[1]) != timesensor_error){
        EEPROM.write(timesensor_memspace[1],timesensor_error);
      }
    if(EEPROM.read(timesensor_memspace[2]) !=  timesensor_error_code){
        EEPROM.write(timesensor_memspace[2],timesensor_error_code);
      } */
  }
  }
  //For manual Setting via MQTT client or from NTP, basically it stores the data into the DS3231 from Local Variables.
  if(master.system.flag.set_time == true){ 
    Clock.setYear(master.system.clock.year);
    Clock.setMonth(master.system.clock.month);
    Clock.setDate(master.system.clock.date);
    Clock.setDoW(master.system.clock.week);
    Clock.setHour(master.system.clock.hour);
    Clock.setMinute(master.system.clock.minute);
    Clock.setSecond(master.system.clock.second);
    master.system.flag.set_time = false;
  }
}

void HomeHub::update_device(){
  notification_handler("system/update/","1");
  end_mqtt();
    if (WiFi.status() == WL_CONNECTED){
    t_httpUpdate_return ret = ESPhttpUpdate.update(_Client, _host_update, _firmware_version);
    switch (ret) {
      case HTTP_UPDATE_FAILED:
            HomeHub_DEBUG_PRINT("HTTP_UPDATE_FAILD Error (");//%d): %s\n");//,ESPhttpUpdate.getLastError(),ESPhttpUpdate.getLastErrorString().c_str());
        break;
      case HTTP_UPDATE_NO_UPDATES:
        HomeHub_DEBUG_PRINT("HTTP_UPDATE_NO_UPDATES");
        break;

      case HTTP_UPDATE_OK:
        HomeHub_DEBUG_PRINT("HTTP_UPDATE_OK");
        break;
    }
    }
    else{
        HomeHub_DEBUG_PRINT("Device is Offline, cant update.");
    }
  initiate_mqtt();
}

void HomeHub::update_device(String host_update){
  notification_handler("system/update/","1");
  end_mqtt();
    if (WiFi.status() == WL_CONNECTED){
    t_httpUpdate_return ret = ESPhttpUpdate.update(_Client, host_update);
    switch (ret) {
      case HTTP_UPDATE_FAILED:
            HomeHub_DEBUG_PRINT("HTTP_UPDATE_FAILD Error" + String(ESPhttpUpdate.getLastErrorString()));
        break;
      case HTTP_UPDATE_NO_UPDATES:
        HomeHub_DEBUG_PRINT("HTTP_UPDATE_NO_UPDATES");
        break;
      case HTTP_UPDATE_OK:
        HomeHub_DEBUG_PRINT("HTTP_UPDATE_OK");
        break;
    }
    }
    else{
        HomeHub_DEBUG_PRINT("Device is Offline, cant update.");
    }
  initiate_mqtt();
}

int HomeHub::mdns_handler(){
  // Check for any mDNS queries and send responses
  mdns->update();
  
  // Check if a client has connected
  WiFiClient client = server->available();
  if (!client) {
    return(20);
  }
  
  //New client has connected
  HomeHub_DEBUG_PRINT("New client");

  // Wait for data from client to become available
  while(client.connected() && !client.available()){
    delay(1);
   }
  
  // Read the first line of HTTP request
  String req = client.readStringUntil('\r');
  
  // First line of HTTP request looks like "GET /path HTTP/1.1"
  // Retrieve the "/path" part by finding the spaces
  int addr_start = req.indexOf(' ');
  int addr_end = req.indexOf(' ', addr_start + 1);
  if (addr_start == -1 || addr_end == -1) {
    HomeHub_DEBUG_PRINT("Invalid request: ");
    HomeHub_DEBUG_PORT.println(req);
    return(20);
   }
  req = req.substring(addr_start + 2, addr_end);
  client.flush();
  client.print(mdns_input_handler(req));
  HomeHub_DEBUG_PRINT("Done with client");
  return(20);
}

String HomeHub::mdns_input_handler(String req){
  //Put all Commands to handler in mdns offline requests here(mostly is same as mqtt input handler)
  HomeHub_DEBUG_PRINT("Handling Normal Mdns Request");
  HomeHub_DEBUG_PRINT(req);
  String s;
  String sub1;
  if(req.length() > 0){
  sub1 = req.substring(0,req.indexOf('/',0));
  req.remove(0,req.indexOf('/',0)+1);
  }
  if(req == "")
  {
    //IPAddress ip = WiFi.softAPIP();
    //String ipStr = String(ip[0]) + '.' + String(ip[1]) + '.' + String(ip[2]) + '.' + String(ip[3]);
    s = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n<!DOCTYPE HTML>\r\n<html>Welcome to the world of TNM"; /*
    s += ipStr;
    s += "<p>";
    s += _wifi_data;
    s += "<form method='get' action='a'><label>SSID: </label><input name='ssid' length=32><input name='pass' length=64><input type='submit'></form>";
    */s += "</html>\r\n\r\n";
    HomeHub_DEBUG_PRINT("Sending 200");
  }
  else if(req == "wifisetup")
  {/*
    IPAddress ip = WiFi.softAPIP();
    String ipStr = String(ip[0]) + '.' + String(ip[1]) + '.' + String(ip[2]) + '.' + String(ip[3]);
    s = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n<!DOCTYPE HTML>\r\n<html>Hello from ESP8266 at ";
    s += ipStr;
    s += "<p>";
    s += _wifi_data;
    s += "<form method='get' action='a'><label>SSID: </label><input name='ssid' length=32><input name='pass' length=64><input type='submit'></form>";
    s += "</html>\r\n\r\n"; */
    s += _wifi_data;
    HomeHub_DEBUG_PRINT("Sending 200");
  }
  else if ( req.startsWith("a?ssid=") ) {
    // /a?ssid=blahhhh&pass=poooo
    HomeHub_DEBUG_PRINT("clearing eeprom");
    saved_wifi_dump();
    String qsid;
    qsid = req.substring(7,req.indexOf('&'));
    qsid.replace("+"," ");
    //Serial.println(qsid);
   // Serial.println("");
    String qpass;
    qpass = req.substring(req.lastIndexOf('=')+1);
      qpass.replace("+"," ");
    //Serial.println(qpass);
    //Serial.println("");
    
    HomeHub_DEBUG_PRINT("writing eeprom ssid:");
    save_wifi_data(qsid,qpass);
      s = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n<!DOCTYPE HTML>\r\n<html>";
      s += "Press Next to Continue.</html>\r\n\r\n";
    WiFi.disconnect();
  }
  else if(sub1 == "sensor"){ //Example of handiling incomming requests : sensor/
      if(req.length() > 0){
        sub1 = req.substring(0,req.indexOf('/',0));
        req.remove(0,req.indexOf('/',0)+1);;
        if(sub1 == "0"){  //Example of handiling incomming requests : sensor/0/
            if(req.length() > 0){
              sub1 = req.substring(0,req.indexOf('/',0));
              req.remove(0,req.indexOf('/',0)+1);
              if(sub1 == "threshold"){ //Example of handiling incomming requests : sensor/0/threhold/
                sub1 = req.substring(0,req.indexOf('/',0));
                req.remove(0,req.indexOf('/',0)+1);
                  if(sub1 == ""){ //Example of handiling incomming requests : sensor/0/threshold/
                      s = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n<!DOCTYPE HTML>\r\n<html>sensor/0/threshold/";
                      //s += String(master.gassensor.threshold.current_data);  //Sending data here
                      s += "<p>";
                      s += "</html>\r\n\r\n";
                  }
                  else{//Example of handiling incomming requests : sensor/0/threshold/value/
                    //master.gassensor.threshold.current_data = sub1.toInt();
                    s = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n<!DOCTYPE HTML>\r\n<html>";
                    //s += String(master.gassensor.threshold.current_data);    //Sending data here
                    s += "<p>";
                    s += "</html>\r\n\r\n";
                    HomeHub_DEBUG_PRINT("Threshold changed.");
                  }
              }
              if(sub1 == "test"){//Example of handiling incomming requests : sensor/0/test/
                //master.gassensor.test = true;
                s = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n<!DOCTYPE HTML>\r\n<html>";
                s += "<p>";
                s += "</html>\r\n\r\n";
                HomeHub_DEBUG_PRINT("Testing Sensor.");
              }
            }
        }
     }
  }
  else
  {
   s = "HTTP/1.1 404 Not Found\r\n\r\n";
   HomeHub_DEBUG_PRINT("Sending 404");
  }
  return s;
}

void HomeHub::webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
  switch(type) {
    case WStype_DISCONNECTED:
      isConnected = false;    
      HomeHub_DEBUG_PRINT("Webservice disconnected from sinric.com!");
      break;
    case WStype_CONNECTED: {
      isConnected = true;
      HomeHub_DEBUG_PRINT("Service connected to sinric.com");
      HomeHub_DEBUG_PRINT("Waiting for commands from sinric.com ...");        
      }
      break;
    case WStype_TEXT: {
        //Initiate JSON6 handler 
        DynamicJsonDocument json(1024);
        deserializeJson(json, (char*) payload);      
        
        String deviceId = json ["deviceId"];     
        String action = json ["action"];
        
        if(action == "setPowerState") { // Switch or Light
            String value = json ["value"];
            if(value == "ON") {
              for(int i=0;i<master.slave.RELAY_NUMBER;i++){
                if(deviceId == master.slave.RELAY_SINRIC_ID[i]){
                  master.slave.relay[i].current_state = bool(true);
                }
              }
            }
            else{
              for(int i=0;i<master.slave.RELAY_NUMBER;i++){
                if(deviceId == master.slave.RELAY_SINRIC_ID[i]){
                  master.slave.relay[i].current_state = bool(false);
                }
              }
            }
        }
        else if (action == "SetTargetTemperature") {
            String deviceId = json ["deviceId"];     
            String action = json ["action"];
            String value = json ["value"];
        }
        else if (action == "test") {
            HomeHub_DEBUG_PRINT("Received test command from sinric.com");
        }
      }
      break;
    case WStype_BIN:
      Serial.printf("[WSc] get binary length: %u\n", length);
      break;
  }
}

void HomeHub::slave_output_handler(){
    if(master.system.flag.slave_handshake == true){ 
        if((master.slave.change == true)){ //&& ((master.slave.all_relay_lastslavecommand == false) || (master.slave.RELAY_NUMBER == 0)) && ((master.slave.all_fan_lastslavecommand == false) || (master.slave.FAN_NUMBER == 0)) && ((master.slave.all_sensor_lastslavecommand == false) || (master.slave.SENSOR_NUMBER == 0))){
            StaticJsonDocument<600> doc;
            doc["ROLE"] = "MASTER";
            JsonObject device = doc.createNestedObject("DEVICE");
            if(master.slave.all_relay_change == true){
                //if((master.slave.all_relay_lastslavecommand == false)){
                JsonArray relay = device.createNestedArray("RELAY");
                for(int i=0;i<master.slave.RELAY_NUMBER;i++){
                  JsonObject relay_x = relay.createNestedObject();
                    if(master.slave.relay[i].change == true){
                        //if((master.slave.relay[i].lastslavecommand == false)){
                            relay_x["STATE"] = master.slave.relay[i].current_state;
                            relay_x["VALUE"] = master.slave.relay[i].current_value;
                        //}
                    }
                }
            //}
            }
            if(master.slave.all_fan_change == true){
                //if((master.slave.all_fan_lastslavecommand == false)){
                JsonArray fan = device.createNestedArray("FAN");
                for(int i=0;i<master.slave.FAN_NUMBER;i++){
                  JsonObject fan_x = fan.createNestedObject();
                    if(master.slave.fan[i].change == true){
                        //if((master.slave.fan[i].lastslavecommand == false)){
                            fan_x["STATE"] = master.slave.fan[i].current_state;
                            fan_x["VALUE"] = master.slave.fan[i].current_value;
                        //}
                    }
                }
                //}
            }
            if(master.slave.all_sensor_change == true){
                //if((master.slave.all_sensor_lastslavecommand == false)){
                JsonArray sensor = device.createNestedArray("SENSOR");
                for(int i=0;i<master.slave.SENSOR_NUMBER;i++){
                  JsonObject sensor_x = sensor.createNestedObject();
                    if(master.slave.sensor[i].change == true){
                        //if((master.slave.sensor[i].lastslavecommand == false)){
                            sensor_x["VALUE"] = master.slave.sensor[i].current_value;
                      //}
                    }
                }
                //}
            }
            //Command
            String command = "NULL";
            if(command != "NULL"){
              doc["COMMAND"] = command;
            }
            serializeJson(doc, HomeHub_SLAVE_DATA_PORT);
        }
    }
}

void HomeHub::slave_input_handler(){
    salve_serial_capture();
    //Proceed with receiving handshake or sync input from slave
    if(master.system.flag.slave_handshake == false){
        if(_slave_handshake_millis == 0){
            String request_handshake = "{\"NAME\":\"" + String(master.NAME) + "\",\"ROLE\":\"MASTER\",\"COMMAND\":\"HANDSHAKE\"}";
            HomeHub_SLAVE_DATA_PORT.print(request_handshake);
            _slave_handshake_millis = millis() + 3000;
        }
        else{
            if(millis() > _slave_handshake_millis){
                HomeHub_DEBUG_PRINT("In Slave handshake handler");
                if(master.system.flag.received_json == true){ 
                  DynamicJsonDocument doc(1024);
                  deserializeJson(doc, _slave_command_buffer);
                  JsonObject obj = doc.as<JsonObject>();
                  master.slave.NAME = doc["NAME"];
                  String role = doc["ROLE"];
                  String command = doc["COMMAND"];
                  JsonArray relays = doc["DEVICE"]["RELAY"];
                  JsonArray fans = doc["DEVICE"]["FAN"];
                  JsonArray sensors = doc["DEVICE"]["SENSOR"];
                  master.slave.RELAY_NUMBER = relays.size();
                  master.slave.FAN_NUMBER = fans.size();
                  master.slave.SENSOR_NUMBER = sensors.size();
                  int index = 0;
                  for (JsonObject repo : relays){
                    if(index < master.slave.RELAY_NUMBER){
                      if(repo.containsKey("STATE")) { //Check if Key is actually present in it, or it will insert default values in places of int
                        master.slave.relay[index].current_state = repo["STATE"].as<bool>();
                        master.slave.relay[index].lastslavecommand = true;
                      }/*
                      if(repo.containsKey("VALUE")) {
                        master.slave.relay[index].current_value = repo["VALUE"].as<int>();
                        master.slave.relay[index].lastslavecommand = true;
                      }*/
                    }
                    index++;
                  }
                  index = 0;
                  for (JsonObject repo : fans){
                    if(index < master.slave.FAN_NUMBER){
                      if(repo.containsKey("STATE")) { //Check if Key is actually present in it, or it will insert default values in places of int
                        master.slave.fan[index].current_state = repo["STATE"].as<bool>();
                        master.slave.fan[index].lastslavecommand = true;
                      }
                      if(repo.containsKey("VALUE")) {
                        master.slave.fan[index].current_value = repo["VALUE"].as<int>();
                        master.slave.fan[index].lastslavecommand = true;
                      }
                    }
                    index++;
                  }
                  index = 0;
                  for (JsonObject repo : sensors){
                    if(index < master.slave.SENSOR_NUMBER){
                      if(repo.containsKey("TYPE")) { //Check if Key is actually present in it, or it will insert default values in places of int
                        master.slave.sensor[index].type = repo["TYPE"].as<char *>();
                        master.slave.sensor[index].lastslavecommand = true;
                      }
                      if(repo.containsKey("VALUE")) {
                        master.slave.sensor[index].current_value = repo["VALUE"].as<int>();
                        master.slave.sensor[index].lastslavecommand = true;
                      }
                    }
                    index++;
                  }
                  master.system.flag.received_json = false;
                  if(role == "SLAVE" && command == "HANDSHAKE"){
                    HomeHub_DEBUG_PRINT("Handshake Command received.");
                    master.system.flag.slave_handshake = true;
                    //Publish MQTT handshake to the cloud 
                    mqtt_handshake_handler();
                  }
                  else{
                    HomeHub_DEBUG_PRINT("Right Handshake Commands Not received.");
                  } 
                }
                _slave_handshake_millis = 0;
            }
        }
    }
    //if handshake has be completed only guide to slave syncronisation command handler
    else{ 
      if(master.system.flag.received_json == true){
        //Rester Last Slave Command flags to false
        for(int i=0;i<master.slave.RELAY_NUMBER;i++){
            master.slave.relay[i].lastslavecommand = false;
        }
        for(int i=0;i<master.slave.FAN_NUMBER;i++){
            master.slave.fan[i].lastslavecommand = false;
        }
        for(int i=0;i<master.slave.SENSOR_NUMBER;i++){
            master.slave.sensor[i].lastslavecommand = false;
        }
        DynamicJsonDocument doc(1024);
        deserializeJson(doc, _slave_command_buffer);
        JsonObject obj = doc.as<JsonObject>();
        JsonArray relays = doc["DEVICE"]["RELAY"];
        JsonArray fans = doc["DEVICE"]["FAN"];
        JsonArray sensors = doc["DEVICE"]["SENSOR"];
        master.slave.NAME = doc["NAME"];
        int index = 0; 
        for (JsonObject repo : relays){ 
          if(index < master.slave.RELAY_NUMBER){
            if(repo.containsKey("STATE")){ //Check if Key is actually present in it, or it will insert default values in places of int
                master.slave.relay[index].current_state = repo["STATE"].as<bool>();
                master.slave.relay[index].lastslavecommand = true;
            }
            /*if(repo.containsKey("VALUE")){
                master.slave.relay[index].current_state = repo["VALUE"].as<int>();
                master.slave.relay[index].lastslavecommand = true;
            }*/
          }
          index++;
        }
        index = 0;
        for (JsonObject repo : fans){
          if(index < master.slave.FAN_NUMBER){
            //if(repo.containsKey("STATE")) { //Check if Key is actually present in it, or it will insert default values in places of int
                master.slave.fan[index].current_state = repo["STATE"].as<bool>();
                master.slave.fan[index].lastslavecommand = true;
            //}
            //if(repo.containsKey("VALUE")) {
                master.slave.fan[index].current_value = repo["VALUE"].as<int>();
                master.slave.fan[index].lastslavecommand = true;
            //}
          }
          index++;
        }
        index = 0;
        for (JsonObject repo : sensors){
          if(index < master.slave.SENSOR_NUMBER){ 
            /* No need to send the device type detials again and again after Handhshake
            if(repo.containsKey("TYPE")) { //Check if Key is actually present in it, or it will insert default values in places of int
                master.slave.sensor[index].type = repo["TYPE"].as<char *>();
                master.slave.sensor[index].lastslavecommand = true;
            }*/
            if(repo.containsKey("VALUE")) { 
                master.slave.sensor[index].current_value = repo["VALUE"].as<int>();
                master.slave.sensor[index].lastslavecommand = true;
            }
          }
          index++;
        }
      master.system.flag.received_json = false;
      const char* command = doc["COMMAND"];
      if(command != nullptr){
        slave_receive_command(command);
      }
    }
  }
}

void HomeHub::slave_receive_command(const char* command){
    char Command[20];
    strlcpy(Command, command,20);
    if((strncmp(Command,"WIFI_SETUP_START",20) == 0)){
        HomeHub_DEBUG_PRINT("Starting Wifi Setup AP");
        //initiate_wifi_setup();
    }
    else if((strncmp(Command,"WIFI_SETUP_STOP",20) == 0)){
        HomeHub_DEBUG_PRINT("Stopping Wifi Setup AP");
        //end_wifi_setup();
    }
    else if((strncmp(Command,"WIFI_SAVED_CONNECT",20) == 0)){
        HomeHub_DEBUG_PRINT("Connecting to presaved Wifi");
        //saved_wifi_connect();
    }
    else if((strncmp(Command,"UPDATE_DEVICE",20) == 0)){
        HomeHub_DEBUG_PRINT("Connecting to presaved Wifi");
        //update_device();
    }
    else if((strncmp(Command,"ALL_LONGPRESS",20) == 0)){
        HomeHub_DEBUG_PRINT("Long Press command received from Slave");
        //initiate_wifi_setup();
    }
    else{
        HomeHub_DEBUG_PRINT("Unsupported Command");
    }
}

void HomeHub::salve_serial_capture(){
    //Read incomming bit per cycle and decode the incomming commands
    if(HomeHub_SLAVE_DATA_PORT.available()) {
        char c = HomeHub_SLAVE_DATA_PORT.read();
        if (c == '{'){
            if(_SLAVE_DATA_PORT_counter==0){
                master.system.flag.receiving_json = true;
                _SLAVE_DATA_PORT_command = "";
            }
            _SLAVE_DATA_PORT_counter+=1;
            _SLAVE_DATA_PORT_command += c;
        }
        else if (c == '}'){
            _SLAVE_DATA_PORT_counter+=-1;
            _SLAVE_DATA_PORT_command += c;
        }
        else{
            _SLAVE_DATA_PORT_command += c;
        }
        if(_SLAVE_DATA_PORT_counter == 0 && master.system.flag.receiving_json == true){
            _slave_command_buffer = _SLAVE_DATA_PORT_command; 
            _SLAVE_DATA_PORT_command = "";
            master.system.flag.receiving_json = false;
            master.system.flag.received_json = true;
        }
    }
}
