
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WiFiMulti.h>
#include <ArduinoJson.h>
#include <Ticker.h>
Ticker tickerOSWatch;

#define WPASS       ""
#define WDtimeout 30000

ESP8266WiFiMulti WiFiMulti;
boolean ssid = false;
String resp = "";
int bloop = 1000; //ms of loop
byte bstatus = 0; //0 Not conected 1- Connected
int httpcode;
int cerror = 0; // Error counter
unsigned long wdt;


double solax_pv1c ; //corriente string 1
double solax_pv2c ; //corriente string 2
int16_t solax_pv1v ; //tension string 1
int16_t solax_pv2v ; //tension string 2
int16_t solax_pw1 ; //potencia string 1
int16_t solax_pw2 ; //potencia string 2
int16_t solax_gridv ; // tension de red
double solax_wsolar ; // Potencia solar
double solax_wtoday ; // Potencia solar diaria
double solax_wgrid ; // Potencia de red (Negativo: de red - Positivo: a red)
double solax_wtogrid ; // Potencia enviada a red



void parseJson(String json) {
  DynamicJsonBuffer  jsonBuffer;
  JsonObject& root = jsonBuffer.parseObject(json);
  if (!root.success()) {
    Serial.println("###JSONERROR");
    cerror++;
    return;
  } else  { 
    solax_pv1c = root["Data"][0]; //corriente string 1
    solax_pv2c = root["Data"][1]; //corriente string 2
    solax_pv1v = root["Data"][2]; //tension string 1
    solax_pv2v = root["Data"][3]; //tension string 2
    solax_gridv = root["Data"][5]; // tension de red
    solax_wsolar = root["Data"][6]; // Potencia solar
    solax_wtoday = root["Data"][8]; // Potencia solar diaria
    solax_wgrid = root["Data"][10]; // Potencia de red (Negativo: de red - Positivo: a red)
    solax_pw1 = root["Data"][11]; //potencia string 1
    solax_pw2 = root["Data"][12]; //potencia string 2
    solax_wtogrid = root["Data"][41]; // Potencia diaria enviada a red
    cerror = 0;
    resp = "###PAYLOAD:{\"Data\":[" + String(httpcode) + "," +String(solax_pv1c) + "," + String(solax_pv2c) + "," + String(solax_pv1v) + "," + String(solax_pv2v) + "," + String(solax_pw1) + "," + String(solax_pw2) + "," + String(solax_gridv) + "," + String(solax_wsolar) + "," + String(solax_wtoday) + "," + String(solax_wgrid) + "," + String(solax_wtogrid) + "]}$$$";
    Serial.println(resp);
  }
}


void ICACHE_RAM_ATTR osWatch(void) {
 #define WDtimeout 30000
  unsigned long wdc = millis();
    if ((millis() - wdc) > 5000) {
      if ((millis() - wdt) > WDtimeout) {
        ESP.restart();
      }
      wdc = millis();
    }
}

void setup() {

    Serial.begin(115200);
    Serial.println("##D INIT VERSION M1 00002$");
    
    wdt = millis();
    tickerOSWatch.attach_ms((WDtimeout / 3), osWatch);
    
    WiFi.mode(WIFI_STA);
    connect_wifi();

}

void serial_com() {

     String currentLine;   
     while (Serial.available()) {
      currentLine = Serial.readStringUntil('\n');
      
      if (currentLine.startsWith("###SSID")){
        String buf;
        buf = currentLine.substring(currentLine.indexOf("###SSID=")+8,currentLine.indexOf("$$$") );
        char bufferdata[buf.length()+1];
        buf.toCharArray(bufferdata,(buf.length() +1));
        WiFiMulti.addAP(bufferdata);
        Serial.println("##D M1: Connecting to " + String(bufferdata));
        ssid = true;
      } else if (currentLine.startsWith("###VERSION")){
        Serial.println("###VERSION M1 00002$");
      } else if (currentLine.startsWith("###RESET")){
        Serial.println("##D RESETING M1");
        ESP.reset();
      } else if (currentLine.startsWith("###PAYLOAD")){
        resp = "###PAYLOAD:{\"Data\":[" + String(solax_pv1c) + "," + String(solax_pv2c) + "," + String(solax_pv1v) + "," + String(solax_pv2v) + "," + String(solax_pw1) + "," + String(solax_pw2) + "," + String(solax_gridv) + "," + String(solax_wsolar) + "," + String(solax_wtoday) + "," + String(solax_wgrid) + "," + String(solax_wtogrid) + "]}$$$";
        Serial.println(resp);
        resp = "";
      } else if (currentLine.startsWith("###STATUS")){
        Serial.println("###STATUS:"+String(bstatus)+"$$$");
      } else if (currentLine.startsWith("###CERROR")){
        Serial.println("###CERROR:"+String(cerror)+"$$$");
      } else if (currentLine.startsWith("###HTTPCODE")){
        Serial.println("###HTTPCODE:"+String(httpcode)+"$$$");
      
     }
   }
}

void connect_wifi() {
  Serial.println("###STATUS:"+String(bstatus)+"$$$");
  uint8_t cont = 100;
  while(WiFiMulti.run() != WL_CONNECTED && cont-- > 0) {
  //if (WiFiMulti.run() != WL_CONNECTED) {
     serial_com();
     if (ssid) {
        if (WiFiMulti.run()  == WL_CONNECTED) {
            Serial.println("##DIP : " + WiFi.localIP());
            Serial.println("##DDONE CONNECT");
            bstatus=1;
        }
        Serial.print(" .");
     }
     //Serial.print(" |");
 delay(50);
 }

}

void loop() {

    wdt = millis();
    serial_com();
    if (WiFiMulti.run() == WL_CONNECTED) {
	   bstatus = 1;
	   if (bloop <= 0) {
        HTTPClient http;

        http.begin("http://5.8.8.8/?optType=ReadRealTimeData"); 
        http.addHeader("Host", "5.8.8.8");
        http.addHeader("Content-Length", "0");
        http.addHeader("Accept", "/*/");
        http.addHeader("Content-Type", "application/x-www-form-urlencoded");
        http.addHeader("X-Requested-With", "com.solaxcloud.starter");

        httpcode = http.POST("");

        if(httpcode > 0) {
            if(httpcode == HTTP_CODE_OK) {
                resp = http.getString();
                parseJson(resp);
                delay(0);
            }
        } else {
            cerror++;
        }

        http.end();
		  bloop = 1000;
	  }

    } else {

      Serial.println("##D M1: NOT CONNECT");
      bstatus = 0;
	    bloop=1500;
      cerror++;
      connect_wifi();

    }
    if (cerror >= 30) ESP.reset();
    delay(1);
	  bloop--;

}
