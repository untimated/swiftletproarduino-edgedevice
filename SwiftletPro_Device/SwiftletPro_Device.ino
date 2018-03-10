#include <Arduino.h>

#include <ArduinoJson.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <EEPROM.h>

#define DHTPIN 4
#define DHTTYPE DHT11
#define EDGE_DISCOVERY_MESSAGE "Register Edge"
#define EDGE_DISCOVERY_NOTIF "Edge discovery finish"

/*E : Edge,
001 : 10^3 total devices,
H : Humidity*/
const char EDGE_ID[6] = "E001H";
const unsigned int LOCAL_UDP_PORT = 55057;
const unsigned int APP_PORT = 5037;
const unsigned int APP_PORT_2 = 5038;
const unsigned int BRIDGE_PORT = 55056;
bool edgeDiscoveryIsCompleted = true;
unsigned long previousMillis = 0;
const long interval = 100000;

IPAddress ipmulti(255,255,255,255);
WiFiUDP Udp;
DHT dht(DHTPIN, DHTTYPE);

void setup()
{
  Serial.begin(115200);
  Serial.println();
  WiFi.begin("Michael's Open Network", "");
  Serial.print("Connecting");
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("Connected to : ");
  Serial.println(WiFi.SSID());
  Serial.print("MAC: ");
  Serial.println(WiFi.macAddress());
  Serial.print("Connected, Local IP address: ");
  Serial.println(WiFi.localIP());
  Udp.begin(LOCAL_UDP_PORT);
  dht.begin();
  clearEEPROM();
  Serial.print("Begin Listening To App ");
}

void loop() {
    char incomingPacket[255];
    char replyPacket[255];
    //Listening
    int packetSize = Udp.parsePacket();
    if (packetSize){
      Serial.printf("Received %d bytes from %s, port %d\n", packetSize, Udp.remoteIP().toString().c_str(), Udp.remotePort());
      int len = Udp.read(incomingPacket, 255);
      if (len > 0)  {incomingPacket[len] = 0;}
      Serial.printf("UDP packet contents: %s\n", incomingPacket);
      if(strcmp(incomingPacket,"Edge discovery finish")==0){edgeDiscoveryIsCompleted = true;};//Read discovery finish flag, and unblock this device uploads
      if((Udp.remotePort() == BRIDGE_PORT) && (strcmp(incomingPacket,EDGE_DISCOVERY_MESSAGE)==0)){
        edgeDiscoveryIsCompleted = false;//Block this device to upload sensor data to bridge
        clearEEPROM();
        writeBridgeIP(Udp.remoteIP());
        Udp.beginPacket(Udp.remoteIP(), Udp.remotePort());
        StaticJsonBuffer<400> jsonBuffer;
        JsonObject& successJson = jsonBuffer.createObject();
        successJson["request"] = "register";
        successJson["serial"] = EDGE_ID;
        successJson["ip"] = WiFi.localIP().toString();
        successJson["name"] = "Swiftlet-Humidity-Pro-Edge";
        successJson.printTo(replyPacket,sizeof(replyPacket));
        successJson.printTo(Serial);
        Udp.write(replyPacket);
        Udp.endPacket();
        Serial.println("\nPacket Sent");
      }
    }else{
      if(edgeDiscoveryIsCompleted && sendFlag()){
        //Send Humidity Data
        //delay(2000);
        float h = dht.readHumidity();
        float t = dht.readTemperature();
        float hic = dht.computeHeatIndex(t, h, false);
        if (isnan(h) || isnan(t)) {
          Serial.println("Failed to read from DHT sensor!");
          return;
        }
        Serial.print("Bridge IP  : ");Serial.println(toIPAddress(readBridgeIP()).toString());
        Udp.beginPacket(toIPAddress(readBridgeIP()), BRIDGE_PORT);
        StaticJsonBuffer<400> jsonBuffer;
        JsonObject& successJson = jsonBuffer.createObject();
        JsonObject& successJsonData = successJson.createNestedObject("data");
        successJson["request"] = "report";
        successJson["serial"] = EDGE_ID;
        successJson["ip"] = WiFi.localIP().toString();
        successJson["name"] = "Swiftlet-Humidity-Pro-Edge";
        successJsonData["humidity"] = h;
        successJsonData["temperature"] = t;
        successJson.printTo(replyPacket,sizeof(replyPacket));
        successJson.printTo(Serial);
        Udp.write(replyPacket);
        Udp.endPacket();
      }
    }
    //Udp.flush();
}

String readBridgeIP(){
  EEPROM.begin(15);
  char buffer[15];
  for(int j=0;j<15;j++){
    buffer[j] = EEPROM.read(j);
  }
  EEPROM.end();
  return String(buffer);
}

IPAddress toIPAddress(String ip){
  IPAddress buffer;
  buffer.fromString(ip);
  return buffer;
}

void writeBridgeIP(IPAddress bridgeIP){
  String bip = bridgeIP.toString();
  char ip[15]={'\0'}; bip.toCharArray(ip, 15);
  EEPROM.begin(15);
  Serial.println("Writing to memory...");
  for(int k=0; k<15;k++){
    if(ip[k]==NULL){
      EEPROM.write(k, '\0');Serial.print('\0');
    }else{
      EEPROM.write(k, ip[k]);Serial.print(ip[k]);Serial.print(" ");
    }
  }
  Serial.println("Finish writting to memory");
  EEPROM.end();
}

void clearEEPROM(){
  EEPROM.begin(15);
  for(int j=0;j<15;j++){
    EEPROM.write(j, 0);
  }
  Serial.println("Clearing EEPROM Cache");
  EEPROM.end();
}

bool sendFlag(){
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    return true;
  }
  return false;
}
