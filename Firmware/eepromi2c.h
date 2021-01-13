#define rom_address 0x57
//in the normal write anything the eeaddress is incrimented after the writing of each byte. The Wire library does this behind the scenes.

// NOTE: Currently has issues with storing data in internal EEPROM

template <class T>
int eeWrite(int ee, const T& value,bool external){
  if(external){
    const byte* p = (const byte*)(const void*)&value;
    int i;
    Wire.beginTransmission(rom_address);
    Wire.write((int)(ee >> 8)); // MSB
    Wire.write((int)(ee & 0xFF)); // LSB
    for (i = 0; i < sizeof(value); i++)
      Wire.write(*p++);
    Wire.endTransmission();delay(4);
    return i;
  }
  else{
    const byte* p = (const byte*)(const void*)&value;
    int i;
    for (i = 0; i < sizeof(value); i++)
      EEPROM.write(ee+i,*p++);delay(20);
      EEPROM.commit();
    return i;
  }
}

template <class T>
int eeRead(int ee, T& value,bool external){
  if(external){
    byte* p = (byte*)(void*)&value;
    int i;
    Wire.beginTransmission(rom_address);
    Wire.write((int)(ee >> 8)); // MSB
    Wire.write((int)(ee & 0xFF)); // LSB
    Wire.endTransmission();
    Wire.requestFrom(rom_address,sizeof(value));
    for (i = 0; i < sizeof(value); i++)
      if(Wire.available())
        *p++ = Wire.read();delay(4);
    return i;
  }
  else{
    byte* p = (byte*)(void*)&value;
    int i;
    for (i = 0; i < sizeof(value); i++)
        *p++ = EEPROM.read(ee+i);
    return i;
  }
}
