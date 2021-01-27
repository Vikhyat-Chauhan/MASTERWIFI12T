#define rom_address 0x57
//in the normal write anything the eeaddress is incrimented after the writing of each byte. The Wire library does this behind the scenes.

template <class T>
int eeWrite(int ee, const T& value){
  Serial.print("HomeHub : ");
  Serial.println(sizeof(value));
  const byte* p = (const byte*)(const void*)&value;
  int i;
  Wire.beginTransmission(rom_address);
  Wire.write((int)(ee >> 8)); // MSB
  Wire.write((int)(ee & 0xFF)); // LSB
  for (i = 0; i < sizeof(value); i++)
  Wire.write(*p++);
  Wire.endTransmission();
  return i;
}

template <class T> 
int eeRead(int ee, T& value){
  Serial.print("HomeHub : ");
  Serial.println(sizeof(value));
  byte* p = (byte*)(void*)&value;
  int i;
  Wire.beginTransmission(rom_address);
  Wire.write((int)(ee >> 8)); // MSB
  Wire.write((int)(ee & 0xFF)); // LSB
  Wire.endTransmission();
  Wire.requestFrom(rom_address,sizeof(value));
  for (i = 0; i < sizeof(value); i++)
  if(Wire.available())
  *p++ = Wire.read();
  return i;
}
