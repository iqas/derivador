/**
 * Derivador de excedentes para ESP32 DEV Kit // Wifi Kit 32
 * (OLED = TRUE for Wifi Kit 32, false for ESP32)
 *
 *  Created on: 2018 by iqas
 *
 */

//#include <Arduino.h>
#include <ArduinoOTA.h>
#include <Wire.h> 
#include <WiFi.h>
#include <WiFiMulti.h>
#include <WiFiClient.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <PubSubClient.h>// to use MQTT protocol
#include "EEPROM.h"


#define OLED true
// !!!!Turn to true when oled screen is available!!!! --> WIFI KIT 32

#if OLED
#include <SSD1306Wire.h>                                                              
#endif

#if OLED 
SSD1306Wire display(0x3c, 4, 15); //For OLED
#endif

#define true 0x1
#define false 0x0
#define USE_SERIAL Serial

// Set up the pwm output
#if OLED
uint8_t pin_pwm = 25;
uint8_t pin_rx = 17;
uint8_t pin_tx = 5;
#else
uint8_t pin_pwm = 2;
uint8_t pin_rx = 17;
uint8_t pin_tx = 5;

#endif
uint16_t solax_pwm = 0;          // a value from 0 to 65535 representing the pwm value
char solax_pwm_active = true;
String espResp;


// Set up relay output
#define PIN_RL1 21
#define PIN_RL2 5
#define PIN_RL3 19
#define PIN_RL4 21

// END USER CONFIG

//  MQTT TOPICS:
// /solax/pwm -> PWM Value
// /solax/relay/1 -> Relay 1
// /solax/relay/2 -> Relay 2
// /solax/relay/3 -> Relay 3
// /solax/relay/4 -> Relay 4
// solax/pv1c -> corriente string 1
// solax/pv2c -> corriente string 2
// solax/pv1v -> tension string 1
// solax/pv2v -> tension string 2
// solax/pw1 -> potencia string 1
// solax/pw2 -> potencia string 2
// solax/gridv ->  tension de red
// solax/wsolar ->  Potencia solar
// solax/wtoday ->  Potencia solar diaria
// solax/wgrid ->  Potencia de red (Negativo: de red - Positivo: a red)
// solax/wtogrid -> Potencia enviada a red

// START CODE
WiFiMulti wifiMulti;
HardwareSerial SerieEsp(2); // RX, TX for esp-01

uint8_t nerror = 0; // Nº de error
boolean werror = false; // Error en conexión Wifi
boolean serror = false; // Error conexiín Solax
boolean esp01 = true;

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

// EEPROM Data
struct CONFIG {
  byte eeinit;
  boolean autoOta;
  boolean mqtt;
  char MQTT_broker[25];
  char MQTT_user[20];
  char MQTT_password[20];
  int  MQTT_port;
  char login1[25];
  char pass1[25];
  char login2[25];
  char pass2[25];
  byte wversion;
  char ssid_esp01[25];
  char password_esp01[20];
  char solax_ip_v1[18];
  boolean P01_on; //To implement
  int pwm_min; 
  int pwm_max;
  boolean R01_on; //Relay 1 control output
  int R01_min;
  int R01_max;
  boolean R02_on; //Relay 2 control output
  int R02_min;
  int R02_max;
  boolean R03_on; //Relay 3 control output
  int R03_min;
  int R03_max;
  boolean R04_on; //Relay 4 control output
  int R04_min;
  int R04_max;
};

CONFIG dsConfig;

WiFiClient espClient;
WiFiServer server(80);
PubSubClient client(espClient);

/******** void ParseJson***********************
  this funcition receives json data from
   http request and decode and print it
 ***********************************************/

void parseJson(String json) {
  Serial.println("JSON:" + json);
  DynamicJsonBuffer  jsonBuffer;
  JsonObject& root = jsonBuffer.parseObject(json);
  if (!root.success()) {
    Serial.println("parseObject() failed");
    //publisher("this is not a Json","JsonStatus");
    return;
  } else Serial.println("parseObject() OK");
  
    //publisher("Json ok ","JsonStatus");
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

    Serial.print("POWER:");
   Serial.println(solax_wgrid);
  
}


/******** void callback***********************
   this function receive the info from mqtt
   suscriptionand print the infro that comes
   through
**********************************************/

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("MQTT Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
  if (strcmp(topic,"/solax/pwm")==0) {  // pwm control ON-OFF
       Serial.println("Mqtt _ PWM control");
      if ((char)payload[0] == '1') {
        Serial.println("PWM ACTIVATED");
        solax_pwm_active = true; 
      } else {
        solax_pwm_active = false;
        Serial.println("DEACTIVATING PWM");
        while (solax_pwm > 0) {
          solax_pwm--;
          ledcWrite(1, solax_pwm);
        }
        Serial.println("PWM DEACTIVATED"); 
      }
  }
  if (strcmp(topic,"/solax/relay/1")==0) {
       Serial.print("Mqtt _ Relay 1");
      if ((char)payload[0] == '1') {
        digitalWrite(PIN_RL1, HIGH); 
        Serial.print("Payload recibido HIGH");  
      } else {
        digitalWrite(PIN_RL1, LOW);
        Serial.print("Payload recibido LOW"); 
      }
   }
  if (strcmp(topic,"/solax/relay/2")==0) {
       Serial.print("Mqtt _ Relay 2");
      if ((char)payload[0] == '1') {
        digitalWrite(PIN_RL2, HIGH); 
        Serial.print("Payload recibido HIGH");  
      } else {
        digitalWrite(PIN_RL2, LOW);
        Serial.print("Payload recibido LOW"); 
      }
   }
   if (strcmp(topic,"/solax/relay/3")==0) {
       Serial.print("Mqtt _ Relay 3");
      if ((char)payload[0] == '1') {
        digitalWrite(PIN_RL3, HIGH); 
        Serial.print("Payload recibido HIGH");  
      } else {
        digitalWrite(PIN_RL3, LOW);
        Serial.print("Payload recibido LOW"); 
      }
    }
    if (strcmp(topic,"/solax/relay/4")==0) {
       Serial.print("Mqtt _ Relay 4");
      if ((char)payload[0] == '1') {
        digitalWrite(PIN_RL4, HIGH); 
        Serial.print("Payload recibido HIGH");  
      } else {
        digitalWrite(PIN_RL4, LOW);
        Serial.print("Payload recibido LOW"); 
      }
    }   
  }




/******** void publisher***********************
    function that publish to the broker
    receives the message and topic
 *                                              *
 ***********************************************/
void publisher(char* topic, String topublish) {
  if (client.connect("SodeClient", dsConfig.MQTT_user, dsConfig.MQTT_password)) {
    //Serial.println("connected");
    // Once connected, publish an announcement...
    int length = topublish.length();
    char bufferdata[length];
    topublish.toCharArray(bufferdata,length+1);
    //client.publish("/solax/pwm", bufferdata);
    
    client.publish(topic, bufferdata);
  } else {
    Serial.print(F("failed, rc="));
    Serial.print(client.state());
    Serial.println(" try again in 5 seconds");
    // Wait 5 seconds before retrying
    delay(5000);
    reconnect();
  }
}


void reconnect() {
  // Loop until we're reconnected
  String mqtt_dir;
  if (!client.connected()) {
    Serial.print(F("Attempting MQTT connection..."));
    // Attempt to connect
    if (client.connect("SodeClient", dsConfig.MQTT_user, dsConfig.MQTT_password)) {
      Serial.println("connected");
      
      client.subscribe("solax/pv1c");
      client.subscribe("solax/pv2c");
      client.subscribe("solax/pv1v");
      client.subscribe("solax/pv2v");
      client.subscribe("solax/pw1");
      client.subscribe("solax/pw2");
      client.subscribe("solax/gridv");
      client.subscribe("solax/wsolar");
      client.subscribe("solax/wtoday");
      client.subscribe("solax/wgrid");
      client.subscribe("solax/wtogrid");
    } else {
      Serial.print(F("failed, rc="));
      //Serial.print(client.stat());
      client.disconnect();
      client.setServer(dsConfig.MQTT_broker, dsConfig.MQTT_port);
      client.setCallback(callback);
      Serial.println(" try again in 5 seconds");
      // Wait 1 second before retrying
      delay(1000);
    }
  }
}


//Reset the esp01 module
void espReset() {
  while (SerieEsp.available()) SerieEsp.readString();
  int cont = 0;
  Serial.println("");
  Serial.print("Searching ESP-01 ..");
  esp01 = false;
  while (cont < 5 && esp01 == false){
   SerieEsp.println("AT+RST");
   delay(1000);
   Serial.print(".");
   while (SerieEsp.available()) {
    espResp = SerieEsp.readStringUntil('\n');
    int pos=espResp.indexOf("OK");
    if(pos >=0 ) esp01 = true;
   }
 cont++; 
 } // End while  

  if (esp01 == true) {
    Serial.println("ESP01 Module found & reset.");
    dsConfig.wversion = 2;
  } else {
    Serial.println("ESP01 Module not Found!!!");
    Serial.println("Autoconfiguring for V1 Wifi version");
    dsConfig.wversion = 1;
  }

}

//Connect esp to your wifi network
void espConnectWifi() {
  dsConfig.wversion = 2;
  int count = 10;
  while(count >= 2 ) {  
    String cmd = "AT+CWJAP=\"" + String(dsConfig.ssid_esp01) +"\",\"" + String(dsConfig.password_esp01) + "\"";
    SerieEsp.println(cmd);
    delay(4000);
    while (SerieEsp.available()) {
      espResp = SerieEsp.readStringUntil('\n');
      int pos=espResp.indexOf("OK");
      if(pos >=0 ) {
        Serial.println("Module ESP-01 Connected!");
        count= 0;
        break;
      }
    }
    count--;
  }
  if (count == 0 ) {
    Serial.println("Module ESP-01 NOT Connected!"); 
    esp01 == false;
    Serial.println("Autoconfiguring for V1 Wifi version");
    dsConfig.wversion = 1;
  }
}

// Send httpost to ESP-01
int espHttppost () {

  int httpcode = 0;
  espResp = "";
  String server = "5.8.8.8";
  String uri = "/?optType=ReadRealTimeData";
  String data = "Message";
    
  SerieEsp.println("AT+CIPSTART=\"TCP\",\"" + server + "\",80");//start a TCP connection.
  if( SerieEsp.find("OK")) {
    USE_SERIAL.println("TCP connection ready");
  } 
  delay(1000);
  
  String postRequest =
  "POST " + uri + " HTTP/1.0\r\n" +
  "Host: " + server + "\r\n" +
  "Accept: *" + "/" + "*\r\n" +
  "Content-Length: " + data.length() + "\r\n" +
  "Content-Type: application/x-www-form-urlencoded\r\n" +
  "\r\n" + data;

  String sendCmd = "AT+CIPSEND=";//determine the number of caracters to be sent.
  SerieEsp.print(sendCmd);
  SerieEsp.println(postRequest.length() );
  delay(500);

  if(SerieEsp.find(">")) { USE_SERIAL.println("Sending.."); SerieEsp.print(postRequest);

   if( SerieEsp.find("SEND OK")) { 
    USE_SERIAL.println("Packet sent");
    httpcode = HTTP_CODE_OK;
   } else {
    serror = true;
    USE_SERIAL.println("Error sending POST to Solax");
   }

   while (SerieEsp.available()) {
    espResp = SerieEsp.readString();
   Serial.println("RECIBIDO: "+espResp);
   espResp = espResp.substring(espResp.indexOf("{"), espResp.indexOf("}")+1); 
   }
  }
// close the connection
SerieEsp.println("AT+CIPCLOSE");

return httpcode;
}


void setup() {

     // OLED
     #if OLED
      pinMode(25,OUTPUT);
      pinMode(16,OUTPUT);
      digitalWrite(16, LOW);    // set GPIO16 low to reset OLED
      delay(50);
      digitalWrite(16, HIGH); // while OLED is running, must set GPIO16 in high

      display.init();
      display.flipScreenVertically();
      display.setFont(ArialMT_Plain_10);
     #endif
    // SERIAL

    USE_SERIAL.begin(115200);

    USE_SERIAL.println();
    USE_SERIAL.println();
    USE_SERIAL.println();

    USE_SERIAL.println("Welcome to openDS+");
    for(uint8_t t = 4; t > 0; t--) {
        USE_SERIAL.printf("[SETUP] WAIT %d...\n", t);
        USE_SERIAL.flush();
        delay(1000);
    }

    // Read config
    USE_SERIAL.println("\nTesting EEPROM Library\n");
    if (!EEPROM.begin(sizeof(dsConfig))) {
      Serial.println("Failed to initialise EEPROM");
      Serial.println("Restarting...");
      delay(1000);
      ESP.restart();
    }
    EEPROM.get(0, dsConfig);
    if (dsConfig.eeinit != 00 ) {
      USE_SERIAL.println("Configuration Not Found, initializing");
      dsConfig.eeinit = 00;
      dsConfig.wversion = 2;
      dsConfig.autoOta = false;
      dsConfig.mqtt = false;
      strcpy(dsConfig.MQTT_broker, "192.168.0.2");
      strcpy(dsConfig.MQTT_user, "MQTT_user");
      strcpy(dsConfig.MQTT_password, "MQTT_password");
      dsConfig.MQTT_port = 1883;
      strcpy(dsConfig.login1, "MIWIFI1");
      strcpy(dsConfig.login2, "MIWIFI2");
      strcpy(dsConfig.pass1, "DSPLUSWIFI1");
      strcpy(dsConfig.pass2, "DSPLUSWIFI2");
      strcpy(dsConfig.ssid_esp01, "SOLAX");
      strcpy(dsConfig.password_esp01, "");
      strcpy(dsConfig.solax_ip_v1, "192.168.0.100");
      dsConfig.pwm_min = -60;
      dsConfig.pwm_max = -90;
      dsConfig.P01_on = false;
      dsConfig.R01_on = false;
      dsConfig.R01_min = 9999;
      dsConfig.R01_max = 9999;
      dsConfig.R02_on = false;
      dsConfig.R02_min = 9999;
      dsConfig.R02_max = 9999;
      dsConfig.R03_on = false;
      dsConfig.R03_min = 9999;
      dsConfig.R03_max = 9999;
      dsConfig.R04_on = false;
      dsConfig.R04_min = 9999;
      dsConfig.R04_max = 9999;
      EEPROM.put(0, dsConfig);
      EEPROM.commit();
    }
    
    
    #if OLED   
      display.drawString(0, 0, "Connecting...");
      display.display();
      delay(1000);
      display.clear();
    #endif

    // WIFI 
    Serial.print("Conecting to Wifi.");
    WiFi.mode(WIFI_AP_STA);
    
    wifiMulti.addAP(dsConfig.login1, dsConfig.pass1);
    wifiMulti.addAP(dsConfig.login2, dsConfig.pass2);

    Serial.print("\r\nWIFI:");
    int count = 10;
    while (wifiMulti.run() != WL_CONNECTED && count-- > 0) {
        delay(500);
        Serial.print(".");
    }
              
    if(wifiMulti.run() == WL_CONNECTED) {
     #if OLED
      display.setTextAlignment(TEXT_ALIGN_CENTER);
      display.clear();
      display.drawString(64, 0,"CONNECTED:");
      display.drawString(64, 14,"IP ADDRESS :");
      display.drawString(64, 28, WiFi.localIP().toString());
     #endif
     Serial.println("");
      USE_SERIAL.println("WiFi connected");
      USE_SERIAL.println("IP address: ");
      USE_SERIAL.println(WiFi.localIP());
      #if OLED
        display.display();
      #endif
     
    } else {
      werror = true;
      #if OLED
      display.setTextAlignment(TEXT_ALIGN_CENTER);
      display.clear();
      display.drawString(64, 0,"NOT CONNECTED:");
      display.drawString(64, 14,"WIFI ERROR");
      display.display();
     #endif
      USE_SERIAL.println("AP Not valid, SmartConfig actived, please run the app ");
      delay(5000);
      #if OLED
      display.setTextAlignment(TEXT_ALIGN_CENTER);
      display.clear();
      display.drawString(64, 0,"WAITING FOR");
      display.drawString(64, 14,"SMARTCONFIG");
      display.display();
     #endif
      //WiFi.mode(WIFI_AP_STA);
      WiFi.beginSmartConfig();
      Serial.println("Waiting for SmartConfig.");
      while (!WiFi.smartConfigDone()) {
          delay(500);
      Serial.print(".");
      }
      delay(2000);
      USE_SERIAL.println("WiFi connected");
      USE_SERIAL.println("IP address: ");
      USE_SERIAL.println(WiFi.localIP());

      #if OLED
      display.clear();
      display.drawString(64, 0,"CONNECTED:");
      display.drawString(64, 14,"IP ADDRESS :");
      display.drawString(64, 28, WiFi.localIP().toString());
      display.display();
      #endif
      delay(2000);
   
      // SAVE WIFI DATA TO EPROM
      {
      String buf = WiFi.SSID();
      char bufferdata[buf.length()];
      buf.toCharArray(bufferdata,(buf.length() +1));
      strcpy(dsConfig.login1,bufferdata );
      }
      {
      String buf = WiFi.psk();
      char bufferdata[buf.length()];
      buf.toCharArray(bufferdata,(buf.length() +1));
      strcpy(dsConfig.pass1,bufferdata );
      }
      EEPROM.put(0, dsConfig);
      EEPROM.commit(); 
      Serial.println ("WIFI DATA SAVED!!!!");
      werror = false;
    }
    
    // WIFI ESP-01
    // initialize serial
    SerieEsp.begin(115200, SERIAL_8N1, pin_rx, pin_tx);
    // initialize ESP-01 module
    delay(1000);
    espReset();
    if (esp01 == true) espConnectWifi(); // connect esp01 to inverter
           
    // PWM
    
    // Initialize channels 
    // channels 0-15, resolution 1-16 bits, freq limits depend on resolution
    ledcSetup(0, 1000, 16); // 1 kHz PWM, 8-bit resolution
    ledcAttachPin(pin_pwm, 0); // assign pins to chanel
    ledcWrite(0, solax_pwm);  // Write new pwm value

    if (dsConfig.mqtt == true ) {
      client.setServer(dsConfig.MQTT_broker, dsConfig.MQTT_port);
      client.setCallback(callback);
    }
    // http server
    server.begin();
}

uint16_t pwm_calc(uint16_t pwm_val) {
  // Calc value with sin and cos
  double value;
  USE_SERIAL.printf("PWM_VAL: %d\n",pwm_val);
  if (pwm_val >= 90) {
    value = (( (1 + cos((180-pwm_val)*3.1416/180))) /2 ) * 65535 ;
    USE_SERIAL.printf("SIN: %f COS: %f VAL: %f\n", sin(90*3.1416/180),cos((180-pwm_val)*3.1416/180), value );
  } else {
    value = (( sin(pwm_val*3.1416/180) /2 ) * 65535) ;
    USE_SERIAL.printf("SIN: %f VAL: %d\n", sin(pwm_val*3.1416/180), (uint16_t)value );
  }
  if (pwm_val == 180) value = 65535;
  return ((uint16_t)value);
}

void loop() {

    int httpcode;
    // wait for WiFi connection
    if((wifiMulti.run() == WL_CONNECTED) ) { 
        
        if (dsConfig.mqtt == true ) {
          if (!client.connected()) {
            reconnect();
            if (!client.connected()) {
              USE_SERIAL.print("Reconnecting MQTT\n");
              #if OLED 
              display.setTextAlignment(TEXT_ALIGN_CENTER);
              display.drawString(64, 38,"MQTT: Conecting...");
              display.display();
              #endif
              reconnect();
              if (!client.connected()) { 
                dsConfig.mqtt = false;
                USE_SERIAL.print("\r\nMQTT service not work!!!!\n\r");
              }
            }
          }
          else client.loop();
        }

        HTTPClient http;
        if (dsConfig.wversion == 1) {
          int tmp_error=2;
          httpcode = -1;
          while (tmp_error-- >0 && httpcode != HTTP_CODE_OK) {
            // configure traged server and url
            char buffer[50];
            sprintf(buffer, "http://%s/api/realTimeData.htm",dsConfig.solax_ip_v1);
            USE_SERIAL.println("BUFFER................................." + (String)buffer);
            http.begin(buffer); //HTTP
            // start connection and send HTTP header
            httpcode = http.GET();

            // httpCode will be negative on error
          }

        }

        if (dsConfig.wversion == 2) {

          USE_SERIAL.println();
          USE_SERIAL.println("Starting connection to ESP-01...");
          
          if (esp01 == true) {
            int tmp_error=5;
            httpcode = -1;
            while (tmp_error-- >0 && httpcode != HTTP_CODE_OK) {
              httpcode = espHttppost();
            }
          } else USE_SERIAL.println("Please connect a ESP-01 module !!!");

        }
        if (httpcode < 0 || httpcode == 404) serror = true; // Error in connection with Solax
        
        if(httpcode > 0) {
            // HTTP header has been send and Server response header has been handled
            USE_SERIAL.printf("[HTTP] code: %d\n", httpcode);

            // file found at server
            if(httpcode == HTTP_CODE_OK) {
                serror==false;
                if (esp01 == false) espResp = http.getString();
                
                parseJson(espResp);
                
                // Check pwm_output
                if (solax_pwm_active == true) {  
                  if ( solax_wgrid > dsConfig.pwm_min ) { // Default value is < 60 w from grid an pwm < 100
                    USE_SERIAL.println("PWM: SUBIENDO POTENCIA");
                    if  ( solax_wgrid > (dsConfig.pwm_max+250) && solax_pwm < 165 ) solax_pwm+= 15; // Increment 15 if the grid to power > 250w
                    if (solax_pwm >= 180) solax_pwm = 179; // 180 = 100%
                    solax_pwm += 1 ; //Increment value
                    ledcWrite(0, pwm_calc(solax_pwm));  // Write new pwm value
                    if (dsConfig.mqtt == true ) publisher("/solax/pwm",(String)solax_pwm);
                   }
                  
                  else if  ( solax_wgrid < dsConfig.pwm_max )  { // Value is < 90 w from grid an pwm < 125
                    USE_SERIAL.println("PWM: BAJANDO POTENCIA");
                    if  ( solax_wgrid < (dsConfig.pwm_max-250)  && solax_pwm > 15) solax_pwm-= 15; // Decrement 15 if the grid from power > 200w
                    if (solax_pwm <= 0) solax_pwm = 1;
                    solax_pwm-= 1 ; //Decrement value 
                    ledcWrite(0, pwm_calc(solax_pwm));  // Write new pwm value
                    if (dsConfig.mqtt == true ) publisher("/solax/pwm", (String)solax_pwm );
                    
                   }
                 }

                
                // Publish mqtt values
                if (dsConfig.mqtt == true ) {
                  publisher("/solax/pv1c", (String)solax_pv1c);
                  publisher("/solax/pv2c", (String)solax_pv2c);
                  publisher("/solax/pw1", (String)solax_pw1);
                  publisher("/solax/pw2", (String)solax_pw2);
                  publisher("/solax/pw1v", (String)solax_pv1v);
                  publisher("/solax/pw2v", (String)solax_pv2v);
                  publisher("/solax/gridv", (String)solax_gridv);
                  publisher("/solax/wsolar", (String)solax_wsolar);
                  publisher("/solax/wtoday", (String)solax_wtoday);
                  publisher("/solax/wgrid",  (String)solax_wgrid);
                  publisher("/solax/wtogrid", (String)solax_wtogrid);
                  publisher("/solax/wgrid",(String)solax_pwm);
                
                } 

                // Display values
                #if OLED 
                  display.clear();
                  display.setTextAlignment(TEXT_ALIGN_LEFT);
                  display.setFont(ArialMT_Plain_10);
                  display.drawString(0, 0, "Power Solar . Power Grid");
                  display.setFont(ArialMT_Plain_24);
                  display.drawString(0, 12, (String)(int)solax_wsolar);
                  display.drawString(64, 12, (String)(int)solax_wgrid);
                #endif

                // Check Relay control

                String pro = String((solax_pwm * 100) / 180) + "%";
                int progressbar = ((solax_pwm * 100) / 180);
                                
                
                USE_SERIAL.print("\r\nPWM VALUE: % ");
                USE_SERIAL.println(pro);
                USE_SERIAL.print("\r\nPWM VALUE: ");
                USE_SERIAL.println(solax_pwm);
                USE_SERIAL.println("\r\nPROGRESS BAR : ");
                USE_SERIAL.println(progressbar);
                
                #if OLED 
                  display.setFont(ArialMT_Plain_10);
                  display.drawProgressBar(0, 38, 120, 10, progressbar);    // draw the progress bar    
                  display.setTextAlignment(TEXT_ALIGN_CENTER);          // draw the percentage as String
                  display.drawString(64, 52,"PWM "+pro + " Relay: o o o o");
                  display.display();
                #endif
                
                
                
                
            }
        } else {
            USE_SERIAL.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpcode).c_str());
            #if OLED
              display.setTextAlignment(TEXT_ALIGN_CENTER);
              display.drawString(64, 52,"Error en red - 01");
              display.display();
            #endif
            
        }

        http.end();
     } // End If Wifi

     if (serror || werror){
      USE_SERIAL.println("### CONFIGURE SYSTEM PLEASE ###");
      #if OLED
      display.setTextAlignment(TEXT_ALIGN_CENTER);
      display.clear();
      display.drawString(64, 0,"CONNECT ERROR");
      display.drawString(64, 14,"SET UP ON IP");
      display.drawString(64, 28, WiFi.localIP().toString());
      display.display();
      
      #endif
     }
      
    // Http server

    for (int16_t s=0;s<=10000;s++) {
      WiFiClient hclient = server.available();
      if (hclient) {                             // if you get a client,
        USE_SERIAL.println("New Client.");           // print a message out the serial port
        String currentLine = "";                // make a String to hold incoming data from the client
        unsigned int cont = 0;
        unsigned int page = 0;
        unsigned int post=0;
        int post_length = 0;
        while (hclient.connected()) {            // loop while the client's connected
            if (hclient.available()) {             // if there's bytes to read from the client,
              char c = hclient.read();             // read a byte, then
              Serial.write(c);                    // print it out the serial monitor
                if (c == '\n') {                    // if the byte is a newline character

                  // if the current line is blank, you got two newline characters in a row.
                  // that's the end of the client HTTP request, so send a response:
                  Serial.println ("CURRENTLINE:" + currentLine);

                  if (currentLine.startsWith("Content-Length:")) {
                    String strtmp = currentLine.substring(currentLine.indexOf('Content-Length:')+1 );
                    strtmp.trim();
                    post_length=strtmp.toInt();
                  }
                  if (currentLine.startsWith("POST /")) {
                    post=1;
                  }
                  if (currentLine.startsWith("GET /?cnet")) {
                    page = 1;
                  }
                  if (currentLine.startsWith("GET /?config")) {
                    page = 2;
                  }
                  if (currentLine.startsWith("GET /?relay")) {
                    page = 3;
                  }
                  if (currentLine.length() == 0) {

                    //Serial.println("PAGINA PRINCIPAL ---------------------------------------------------------------------------------------------------------------------");

                    if (post!=0) {
                      currentLine = "";
                      while(post_length-- > 0)
                      {
                        char c = hclient.read();
                        currentLine += c;
                      }
                      post=0;
                      post_length=0;
                      
                      // MQTT CONF
                      if (currentLine.indexOf("mqttuser=") > 1) {            
                        {
                        String buf = currentLine.substring(currentLine.indexOf("tport=")+6,currentLine.indexOf("&Guardar") );
                        dsConfig.MQTT_port=buf.toInt();
                        }
                        {
                        String buf = currentLine.substring(currentLine.indexOf("roker=")+6,currentLine.indexOf("&mqttuser") );
                        char bufferdata[buf.length()];
                        buf.toCharArray(bufferdata,(buf.length() +1));
                        strcpy(dsConfig.MQTT_broker,bufferdata );
                        }
                        {
                        String buf = currentLine.substring(currentLine.indexOf("tuser=")+6,currentLine.indexOf("&mqttpass") );
                        char bufferdata[buf.length()];
                        buf.toCharArray(bufferdata,(buf.length() +1));
                        strcpy(dsConfig.MQTT_user,bufferdata );
                        }
                        {
                        String buf = currentLine.substring(currentLine.indexOf("tpass=")+6,currentLine.indexOf("&mqttport") );
                        char bufferdata[buf.length()];
                        buf.toCharArray(bufferdata,(buf.length() +1));
                        strcpy(dsConfig.MQTT_password,bufferdata );
                        }
                        if (currentLine.substring(currentLine.indexOf("ctive=")+6,currentLine.indexOf("&broker") ) == "Yes") {
                          dsConfig.mqtt = true;
                          if (!client.connected()) {
                            reconnect();
                          }
                        }
                        
                      }

                      // WIFI CONFIG POST
                      if (currentLine.indexOf("wifi1=") > 1) {
                        {
                        String buf = currentLine.substring(currentLine.indexOf("wifi1=")+6,currentLine.indexOf("&wifip1") );
                        char bufferdata[buf.length()];
                        buf.toCharArray(bufferdata,(buf.length() +1));
                        strcpy(dsConfig.login1,bufferdata );
                        }
                        {
                        String buf = currentLine.substring(currentLine.indexOf("wifi2=")+6,currentLine.indexOf("&wifip2") );
                        char bufferdata[buf.length()];
                        buf.toCharArray(bufferdata,(buf.length() +1));
                        strcpy(dsConfig.login2 ,bufferdata );
                        }
                        {
                        String buf = currentLine.substring(currentLine.indexOf("wifip1=")+7,currentLine.indexOf("&wifi2") );
                        char bufferdata[buf.length()];
                        buf.toCharArray(bufferdata,(buf.length() +1));
                        strcpy(dsConfig.pass1 ,bufferdata );
                        }
                        {
                        String buf = currentLine.substring(currentLine.indexOf("wifip2=")+7,currentLine.indexOf("&wifis") );
                        char bufferdata[buf.length()];
                        buf.toCharArray(bufferdata,(buf.length() +1));
                        strcpy(dsConfig.pass2 ,bufferdata );
                        }
                        {
                        String buf = currentLine.substring(currentLine.indexOf("wifis=")+6,currentLine.indexOf("&Guard") );
                         if (dsConfig.wversion == 2) {
                          char bufferdata[buf.length()];
                          buf.toCharArray(bufferdata,(buf.length() +1));
                          strcpy(dsConfig.ssid_esp01,bufferdata );
                         }
                         if (dsConfig.wversion == 1) { 
                          char bufferdata[buf.length()];
                          buf.toCharArray(bufferdata,(buf.length() +1));
                          strcpy(dsConfig.solax_ip_v1,bufferdata );
                         }
                        }
                        EEPROM.put(0, dsConfig);
                        EEPROM.commit(); 
                      Serial.println ("DATA SAVED!!!!");
                      Serial.println ("RESTARTING IN 5 SEC !!!!");
                      #if OLED
                        display.setTextAlignment(TEXT_ALIGN_CENTER);
                        display.clear();
                        display.drawString(64, 0,"DATA SAVED");
                        display.drawString(64, 14,"RESTARTING");
                        display.drawString(64, 28, "IN 10 SECONDS");
                        display.display();
                      #endif
                      delay(10000);
                      ESP.restart();
                      }     
                      currentLine ="";
                    } // End POST
                 // } else if (currentLine.startsWith("GET /?config")) {

                                       
                    
                    hclient.print("HTTP/1.1 200 OK\r\n"); //send new page
                    hclient.print("Content-Type: text/html\r\n\r\n"); 
                    hclient.print("<!DOCTYPE HTML>\r\n");
                    hclient.print("<HTML>\r\n");//html tag
                    hclient.print("<HEAD>\r\n");

                    hclient.println("<link rel=\"icon\" href=\"data:,\">");
                    hclient.println("<style>");

                    hclient.println (".myform { width:450px;padding:30px;margin:40px auto;background: #FFF; border-radius: 10px; -webkit-border-radius:10px; -moz-border-radius: 10px; box-shadow: 0px 0px 10px rgba(0, 0, 0, 0.13); -moz-box-shadow: 0px 0px 10px rgba(0, 0, 0, 0.13); -webkit-box-shadow: 0px 0px 10px rgba(0, 0, 0, 0.13); }");
                    hclient.println (".myform .inner-wrap{ padding: 30px;background: #F8F8F8; border-radius: 6px; margin-bottom: 15px; }");
                    hclient.println (".myform h1{background: #2A88AD;padding: 20px 30px 15px 30px;margin: -30px -30px 30px -30px;border-radius: 10px 10px 0 0;-webkit-border-radius: 10px 10px 0 0;-moz-border-radius: 10px 10px 0 0;color: #fff;text-shadow: 1px 1px 3px rgba(0, 0, 0, 0.12);font: normal 30px Arial, Helvetica;-moz-box-shadow: inset 0px 2px 2px 0px rgba(255, 255, 255, 0.17);-webkit-box-shadow: inset 0px 2px 2px 0px rgba(255, 255, 255, 0.17);box-shadow: inset 0px 2px 2px 0px rgba(255, 255, 255, 0.17);border: 1px solid #257C9E; }");
                    hclient.println (".myform h1 > span{display: block;margin-top: 2px;font: 13px Arial, Helvetica, sans-serif;}");
                    hclient.println (".myform label{display: block;font: 13px Arial, Helvetica, sans-serif;color: #888;margin-bottom: 15px;}");
                    hclient.println (".myform input[type='text'], .myform input[type='date'], .myform input[type='datetime'], .myform input[type='email'], .myform input[type='number'], .myform input[type='search'], .myform input[type='time'], .myform input[type='url'], .myform input[type='password'], .myform textarea, .myform select {display: block; box-sizing: border-box; -webkit-box-sizing: border-box; -moz-box-sizing: border-box; width: 100%; padding: 8px; border-radius: 6px; -webkit-border-radius:6px; -moz-border-radius:6px; border: 2px solid #fff; box-shadow: inset 0px 1px 1px rgba(0, 0, 0, 0.33); -moz-box-shadow: inset 0px 1px 1px rgba(0, 0, 0, 0.33); -webkit-box-shadow: inset 0px 1px 1px rgba(0, 0, 0, 0.33); }");
                    hclient.println (".myform .section{font: normal 20px Arial, Helvetica;color: #2A88AD;margin-bottom: 5px;}");
                    hclient.println (".myform .section span {background: #2A88AD;padding: 5px 10px 5px 10px;position: absolute;border-radius: 50%;-webkit-border-radius: 50%;-moz-border-radius: 50%;border: 4px solid #fff;font-size: 14px;margin-left: -45px;color: #fff;margin-top: -3px;}");
                    hclient.println (".myform input[type='button'], .myform input[type='submit']{background: #2A88AD;padding: 8px 20px 8px 20px;border-radius: 5px;-webkit-border-radius: 5px;-moz-border-radius: 5px;color: #fff;text-shadow: 1px 1px 3px rgba(0, 0, 0, 0.12);font: normal 30px Arial, Helvetica;-moz-box-shadow: inset 0px 2px 2px 0px rgba(255, 255, 255, 0.17);-webkit-box-shadow: inset 0px 2px 2px 0px rgba(255, 255, 255, 0.17);box-shadow: inset 0px 2px 2px 0px rgba(255, 255, 255, 0.17);border: 1px solid #257C9E;font-size: 15px;}");
                    hclient.println (".myform input[type='button']:hover, .myform input[type='submit']:hover{background: #2A6881;-moz-box-shadow: inset 0px 2px 2px 0px rgba(255, 255, 255, 0.28);-webkit-box-shadow: inset 0px 2px 2px 0px rgba(255, 255, 255, 0.28);box-shadow: inset 0px 2px 2px 0px rgba(255, 255, 255, 0.28);}");
                    hclient.println (".myform .privacy-policy{float: right;width: 250px;font: 12px Arial, Helvetica, sans-serif;color: #4D4D4D;margin-top: 10px;text-align: right;}");

                    hclient.println("</style>");

                    hclient.println ("<meta name='apple-mobile-web-app-capable' content='yes' />\r\n");
                    hclient.println ("<meta name='apple-mobile-web-app-status-bar-style' content='black-translucent' />\r\n");
                   
                   if(page == 1) { // Wifi Config                   
                    
                    // Search wifi networks
                    int n = WiFi.scanNetworks();
                    
                    hclient.println ("</head>");
                    hclient.println ("<body>");
                    hclient.println ("<div class=\"myform\">");
                    hclient.println ("<h1>openDS+ #Configuraci&oacuten Wifi<span>Derivador de excedentes para Solax X1</span></h1>");
                    hclient.println ("<form method=\"post\">");
                    hclient.println ("<div class=\"section\"><span>1</span>Datos conexion wifi</div>");
                    // 1 - DATOS WIFI
                    hclient.println ("<div class=\"inner-wrap\">");
                    hclient.println ("<label>SSID Wifi 1 ("+String(dsConfig.login1)+")");
                    hclient.println ("<select id='wifi1' name='wifi1'>");
                    hclient.println (" <option value=\""+String(dsConfig.login1)+"\" selected>Seleccione</option>");
                    for (int i = 0; i < n; ++i) {
                    hclient.println (" <option value=\"" +(String)(WiFi.SSID(i))+"\">"+(String)(WiFi.SSID(i))+"</option>");
                    }
                    hclient.println ("</select></label>");
                    hclient.println ("<label>Wifi 1 Password <input type=\"password\" value=\""+String(dsConfig.pass1)+"\" name=\"wifip1\"/></label>");
                    //2
                    hclient.println ("<label>SSID Wifi 2 ("+String(dsConfig.login2)+")");
                    hclient.println ("<select id='wifi2' name='wifi2'>");
                    hclient.println (" <option value=\""+String(dsConfig.login2)+"\" selected>Seleccione</option>");
                    for (int i = 0; i < n; ++i) {
                    hclient.println (" <option value=\"" +(String)(WiFi.SSID(i))+"\">"+(String)(WiFi.SSID(i))+"</option>");
                    }
                    hclient.println ("</select></label>");
                    hclient.println ("<label>Wifi 2 Password <input type=\"password\" value=\""+String(dsConfig.pass2)+"\" name=\"wifip2\"/></label>");
                    hclient.println ("</div>");

                    // Solax
                    hclient.println ("<div class=\"section\"><span>2</span>Datos Solax</div>");
                    hclient.println ("<div class=\"inner-wrap\">");
                    //Solax V1
                    if (dsConfig.wversion == 1) {
                      hclient.println ("<label>IP Wifi V1 solax <input type=\"text\" value=\""+String(dsConfig.solax_ip_v1)+"\" name=\"wifis\"/></label>");
                    }
                    //Solax V2
                    if (dsConfig.wversion == 2) {
                      hclient.println ("<label>SSID Wifi Solax ("+String(dsConfig.ssid_esp01)+")");
                      hclient.println ("<select id='wifi1' name='wifis'>");
                      hclient.println (" <option value=value=\""+String(dsConfig.ssid_esp01)+"\" selected>Seleccione</option>");
                      for (int i = 0; i < n; ++i) {
                        hclient.println (" <option value=\"" +(String)(WiFi.SSID(i))+"\">"+(String)(WiFi.SSID(i))+"</option>");
                      }
                      hclient.println ("</select></label>");
                    }
                    hclient.println ("</div>");
                    // End
                        
                    hclient.println ("<div class=\"button-section\">");
                    hclient.println ("  <input type=\"submit\" name=\"Guardar\" value=\"Guardar\"/>");
                    hclient.println ("  <input type=\"button\" name=\"cancel\" value=\"Cancelar\" onClick=\"window.location.href=\'/\'\">");
                    hclient.println ("</div>");
                    hclient.println ("</div>");
                    hclient.println ("</div>");
                    hclient.println ("</form>");
                    hclient.println ("</div>");
                    
                   } else if (page == 2) { // Config data
                    
                    // CONFIG MQTT /////////////////////////////////////////////////
                    Serial.println("CONFIG DATA...................");
                    hclient.println ("</head>");
                    hclient.println ("<body>");
                    hclient.println ("<div class=\"myform\">");
                    hclient.println ("<h1>openDS+ #Configuraci&oacuten <span>Derivador de excedentes para Solax X1</span></h1>");
                    // action=\"/proc\"
                    hclient.println ("<form method=\"post\">");
                    hclient.println ("<div class=\"section\"><span>1</span>Datos MQTT</div>");
                    // 1 - DATOS MQTT
                    hclient.println ("<div class=\"inner-wrap\">");
                    {
                    String tmp = "";
                    if (dsConfig.mqtt == true) tmp = " checked ";
                    hclient.println ("<label>MQTT Activo <input type=\"checkbox\" value=\"Yes\" name=\"mqttactive\" "+ tmp +"/></label>");
                    }
                    hclient.println ("<label>MQTT Broker <input type=\"textname\" value=\""+String(dsConfig.MQTT_broker)+"\" name=\"broker\"/></label>");
                    hclient.println ("<label>MQTT Usuario <input type=\"textname\" value=\""+String(dsConfig.MQTT_user)+"\" name=\"mqttuser\"/></label>");
                    hclient.println ("<label>MQTT Password <input type=\"password\" value=\""+String(dsConfig.MQTT_password)+"\" name=\"mqttpass\"/></label>");
                    hclient.println ("<label>MQTT Puerto <input type=\"textname\" value=\""+String(dsConfig.MQTT_port)+"\" name=\"mqttport\"/></label>");
                    hclient.println ("</div>");
                    // End    
                    hclient.println ("<div class=\"button-section\">");
                    hclient.println ("  <input type=\"submit\" name=\"Guardar\" value=\"Guardar\"/>");
                    hclient.println ("  <input type=\"button\" name=\"cancel\" value=\"Cancelar\" onClick=\"window.location.href=\'/\'\">");
                    hclient.println ("</div>");
                    hclient.println ("</div>");
                    hclient.println ("</div>");
                    hclient.println ("</form>");
                    hclient.println ("</div>");

                    
                   } else if (page == 3) { // Relay page
                    Serial.println("RELAY DATA...................");
    
                    hclient.println ("</head>");
                    hclient.println ("<body>");
                    hclient.println ("<div class=\"myform\">");
                    hclient.println ("<h1>openDS+ #Configuraci&oacuten Salidas<span>Derivador de excedentes para Solax X1</span></h1>");
                    // action=\"/proc\"
                    hclient.println ("<form method=\"post\">");
                    hclient.println ("<div class=\"section\"><span>1</span>Control de salidas</div>");
                    // 1 - DATOS MQTT
                    hclient.println ("<div class=\"inner-wrap\">");
                    hclient.println ("<label>Coming soon, in the next version <input type=\"textname\" value=\"NADA\" name=\"broker\"/></label>");
                    hclient.println ("</div>");
                    // End    
                    hclient.println ("<div class=\"button-section\">");
                    hclient.println ("  <input type=\"submit\" name=\"Guardar\" value=\"Guardar\"/>");
                    hclient.println ("  <input type=\"button\" name=\"cancel\" value=\"Cancelar\" onClick=\"window.location.href=\'/\'\">");
                    hclient.println ("</div>");
                    hclient.println ("</div>");
                    hclient.println ("</div>");
                    hclient.println ("</form>");
                    hclient.println ("</div>");

                   } else {
                    
                    ////////////////////////////////Main Page///////////////////////
                    hclient.println ("<meta http-equiv='refresh' content='30'/>\r\n");
                    hclient.println ("</head>");
                    hclient.println ("<body>");
                    hclient.println ("<form method=\"get\">");
                    hclient.println ("<div class=\"myform\">");
                    hclient.println ("<h1>openDS+<span>Derivador de excedentes para Solax X1 V"+String(dsConfig.wversion)+"</span></h1>");
                   
                    if (serror || werror) {
                      hclient.println ("<div class=\"section\"><span>0</span>Errores</div>");
                      hclient.println ("<div class=\"inner-wrap\">");
                      if (werror) hclient.println ("<label>No se puede conectar a la Wifi Local</label>");
                      if (serror) hclient.println ("<label>No se puede conectar al inversor Solax</label>");
                      hclient.println ("</div>");
                    }
                    hclient.println ("<div class=\"section\"><span>1</span>Monitorizaci&oacute;n</div>");
                    hclient.println ("<div class=\"inner-wrap\">");
                    hclient.println ("<label>Potencia Solar      : <input type=\"textname=\" value=\"" +String(solax_wsolar) + "\" disabled/> W</label>");
                    hclient.println ("<label>Potencia de Red     : <input type=\"textname=\" value=\"" +String(solax_wgrid) + "\" disabled/> W</label>");
                    hclient.println ("<label>Energ&iacute;a Diaria Solar: <input type=\"textname=\" value=\"" +String(solax_wtoday) + "\" disabled/>Wh</label>");
                    hclient.println ("</div>");
                    hclient.println ("<div class=\"button-section\">");
                    hclient.println ("  <input type=\"button\" name=\"cnet\" value=\"Red\" onClick=\"window.location.href=\'/?cnet\'\">");
                    hclient.println ("  <input type=\"button\" name=\"config\" value=\"Config\" onClick=\"window.location.href=\'/?config\'\">");
                    hclient.println ("  <input type=\"button\" name=\"relay\" value=\"Salidas\" onClick=\"window.location.href=\'/?relay\'\">");
                    hclient.println ("</div>");
                    hclient.println ("</div>");
                    hclient.println ("</form>");
                   }
                    hclient.println ("</body> ");
                    
                    hclient.println ("</HTML>");
                    
                    // The HTTP response ends with another blank line:
                    hclient.println();
                    hclient.stop();
                    page = 0;
                    break;  
                    

                  } else {    // if you got a newline, then clear currentLine:
                    currentLine = "";
                  }
                } else if (c != '\r') {  // if you got anything else but a carriage return character,
                  currentLine += c;      // add it to the end of the currentLine
                }

        }
        cont++; // If the connection crash, break while
        if (cont == 900) break;

    }
    // close the connection:
    hclient.stop();
    Serial.println("Client Disconnected.");
   }
      delay(1);
  } // End for
} // End loop
