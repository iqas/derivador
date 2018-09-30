/**
 * Derivador de excedentes para ESP32 DEV Kit
 *
 *  Created on: 2018 by iqas
 *
 */

#include <Arduino.h>
#include <Wire.h> 
#include <WiFi.h>
#include <WiFiMulti.h>
#include <WiFiClient.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
//#include "SSD1306.h"
#include <PubSubClient.h>// to use MQTT protocol

#define USE_SERIAL Serial
#define true 0x1
#define false 0x0

//SSD1306 display(0x3c, 4, 15); For OLED


// Update these with values suitable for your network.

const char* ssid = "WIFI1";
const char* password = "password1";
const char* ssid2 = "WIFI2";
const char* password2 = "password2";
const char* solax_api = "http://192.168.90.105/api/realTimeData.htm"; //Api for Wifi solax V1, change only the IP

//-------- mqtt setup --------//
const char solax_mqtt_active = true; // turn false to deactivate mqtt
const char * MQTT_broker = "192.168.90.15";//name of the mqtt broker
const char * MQTT_user = "mqttuser";//username to access to the broker
const char * MQTT_password = "mqttpass";//password to access to the broker
const uint16_t MQTT_port = 1883; // port  to access to the broker

// Set up the pwm output
uint8_t pin_pwm = 2;
uint16_t solax_pwm = 0;          // a value from 0 to 65535 representing the pwm value
uint16_t solax_increment  = 6000;  // Value for increment or decrement pwm, 6000 is good form me with a 16 bits pwm
char solax_pwm_active = true;

// Set up relay output
#define PIN_RL1 21
#define PIN_RL2 5
#define PIN_RL3 19
#define PIN_RL4 23

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
// solax/gridv ->  tension de red
// solax/wsolar ->  Potencia solar
// solax/wtoday ->  Potencia solar diaria
// solax/wgrid ->  Potencia de red (Negativo: de red - Positivo: a red)
// solax/wgridday -> Potencia diaria consumida de red

// START CODE
WiFiMulti wifiMulti;

int16_t solax_pv1c ; //corriente string 1
int16_t solax_pv2c ; //corriente string 2
int16_t solax_pv1v ; //tension string 1
int16_t solax_pv2v ; //tension string 2
int16_t solax_gridv ; // tension de red
int16_t solax_wsolar ; // Potencia solar
int16_t solax_wtoday ; // Potencia solar diaria
double solax_wgrid ; // Potencia de red (Negativo: de red - Positivo: a red)
int16_t solax_wgridday ; // Potencia diaria consumida de red

WiFiClient espClient;
PubSubClient client(espClient);

/******** void ParseJson***********************
  this funcition receives json data from
   http request and decode and print it
 ***********************************************/

void parseJson(String json) {
  DynamicJsonBuffer  jsonBuffer;
  JsonObject& root = jsonBuffer.parseObject(json);
  if (!root.success()) {
    Serial.println("parseObject() failed");
    //publisher("this is not a Json","JsonStatus");
    return;
  }
  
    //publisher("Json ok ","JsonStatus");
    solax_pv1c = root["Data"][0]; //corriente string 1
    solax_pv2c = root["Data"][1]; //corriente string 2
    solax_pv1v = root["Data"][2]; //tension string 1
    solax_pv2v = root["Data"][3]; //tension string 2
    solax_gridv = root["Data"][5]; // tension de red
    solax_wsolar = root["Data"][6]; // Potencia solar
    solax_wtoday = root["Data"][8]; // Potencia solar diaria
    solax_wgrid = root["Data"][10]; // Potencia de red (Negativo: de red - Positivo: a red)
    solax_wgridday = root["Data"][3]; // Potencia diaria consumida de red

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
void publisher(String topic, String topublish) {
  if (client.connect("SodeClient", MQTT_user, MQTT_password)) {
    //Serial.println("connected");
    // Once connected, publish an announcement...
    //client.publish(topic, topublish);
  } else {
    Serial.print(F("failed, rc="));
    Serial.print(client.state());
    Serial.println(" try again in 5 seconds");
    // Wait 5 seconds before retrying
    delay(5000);
    reconnect();
  }
}

/*
   when the mqtt client is disconnected,
   then try to reconnect to mqtt broker.
   and suscribe the mqtt suscription topic
   else try to reconnect each 5+1 seconds
*/

void reconnect() {
  // Loop until we're reconnected
  String mqtt_dir;
  if (!client.connected()) {
    Serial.print(F("Attempting MQTT connection..."));
    // Attempt to connect
    if (client.connect("SodeClient", MQTT_user, MQTT_password)) {
      Serial.println("connected");
      
      client.subscribe("solax/pv1c");
      client.subscribe("solax/pv2c");
      client.subscribe("solax/pv1v");
      client.subscribe("solax/pv2v");
      client.subscribe("solax/gridv");
      client.subscribe("solax/wsolar");
      client.subscribe("solax/wtoday");
      client.subscribe("solax/wgrid");
      client.subscribe("solax/wgridday");
    } else {
      Serial.print(F("failed, rc="));
      //Serial.print(client.stat());
      client.disconnect();
      client.setServer(MQTT_broker, MQTT_port);
      client.setCallback(callback);
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(1000);
    }
  }
}
void setup() {

     // OLED
    //pinMode(25,OUTPUT);

    //pinMode(16,OUTPUT);
    //digitalWrite(16, LOW);    // set GPIO16 low to reset OLED
    //delay(50);
    //digitalWrite(16, HIGH); // while OLED is running, must set GPIO16 in high

    //display.init();
    //display.flipScreenVertically();
    //display.setFont(ArialMT_Plain_10);
    // SERIAL

    USE_SERIAL.begin(115200);

    USE_SERIAL.println();
    USE_SERIAL.println();
    USE_SERIAL.println();

    for(uint8_t t = 4; t > 0; t--) {
        USE_SERIAL.printf("[SETUP] WAIT %d...\n", t);
        USE_SERIAL.flush();
        delay(1000);
    }

    // WIFI
    wifiMulti.addAP(ssid, password);
    wifiMulti.addAP(ssid2, password2);
    //display.drawString(0, 0, "Connecting...");
    //display.display();
    //delay(1000);
    //display.clear();

    // PWM
    
    // Initialize channels 
    // channels 0-15, resolution 1-16 bits, freq limits depend on resolution
    ledcSetup(0, 1000, 16); // 1 kHz PWM, 8-bit resolution
    ledcAttachPin(pin_pwm, 0); // assign pins to chanel
    ledcWrite(0, solax_pwm);  // Write new pwm value

 
    client.setServer(MQTT_broker, MQTT_port);
    client.setCallback(callback);
}

void loop() {
    // wait for WiFi connection
    if((wifiMulti.run() == WL_CONNECTED)) {
        
        if (solax_mqtt_active == true ) {
          if (!client.connected()) {
              reconnect();
          }
          else client.loop();
        }

        HTTPClient http;

        USE_SERIAL.print("[HTTP] begin...\n");
        // configure traged server and url
        http.begin(solax_api); //HTTP

        USE_SERIAL.print("[HTTP] GET...\n");
        // start connection and send HTTP header
        int httpCode = http.GET();

        // httpCode will be negative on error
        if(httpCode > 0) {
            // HTTP header has been send and Server response header has been handled
            USE_SERIAL.printf("[HTTP] GET... code: %d\n", httpCode);

            // file found at server
            if(httpCode == HTTP_CODE_OK) {
                String payload = http.getString();
                parseJson(payload);
                
                // Display values
                //display.clear();
                //display.setTextAlignment(TEXT_ALIGN_LEFT);
                //display.setFont(ArialMT_Plain_10);
                //display.drawString(0, 0, "Power Solar . Power Grid");
                //display.setFont(ArialMT_Plain_24);
                //display.drawString(0, 12, (String)solax_wsolar);
                //display.drawString(64, 12, (String)(int)solax_wgrid);


                // Check pwm_output
                if (solax_pwm_active == true) {  
                  if ( ( solax_wgrid > -100 ) ) { // Value is < 100 w from grid an pwm < 256
                    USE_SERIAL.println("SUBIENDO");
                    if (solax_pwm >= 65535-solax_increment) solax_pwm = 65535-solax_increment; // 255 = 100%
                    solax_pwm += solax_increment ; //Increment value
                    ledcWrite(0, solax_pwm);  // Write new pwm value
                    if (solax_mqtt_active == true ) publisher("/solax/pwm", String(solax_pwm));
                   }
                  
                  else if  ( ( solax_wgrid < -125 ) ) { // Value is < 100 w from grid an pwm < 256
                    USE_SERIAL.println("BAJANDO");
                    if (solax_pwm <= 0+solax_increment) solax_pwm = 0+solax_increment;
                    solax_pwm -= solax_increment ; //Decrement value 
                    ledcWrite(0, solax_pwm);  // Write new pwm value
                    if (solax_mqtt_active == true ) publisher("/solax/pwm", String(solax_pwm));
                    
                   }
                 }
                // Publish mqtt values
                if (solax_mqtt_active == true ) {
                  publisher("/solax/pv1c", String(solax_pv1c));
                  publisher("/solax/pv2c", String(solax_pv2c));
                  publisher("/solax/pw1v", String(solax_pv1v));
                  publisher("/solax/pw2v", String(solax_pv2v));
                  publisher("/solax/gridv", String(solax_gridv));
                  publisher("/solax/wsolar", String(solax_wsolar));
                  publisher("/solax/wtoday", String(solax_wtoday));
                  publisher("/solax/wgrid", String(solax_wgrid));
                  publisher("/solax/wgridday", String(solax_wgridday));
                
                }
                // Check Relay control

                String pro = String((solax_pwm * 100) / 65535) + "%";
                int progressbar = ((solax_pwm * 100) / 65535);
                                
                
                USE_SERIAL.println("PWM VALUE: % ");
                USE_SERIAL.println(pro);
                USE_SERIAL.println("PWM VALUE: ");
                USE_SERIAL.println(solax_pwm);
                USE_SERIAL.println("PROGRESS BAR : ");
                USE_SERIAL.println(progressbar);

                //display.setFont(ArialMT_Plain_10);
                //display.drawProgressBar(0, 38, 120, 10, progressbar);    // draw the progress bar    
                //display.setTextAlignment(TEXT_ALIGN_CENTER);          // draw the percentage as String
                //display.drawString(64, 52,"PWM "+pro + " Relay: o o o o");
                //display.display();
                
            }
        } else {
            USE_SERIAL.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
        }

        http.end();
    }

    delay(3000);
}
