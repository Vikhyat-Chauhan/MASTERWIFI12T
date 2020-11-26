 /*
* HomeHub.cpp - An TNM Home Automation Library
* Created by Vikhyat Chauhan @ TNM on 9/11/19
* www.thenextmove.in
* Revision #10 - See readMe
*/

#include "HomeHub.h"
#include <Arduino.h>

WiFiClient _Client;
PubSubClient mqttclient(_Client);
DS3231 Clock;

HomeHub::HomeHub(){
    //Initilize Serial Lines
    HomeHub_DEBUG_PORT.begin(HomeHub_DEBUG_PORT_BAUD);
    //Initiate memory system
    initiate_memory();
    HomeHub_DEBUG_PRINT("STARTED");
}

void HomeHub::asynctasks(){
    wifi_handler();
    device_handler(); // Should be only placed at end of each loop since it monitors changes
    timesensor_handler();
}

void HomeHub::wifi_handler(){
  if(functionstate.wifi_handler == false){        //Wifi Handler first started, handling setup.
    functionstate.wifi_handler = true;
    WiFi.mode(WIFI_AP_STA);
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
          if(master.flag.saved_wifi_present == true){ Serial.println("Saved Wifi Present");
            saved_wifi_connect();
          }
      }
    }
    else{ //All network based fucntions here.
        if(!(mqttclient.connected())){
          initiate_mqtt();                        Serial.println("Initiate Mqtt connection");
        }
        else{
            mqttclient.loop();
            mqtt_output_handler();
        }
      }
    }
  mdns_handler();
}

void HomeHub::device_handler(){
    //Add Device hardware variable change monitoring features here
}

//Initiate Memory
void HomeHub::initiate_memory(){
    functionstate.initiate_memory = true;
    if(master.flag.rom_external == false){
        EEPROM.begin(512);
    }
    else{
        Wire.begin();
    }
}

//Custom EEPROM replacement functions
void HomeHub::rom_write(unsigned int eeaddress, byte data) {
    if(master.flag.rom_external == false){
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
byte HomeHub::rom_read(unsigned int eeaddress) {
    if(master.flag.rom_external == false){
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
    if(master.flag.rom_external == false){
        
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
      _esid += char(rom_read(_wifi_data_memspace+i)); //_esid += char(EEPROM.read(i));
    }
  HomeHub_DEBUG_PRINT("SSID:");HomeHub_DEBUG_PORT.println(_esid);
  _epass = "";
  for(int i = 32;i < 96;++i)
    {
      _epass += char(rom_read(_wifi_data_memspace+i)); //_epass += char(EEPROM.read(i));
    }
  HomeHub_DEBUG_PRINT("PASS:");HomeHub_DEBUG_PORT.println(_epass);
  if(_esid.length() > 1){
    return true;
  }
  else{
    return false;
  }
}

bool HomeHub::test_wifi() {
  HomeHub_DEBUG_PRINT("Waiting for Wifi to connect");
  for(int i=0;i<100;i++){
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
  
  wifi_data = "<ul>";
  for (int i = 0; i < network_available_number; ++i)
    {
      // Print SSID and RSSI for each network found
      /*Serial.print(i + 1);Serial.print(": ");Serial.print(WiFi.SSID(i));
      Serial.print(" (");Serial.print(WiFi.RSSI(i));Serial.print(")");
      Serial.println((WiFi.encryptionType(i) == ENC_TYPE_NONE)?" ":"*");
      */
      // Print SSID and RSSI for each network found
      wifi_data += "<li>";
      wifi_data +=i + 1;
      wifi_data += ": ";
      wifi_data += WiFi.SSID(i);
      wifi_data += " (";
      wifi_data += WiFi.RSSI(i);
      wifi_data += ")";
      wifi_data += (WiFi.encryptionType(i) == ENC_TYPE_NONE)?" ":"*";
      wifi_data += "</li>";
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
        master.flag.saved_wifi_present = true;
      }
    }
    wifi_data += "</ul>";
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

bool HomeHub::saved_wifi_connect(){ Serial.println("Saved Wifi Connect");
  if(retrieve_wifi_data()){
      WiFi.begin(_esid.c_str(), _epass.c_str());
      bool connection_result = test_wifi();
      if(connection_result){
          if(master.flag.boot_check_update == true){
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
        if(master.flag.boot_check_update == true){
            update_device();
        }
    }
    return (connection_result);
}

void HomeHub::save_wifi_data(String ssid, String password){
    Serial.println("Saved Wifi data");
    for (int i = 0; i < ssid.length(); ++i)
      {
        rom_write(_wifi_data_memspace+i, ssid[i]); //EEPROM.write(i, qsid[i]);
        HomeHub_DEBUG_PRINT("Wrote: ");
        HomeHub_DEBUG_PORT.println(ssid[i]);
        _esid = ssid;
      }
    HomeHub_DEBUG_PRINT("writing eeprom pass:");
    for (int i = 0; i < password.length(); ++i)
      {
        rom_write(_wifi_data_memspace+32+i, password[i]); //EEPROM.write(32+i, qpass[i]);
        HomeHub_DEBUG_PRINT("Wrote: ");
        HomeHub_DEBUG_PORT.println(password[i]);
        _epass = password;
      }
}

void HomeHub::saved_wifi_dump(){
  HomeHub_DEBUG_PRINT("clearing eeprom");
  for (int i = 0; i < 96; ++i) {
      rom_write(_wifi_data_memspace+i,0);// EEPROM.write(i, 0);
  }
  //EEPROM.commit();
}

void HomeHub::mqtt_handshake_handler(){
  //Add all the mqtt publish messages that are to be send when first connecting to mqtt cloud
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
        publish_mqtt(topic,"0");
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

HomeHub& HomeHub::setCallback(MQTT_CALLBACK_SIGN)
{
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
    HomeHub_DEBUG_PORT.println("Topic : "+topic);
    HomeHub_DEBUG_PORT.println("Payload : "+payload);
    //Add all device hardware change and variable changes that are to be mapped with incomming Mqtt data here
}

void HomeHub::publish_mqtt(String message)
{
    mqttclient.publish(Device_Id_In_Char_As_Publish_Topic, message.c_str(), true);
}

void HomeHub::publish_mqtt(String topic, String message)
{
    
    mqttclient.publish(topic.c_str(), message.c_str(),true);
}

void HomeHub::mqtt_output_handler(){
    //add all variables and fucntions that monitor change and publish data to Mqtt server here
}

void HomeHub::timesensor_handler(){
  if(_timesensor_set == 0){
    Clock.setYear(_timesensor_year);
    Clock.setMonth(_timesensor_month);
    Clock.setDate(_timesensor_date);
    Clock.setDoW(_timesensor_week);
    Clock.setHour(_timesensor_hour);
    Clock.setMinute(_timesensor_minute);
    Clock.setSecond(_timesensor_second);
  }
  if(_timesensor_set == 1){
      _timesensor_date = Clock.getDate();
      _timesensor_month = Clock.getMonth(_timesensor_century);
      _timesensor_year = Clock.getYear();
      _timesensor_week = Clock.getDoW();
      _timesensor_hour = Clock.getHour(_timesensor_h12,_timesensor_pmam);
      _timesensor_minute = Clock.getMinute();
      _timesensor_second = Clock.getSecond();
      _timesensor_temp = Clock.getTemperature();
      
    // 1: Date 2: Month 3: Year 4: Week 5:HouR 6:Minute 7: Second 8: temperature 9: Mode 10: Oscillator issue
    if((_timesensor_date >31)||((_timesensor_date <0))){
      _timesensor_error_code = 1;
      _timesensor_error = 0;
    }
    else if((_timesensor_month >12)||((_timesensor_month <0))){
      _timesensor_error_code = 2;
      _timesensor_error = 0;
    }
    else if((_timesensor_year > 30)||((_timesensor_year < 19))){
      _timesensor_error_code = 3;
      _timesensor_error = 0;
    }
    else if((_timesensor_week >7)||((_timesensor_week <0))){
      _timesensor_error_code = 4;
      _timesensor_error = 0;
    }
    else if((_timesensor_hour >24)||((_timesensor_hour <0))){
      _timesensor_error_code = 5;
      _timesensor_error = 0;
    }
    else if((_timesensor_minute >60)||((_timesensor_minute <0))){
      _timesensor_error_code = 6;
      _timesensor_error = 0;
    }
    else if((_timesensor_second >60)||((_timesensor_second <0))){
      _timesensor_error_code = 7;
      _timesensor_error = 0;
    }
    else if((_timesensor_temp >70)||((_timesensor_temp <10))){
      _timesensor_error_code = 8;
      _timesensor_error = 0;
    }
    //else if((Clock.oscillatorCheck())){
    //  Td_sensor.Error_code = 10;
    //  Td_sensor.Error = 0;
    //}
    else{
      _timesensor_error_code = 0;
      _timesensor_error = 1;
    }/*
    if(EEPROM.read(timesensor_memspace[1]) != timesensor_error){
        EEPROM.write(timesensor_memspace[1],timesensor_error);
      }
    if(EEPROM.read(timesensor_memspace[2]) !=  timesensor_error_code){
        EEPROM.write(timesensor_memspace[2],timesensor_error_code);
      } */
  }
}

void HomeHub::update_device()
{
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

void HomeHub::update_device(String host_update)
{
  publish_mqtt((Device_Id_As_Publish_Topic+"system/update/"),"1");
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

int HomeHub::mdns_handler()
{
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
    IPAddress ip = WiFi.softAPIP();
    String ipStr = String(ip[0]) + '.' + String(ip[1]) + '.' + String(ip[2]) + '.' + String(ip[3]);
    s = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n<!DOCTYPE HTML>\r\n<html>Hello from ESP8266 at ";
    s += ipStr;
    s += "<p>";
    s += _wifi_data;
    s += "<form method='get' action='a'><label>SSID: </label><input name='ssid' length=32><input name='pass' length=64><input type='submit'></form>";
    s += "</html>\r\n\r\n";
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
      s += "Successfully connected";
      s += req;
      s += "<p> saved to eeprom... reset to boot into new wifi</html>\r\n\r\n";
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
