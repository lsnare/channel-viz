/*************************************************** 
  This is a sketch to use the CC3000 WiFi chip & Xively
  
  Written by Marco Schwartz for Open Home Automation
 ****************************************************/

// Libraries
#include <Adafruit_CC3000.h>
#include <ccspi.h>
#include <SPI.h>
#include <string.h>
#include "utility/debug.h"
#include "DHT.h"
#include "Wire.h"
#include "Adafruit_BMP085.h"

int flag = 0;
float hPa = 0;

// Create CC3000 instances
Adafruit_CC3000 cc3000 = Adafruit_CC3000(10, 3, 5, SPI_CLOCK_DIV2); // you can change this clock speed
                                  
// DHT instance
DHT dht(7, DHT22);

//BMP180 presure
Adafruit_BMP085 bmp;

uint32_t ip;

//NOAA method for calculating dew point
double dewPoint(double celsius, double humidity)
{
	// (1) Saturation Vapor Pressure = ESGG(T)
	double RATIO = 373.15 / (273.15 + celsius);
	double RHS = -7.90298 * (RATIO - 1);
	RHS += 5.02808 * log10(RATIO);
	RHS += -1.3816e-7 * (pow(10, (11.344 * (1 - 1/RATIO ))) - 1) ;
	RHS += 8.1328e-3 * (pow(10, (-3.49149 * (RATIO - 1))) - 1) ;
	RHS += log10(1013.246);

        // factor -3 is to adjust units - Vapor Pressure SVP * humidity
	double VP = pow(10, RHS - 3) * humidity;

        // (2) DEWPOINT = F(Vapor Pressure)
	double T = log(VP/0.61078);   // temp var
	return (241.88 * T) / (17.558 - T);
}

void setup(void)
{
  // Initialize
  Serial.begin(9600);
  
  Serial.println(F("\nInitializing..."));
  if (!cc3000.begin())
  {
    Serial.println(F("Couldn't begin()! Check your wiring?"));
    while(1);
  }
  
  bmp.begin();
 
}

void loop(void)
{
  // Connect to WiFi network
  cc3000.connectToAP("Fred", "1123581321", WLAN_SEC_WPA2);
  Serial.println(F("Connected!"));
  
  /* Wait for DHCP to complete */
  Serial.println(F("Request DHCP"));
  while (!cc3000.checkDHCP())
  {
    delay(100);
  }  

  // Get the website IP & print it
  ip = 0;
  Serial.print("api.xively.com"); Serial.print(F(" -> "));
  while (ip == 0) {
    if (! cc3000.getHostByName("api.xively.com", &ip)) {
      Serial.println(F("Couldn't resolve!"));
    }
    delay(500);
  }
  cc3000.printIPdotsRev(ip);
  
  // Get data & transform to integers

  double h = dht.readHumidity();
  double t = dht.readTemperature();
  double d = dewPoint(t,h);
  
  
  // Prepare JSON for Xively & get length
  int length = 0;

  String data = "";
  
  switch (flag) {
    case 0:
      data = data+"\n" + "{\"version\":\"1.0.0\",\"datastreams\" : [ {\"id\" : \"Temperature\",\"current_value\" : \"" + String((int)t) + "\"}]}";
      flag++;
      break;
    case 1:
      data = data+"\n" + "{\"version\":\"1.0.0\",\"datastreams\" : [ {\"id\" : \"Humidity\",\"current_value\" : \"" + String((int)h) + "\"}]}";
      flag++;
      break;
    case 2:
      data = data+"\n" + "{\"version\":\"1.0.0\",\"datastreams\" : [ {\"id\" : \"DewPoint\",\"current_value\" : \"" + String((int)d) + "\"}]}"; 
      flag++;
      break;
    case 3:
      data = data +"\n" + "{\"version\":\"1.0.0\",\"datastreams\" : [ {\"id\" : \"Pressure\",\"current_value\" : \"" + String((int)(0.01*bmp.readPressure())) + "\"}]}";
      flag++;
      break;
    case 4:
      data = data +"\n" + "{\"version\":\"1.0.0\",\"datastreams\" : [ {\"id\" : \"LCL\",\"current_value\" : \"" + String((int)((20+(t/5))*(100-h))) + "\"}]}";
      flag = 0;
      break;
  }
   
   length = data.length();
   Serial.println(data);
  // Send request
  Adafruit_CC3000_Client client = cc3000.connectTCP(ip, 80);
  if (client.connected()) {
    Serial.println("Connected!");
    client.println("PUT /v2/feeds/1855457926.json HTTP/1.0");
    client.println("Host: api.xively.com");
    client.println("X-ApiKey: cVFKWYvLsT0X2lQdUoHhveHtJpwLcbARpm6AAF99KRuiOyQl");
    client.println("Content-Length: " + String(length));
    client.print("Connection: close");
    client.println();
    client.print(data);
    client.println();
  } else {
    Serial.println(F("Connection failed"));    
    return;
  }
  
  Serial.println(F("-------------------------------------"));
  //the getHostByName function helps resolve the issue of becoming stuck in
  //an infinite loop after being disconnected from the internet
  while (client.connected() && cc3000.getHostByName("api.xively.com", &ip)) {
    while (client.available()) {
      char c = client.read();
      Serial.print(c);
    }
  }
  client.close();
  Serial.println(F("-------------------------------------"));
  
  Serial.println(F("\n\nDisconnecting"));
  cc3000.disconnect();
  
  // Wait 10 seconds until next update
  delay(10000);
}
