#include <Arduino.h>
#include <ArduinoJson.h>
#include <SoftwareSerial.h>
#include <math.h>
#include "LowPower.h"

// Sensors
#include <Wire.h>
#include "Adafruit_MCP9808.h"
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <BH1750.h>
#include "Adafruit_SI1145.h"
#include <PMserial.h>

#define NODE_ID           103   // Device ID, for internal purpose
#define CONFIG_PIN        3     // D3, set maintenance mode
#define SLEEP_TIME        37    // x * 8s = sleep time
#define PMS_SET           2     // D2
#define RX_PIN            8     // D8
#define TX_PIN            9     // D9
#define SET_PIN           7     // D7
#define MEASURE_NUMBER    2     // Number of Smog measurements to average
#define BATTERY_PIN       A0    // Battery voltage measurement
#define PANEL_PIN         A1    // Solar Panel voltage measurement
#define BATTERY_DIVIDER   0.232558      // R2 = 100k, R1 = 330k   R2/(R1 + R2)
#define PANEL_DIVIDER     0.128205      // R5 = 75k, R4 = 510k   R5/(R4 + R5)

// Sensors config data
#define ALTITUDE        515.0 //define altitude of location
#define BME280_ADDR     0x76  //BME280 I2C Address
#define MCP9808_ADDR    0x18  //MCP9808 I2C Address

// I2C settings
#define BME_SCK         28  //SCL
#define BME_CS          27  //SDA

SerialPM pms(PMSx003, Serial);          // PMSx003, UART
SoftwareSerial HC12(RX_PIN, TX_PIN);    // HC-12 TX Pin, HC-12 RX Pin

Adafruit_MCP9808 tempsensor = Adafruit_MCP9808();   // temp
Adafruit_BME280 bme;                                // temp, hum, press
BH1750 lightMeter;                                  // lux
Adafruit_SI1145 uv = Adafruit_SI1145();             // uv

// Globals

int pm10;
int pm25;

// Status variables

boolean SI1145initStatus = false;
boolean BH1750initStatus = false;
boolean MCP9808initStatus = false;
boolean BME280initStatus = false;
boolean PMS3003initStatus = false;

// SI1145 functions

boolean InitialiseSI1145()
{
  if(!uv.begin()) 
  {
    Serial.println("SI1145 Error");
    return false;
  }
  else
  {
    Serial.println("SI1145 Initialised");
    return true;
  }
}

float GetUVindex()
{
  float index = uv.readUV();
  index /= 100;

  return index;
}

// BH1750 functions

boolean InitialiseBH1750()
{
  if(!lightMeter.begin(BH1750::ONE_TIME_LOW_RES_MODE)) 
  {
    Serial.println("BH1750 Error");
    return false;
  }
  else
  {
    Serial.println("BH1750 Initialised");
    return true;
  }
}

float GetLightBH1750()
{
  float light = lightMeter.readLightLevel();

  return light;
}

// MCP9808 functions

boolean InitialiseMPC9808(byte mode)
{
  if(!tempsensor.begin(MCP9808_ADDR)) 
  {
    Serial.println("MCP9808 Error");
    return false;
  }
  else
  {
    Serial.println("MCP9808 Initialised");
    tempsensor.setResolution(mode); // sets the resolution mode of reading, the modes are defined in the table bellow:
  // Mode Resolution SampleTime
  //  0    0.5°C       30 ms
  //  1    0.25°C      65 ms
  //  2    0.125°C     130 ms
  //  3    0.0625°C    250 ms
    return true;
  }
}

float GetTemperatureMCP9808()
{
  tempsensor.wake();   // wake up, ready to read!
  float temperature = tempsensor.readTempC();
  tempsensor.shutdown_wake(1); // shutdown MSP9808 - power consumption ~0.1 mikro Ampere, stops temperature sampling

  return temperature;
}

// BME280 functions

boolean InitialiseBME280()
{
  if (!bme.begin(BME280_ADDR, &Wire))
  //if (!bme.begin())  
  {
    Serial.println("BME280 Error");
    return false;
  }
  else
  {
    Serial.println("BME280 Initialised");
    return true;
  }
  
}

// Not used
/* float GetTemperatureBME280()
{
  float temperature = bme.readTemperature();
  
  return temperature;
} */

float GetHumidityBME280()
{
  float humidity = bme.readHumidity();
  
  return humidity;
}

float GetPressureBME280()
{
  float pressure = bme.readPressure();
  pressure = bme.seaLevelForAltitude(ALTITUDE, pressure);
  pressure = pressure / 100.0F;
  
  return pressure;
}

// PMS3003 functions

void GetPollution()
{
  int tempPM10 = 0;
  int tempPM25 = 0;

  digitalWrite(PMS_SET, HIGH);
  delay(200);
  
  for(int i = 0; i < MEASURE_NUMBER; i ++)
  {
    int timeout = 0;

    do
    {
      pms.read();
      timeout ++;
    } while (!pms && timeout < 20);

    tempPM25 += pms.pm25;
    tempPM10 += pms.pm10;
  }
  
  if(pms)
  {  // successfull read
    pm25 = tempPM25 / MEASURE_NUMBER;
    pm10 = tempPM10 / MEASURE_NUMBER;

    PMS3003initStatus = true;
  } 
  else 
  { // something went wrong
    pm25 = -100;
    pm10 = -100;

    switch (pms.status) 
    {
        case pms.OK: // should never come here
          break;     // included to compile without warnings
        case pms.ERROR_TIMEOUT:
          Serial.println(F(PMS_ERROR_TIMEOUT));
          break;
        case pms.ERROR_MSG_UNKNOWN:
          Serial.println(F(PMS_ERROR_MSG_UNKNOWN));
          break;
        case pms.ERROR_MSG_HEADER:
          Serial.println(F(PMS_ERROR_MSG_HEADER));
          break;
        case pms.ERROR_MSG_BODY:
          Serial.println(F(PMS_ERROR_MSG_BODY));
          break;
        case pms.ERROR_MSG_START:
          Serial.println(F(PMS_ERROR_MSG_START));
          break;
        case pms.ERROR_MSG_LENGTH:
          Serial.println(F(PMS_ERROR_MSG_LENGTH));
          break;
        case pms.ERROR_MSG_CKSUM:
          Serial.println(F(PMS_ERROR_MSG_CKSUM));
          break;
        case pms.ERROR_PMS_TYPE:
          Serial.println(F(PMS_ERROR_PMS_TYPE));
          break;
    }
  
    PMS3003initStatus = false;    
  }

  digitalWrite(PMS_SET, LOW);
}

// Read battery voltage

float GetVoltage(int pinNumber, float divider)
{
  float voltage = (analogRead(pinNumber) + analogRead(pinNumber)) / 2;
  voltage = (voltage / 1023) * 1.1 / divider;

  return voltage;
}

// Sending message

void SendCommand(String command)
{
  digitalWrite(SET_PIN, LOW);
  delay(100);
  HC12.print(command);
  delay(100);
  while (HC12.available()) 
  {           // If HC-12 has data (the AT Command response)
    Serial.write(HC12.read());         // Send the data to Serial monitor
  }
  digitalWrite(SET_PIN, HIGH);
  delay(100);
}

// Make JSON message

String ComposeJSONmessage(int id, float temp, float hum, float press, float lux, float uv, int pm_25, int pm_10, float bat, float panel)
{
  String message;

  const size_t capacity = JSON_OBJECT_SIZE(10) + 90;
  DynamicJsonDocument doc(capacity);

  doc["id"] = id;

  if(MCP9808initStatus)
    doc["te"] = roundf(temp * 100) / 100.0;
  if(BME280initStatus)
  {
    doc["hu"] = roundf(hum * 10) / 10.0;
    doc["pr"] = roundf(press * 10) / 10.0;
  }
  if(BH1750initStatus)
    doc["li"] = roundf(lux);
  if(SI1145initStatus)
    doc["uv"] = roundf(uv * 100) / 100.0;
  if(PMS3003initStatus)
  {
    doc["p2"] = pm_25;
    doc["p1"] = pm_10;
  }
  doc["ba"] = roundf(bat * 100) / 100.0;
  doc["pa"] = roundf(panel * 100) / 100.0;

  serializeJson(doc,message);

  return message;
}

// Check maintenance mode

unsigned int CheckMode(unsigned int pin)
{
  unsigned int time;

  if(digitalRead(pin) == HIGH)
    time = 1;
  else
    time = SLEEP_TIME;
  
  return time;
}

void setup() 
{
  Serial.begin(9600);
  HC12.begin(9600);

  Wire.begin();

  // Initialise sensors and report status
  MCP9808initStatus = InitialiseMPC9808(2);
  BME280initStatus =  InitialiseBME280();
  BH1750initStatus = InitialiseBH1750();
  SI1145initStatus = InitialiseSI1145();

  // Pin mode setup
  pinMode(CONFIG_PIN, INPUT_PULLUP);
  pinMode(BATTERY_PIN, INPUT);
  pinMode(PANEL_PIN, INPUT);
  pinMode(PMS_SET, OUTPUT);
  pinMode(SET_PIN, OUTPUT);

  analogReference(INTERNAL);

  // Initialise PMS
  digitalWrite(PMS_SET, HIGH);
  delay(100);
  pms.init();
  digitalWrite(PMS_SET, LOW);
}

void loop() 
{ 
  float tempOut = 0;
  float humOut = 0;
  float pressOut = 0;
  float lightOut = 0;
  float UVindex = 0;

  // Get measurements only if device is initialised
  if(MCP9808initStatus)
    tempOut = GetTemperatureMCP9808();
  if(BME280initStatus)
  {
    humOut = GetHumidityBME280();
    pressOut = GetPressureBME280();
  }
  if(BH1750initStatus)
    lightOut = GetLightBH1750();
  if(SI1145initStatus)
    UVindex = GetUVindex();
  if(tempOut > - 10.0)  // Measure pollution only if temperature is above -10 C
    GetPollution();
  else
    PMS3003initStatus = false;

  float batteryVoltage = GetVoltage(BATTERY_PIN, BATTERY_DIVIDER);
  delay(50);
  float panelVoltage = GetVoltage(PANEL_PIN, PANEL_DIVIDER);
  
  String messToSend = ComposeJSONmessage(NODE_ID, tempOut, humOut, pressOut, lightOut, UVindex, pm25, pm10, batteryVoltage, panelVoltage);
  SendCommand("AT");
  Serial.println(messToSend);
  HC12.println(messToSend);      // Send that data to HC-12
  delay(50);
  SendCommand("AT+SLEEP");
  
  int counter = CheckMode(CONFIG_PIN);

  for(int i = 0; i < counter; i ++)   // sleep time depends on config pin
    LowPower.powerDown(SLEEP_8S, ADC_ON, BOD_OFF);
  delay(50);
}