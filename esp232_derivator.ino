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

#define OLED false
// !!!!Turn to true when oled screen is available!!!! --> WIFI KIT 32

#if OLED
#include <SSD1306.h>                                                              
#endif

#if OLED 
SSD1306 display(0x3c, 4, 15); //For OLED
#endif

#define true 0x1
#define false 0x0
#define USE_SERIAL Serial

// Update these with values suitable for your network.

//const char* ssid = "";
//const char* password = "";
//const char* ssid2 = "";
//const char* password2 = "";
//Cambiar parametros temporalmente en la función de setup
const char* solax_api = "http://192.168.90.105/api/realTimeData.htm"; //Api for Wifi solax V1, change only the IP

//-------- mqtt setup --------//
const char solax_mqtt_active = true; // turn false to deactivate mqtt
const char * MQTT_broker = "192.168.90.15";//name of the mqtt broker
const char * MQTT_user = "moscon";//username to access to the broker
const char * MQTT_password = "pepon";//password to access to the broker
const uint16_t MQTT_port = 1883; // port  to access to the broker

// Set up the pwm output
#if OLED
uint8_t pin_pwm = 25;
#else
uint8_t pin_pwm = 2;
#endif
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
// solax/wtogrid -> Energia enviada a red

// START CODE
WiFiMulti wifiMulti;

uint8_t error = 0;

double solax_pv1c ; //corriente string 1
double solax_pv2c ; //corriente string 2
int16_t solax_pv1v ; //tension string 1
int16_t solax_pv2v ; //tension string 2
int16_t solax_gridv ; // tension de red
double solax_wsolar ; // Potencia solar
double solax_wtoday ; // Potencia solar diaria
double solax_wgrid ; // Potencia de red (Negativo: de red - Positivo: a red)
double solax_wtogrid ; // Potencia enviada a red

// EEPROM Data
struct CONFIG {
  byte eeinit;
  char login1[20];
  char pass1[20];
  char login2[20];
  char pass2[20];
  byte wversion;
  char ssid_AP[20];
  char password_AP[20];
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
    solax_wtogrid = root["Data"][41]; // Energia enviada a red

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
  if (client.connect("SodeClient", MQTT_user, MQTT_password)) {
    Serial.println("connected");
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
      client.subscribe("solax/wtogrid");
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
    if (dsConfig.eeinit != 99 ) {
      USE_SERIAL.println("Configuration Not Found, initializing");
      dsConfig.eeinit = 99;
      dsConfig.wversion = 1;
	  // Cambiar aqui datos mientras no se desarrolla el setup via web
      strcpy(dsConfig.login1, "SSID-1");
      strcpy(dsConfig.login2, "SSID-2");
      strcpy(dsConfig.pass1, "PASS1");
      strcpy(dsConfig.pass2, "PASS2");
      strcpy(dsConfig.ssid_AP, "Mi_DS_Plus");
      strcpy(dsConfig.password_AP, "1234");
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
    Serial.print("Conecting.");
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(dsConfig.ssid_AP, dsConfig.password_AP);
   
    USE_SERIAL.print("AP IP address: ");
    USE_SERIAL.println(WiFi.softAPIP());
    
     
    wifiMulti.addAP(dsConfig.login1, dsConfig.pass1);
    wifiMulti.addAP(dsConfig.login2, dsConfig.pass2);
    
    while (wifiMulti.run() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    
    if(wifiMulti.run() == WL_CONNECTED) {
     #if OLED
      display.setTextAlignment(TEXT_ALIGN_CENTER);
      display.clear();
      display.drawString(64, 0,"CONECTED:");
      display.drawString(64, 14,"IP ADDRESS :");
      display.drawString(64, 28, String(WiFi.localIP()));
     #endif
     Serial.println("");
      USE_SERIAL.println("WiFi connected");
      USE_SERIAL.println("IP address: ");
      USE_SERIAL.println(WiFi.localIP());
      #if OLED
        display.display();
      #endif
      server.begin();
    }
    
    // PWM
    
    // Initialize channels 
    // channels 0-15, resolution 1-16 bits, freq limits depend on resolution
    ledcSetup(0, 1000, 16); // 1 kHz PWM, 8-bit resolution
    ledcAttachPin(pin_pwm, 0); // assign pins to chanel
    ledcWrite(0, solax_pwm);  // Write new pwm value

    if (solax_mqtt_active == true ) {
      client.setServer(MQTT_broker, MQTT_port);
      client.setCallback(callback);
    }

    
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
    // wait for WiFi connection
    if((wifiMulti.run() == WL_CONNECTED)) {
        
        if (solax_mqtt_active == true ) {
          if (!client.connected()) {
            #if OLED 
              display.setTextAlignment(TEXT_ALIGN_CENTER);
              display.drawString(64, 38,"MQTT: Conecting...");
              display.display();
            #endif
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
                
                // Check pwm_output
                if (solax_pwm_active == true) {  
                  if ( solax_wgrid > -60 ) { // Value is < 60 w from grid an pwm < 100
                    USE_SERIAL.println("PWM: SUBIENDO POTENCIA");
                    if  ( solax_wgrid > 200 && solax_pwm < 165 ) solax_pwm+= 15; // Increment 15 if the grid to power > 200w
                    if (solax_pwm >= 180) solax_pwm = 179; // 180 = 100%
                    solax_pwm += 1 ; //Increment value
                    ledcWrite(0, pwm_calc(solax_pwm));  // Write new pwm value
                    if (solax_mqtt_active == true ) publisher("/solax/pwm",(String)solax_pwm);
                   }
                  
                  else if  ( solax_wgrid < -90 )  { // Value is < 90 w from grid an pwm < 125
                    USE_SERIAL.println("PWM: BAJANDO POTENCIA");
                    if  ( solax_wgrid < -350  && solax_pwm > 15) solax_pwm-= 15; // Decrement 15 if the grid from power > 200w
                    if (solax_pwm <= 0) solax_pwm = 1;
                    solax_pwm-= 1 ; //Decrement value 
                    ledcWrite(0, pwm_calc(solax_pwm));  // Write new pwm value
                    if (solax_mqtt_active == true ) publisher("/solax/pwm", (String)solax_pwm );
                    
                   }
                 }

                publisher("/solax/wgrid",(String)solax_pwm);
                // Publish mqtt values
                if (solax_mqtt_active == true ) {
                  publisher("/solax/pv1c", (String)solax_pv1c);
                  publisher("/solax/pv2c", (String)solax_pv2c);
                  publisher("/solax/pw1v", (String)solax_pv1v);
                  publisher("/solax/pw2v", (String)solax_pv2v);
                  publisher("/solax/gridv", (String)solax_gridv);
                  publisher("/solax/wsolar", (String)solax_wsolar);
                  publisher("/solax/wtoday", (String)solax_wtoday);
                  publisher("/solax/wgrid",  (String)solax_wgrid);
                  publisher("/solax/wtogrid", (String)solax_wtogrid);
                
                } 

                // Display values
                #if OLED 
                  display.clear();
                  display.setTextAlignment(TEXT_ALIGN_LEFT);
                  display.setFont(ArialMT_Plain_10);
                  display.drawString(0, 0, "Power Solar . Power Grid");
                  display.setFont(ArialMT_Plain_24);
                  display.drawString(0, 12, (String)solax_wsolar);
                  display.drawString(64, 12, (String)(int)solax_wgrid);
                #endif

                // Check Relay control

                String pro = String((solax_pwm * 100) / 180) + "%";
                int progressbar = ((solax_pwm * 100) / 180);
                                
                
                USE_SERIAL.println("PWM VALUE: % ");
                USE_SERIAL.println(pro);
                USE_SERIAL.println("PWM VALUE: ");
                USE_SERIAL.println(solax_pwm);
                USE_SERIAL.println("PROGRESS BAR : ");
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
            USE_SERIAL.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
            #if OLED
              display.setTextAlignment(TEXT_ALIGN_CENTER);
              display.drawString(64, 52,"Error en red - 01");
              display.display();
            #endif
            
        }

        http.end();
 
    // Http server
    USE_SERIAL.println("IP address: ");
    USE_SERIAL.println(WiFi.localIP());
    
    for (int16_t s=0;s<=3000;s++) {
      WiFiClient hclient = server.available();
      if (hclient) {                             // if you get a client,
        USE_SERIAL.println("New Client.");           // print a message out the serial port
        String currentLine = "";                // make a String to hold incoming data from the client
        unsigned int cont = 0;
        while (hclient.connected()) {            // loop while the client's connected
            if (hclient.available()) {             // if there's bytes to read from the client,
              char c = hclient.read();             // read a byte, then
              Serial.write(c);                    // print it out the serial monitor
                if (c == '\n') {                    // if the byte is a newline character

                  // if the current line is blank, you got two newline characters in a row.
                  // that's the end of the client HTTP request, so send a response:
                  if (currentLine.length() == 0) {
                 
                    hclient.print("HTTP/1.1 200 OK\r\n"); //send new page
                    hclient.print("Content-Type: text/html\r\n\r\n"); 
                    hclient.print("<!DOCTYPE HTML>\r\n");
                    hclient.print("<HTML>\r\n");//html tag
                    hclient.print("<HEAD>\r\n");
                    hclient.print("<meta http-equiv='refresh' content='10'/>\r\n");
                    hclient.print("<meta name='apple-mobile-web-app-capable' content='yes' />\r\n");
                    hclient.print("<meta name='apple-mobile-web-app-status-bar-style' content='black-translucent' />\r\n");
                    hclient.print("<link rel='stylesheet' href='https://maxcdn.bootstrapcdn.com/bootstrap/3.3.7/css/bootstrap.min.css'><script src='https://ajax.googleapis.com/ajax/libs/jquery/3.1.1/jquery.min.js'></script><script src='https://maxcdn.bootstrapcdn.com/bootstrap/3.3.7/js/bootstrap.min.js'></script>\r\n");
                    hclient.print("<title>SOLAX openDS+</title>\r\n");
                    hclient.print("</HEAD>\r\n");
                    hclient.print("<BODY>\r\n");
                    hclient.print("<div class='container-fluid'><div class='row'><div class='col-md-12'>");
                    hclient.print("<h1>OpenDS+ Derivador de excedentes</h1>\r\n");
                    hclient.print("<h3>para Solax X1 Boost con Wifi Kit</h3>\r\n"); 
                    hclient.print("<br />\r\n"); 
                    hclient.print("<ul class='nav nav-pills'>");
                    hclient.print("<li class='active'> <span class='badge pull-right'>" +String(solax_wsolar) + "w</span> Solar : </li>");
                    hclient.print("<li> <span class='badge pull-right'>" +String(solax_wgrid)+"w</span> Red : </li>");
                    hclient.print("<li> <span class='badge pull-right'>" +String(solax_wtoday)+"Kwh</span> Diario : </li>");
                    hclient.print("</ul>");
// Tablas
                    hclient.print("<table class='table'>");  
                    hclient.print("<thead><tr><th>ID</th><th>Medida</th><th>Valor</th></tr></thead><tbody>");
                    hclient.print("<tr><td>wsolar</td><td>Potencia Solar</td><td>"+String(solax_wsolar) + "W</td></tr>");

                    hclient.print("<tr><td>pv1c</td><td>Corriente String-1</td><td>"+String(solax_pv1c) + "A</td></tr>");
                    hclient.print("<tr><td>pv2c</td><td>Corriente String-2</td><td>"+String(solax_pv2c) + "A</td></tr>");
                    hclient.print("<tr><td>solax_pv1v</td><td>Tensi&oacute;n String-1</td><td>"+String(solax_pv1v) + "V</td></tr>");
                    hclient.print("<tr><td>pv2v</td><td>Tensi&oacute;n String-2</td><td>"+String(solax_pv2v) + "V</td></tr>");
                    hclient.print("<tr><td>gridv</td><td>Tensi&oacute;n de red</td><td>"+String(solax_gridv) + "V</td></tr>");
                    hclient.print("<tr><td>wsolar</td><td>Consumo de red inst&aacute;ntaneo</td><td>"+String(solax_wgrid) + "W</td></tr>");
                    hclient.print("<tr><td>wtoday</td><td>Energ&iacute;a solar diaria</td><td>"+String(solax_wtoday) + "KWh</td></tr>");
                    hclient.print("<tr><td>wtogrid</td><td>Energ&iacute;a solar enviada a red</td><td>"+String(solax_wtogrid) + "KWh</td></tr>");
 
                    hclient.print("</tbody></table>");


// GPIOS

                    hclient.print("<div class='row'>");
                    hclient.print("<div class='col-md-4'><h4 class ='text-left'>Relay/1 <span class='badge'> Rele 1 </span></h4></div>");
                    hclient.print("<div class='col-md-4'><form action='/' method='POST'><button type='button submit' name='R1' value='1' class='btn btn-success btn-lg'>ON</button></form></div>");
                    hclient.print("<div class='col-md-4'><form action='/' method='POST'><button type='button submit' name='R1' value='0' class='btn btn-danger btn-lg'>OFF</button></form></div>");
                    
                    hclient.print("<div class='col-md-4'><h4 class ='text-left'>Relay/2 <span class='badge'> Rele 2 </span></h4></div>");
                    hclient.print("<div class='col-md-4'><form action='/' method='POST'><button type='button submit' name='R2' value='1' class='btn btn-success btn-lg'>ON</button></form></div>");
                    hclient.print("<div class='col-md-4'><form action='/' method='POST'><button type='button submit' name='R2' value='0' class='btn btn-danger btn-lg'>OFF</button></form></div>");

                    
                    hclient.print("<div class='col-md-4'><h4 class ='text-left'>Relay/3 <span class='badge'> Rele 3 </span></h4></div>");
                    hclient.print("<div class='col-md-4'><form action='/' method='POST'><button type='button submit' name='R3' value='1' class='btn btn-success btn-lg'>ON</button></form></div>");
                    hclient.print("<div class='col-md-4'><form action='/' method='POST'><button type='button submit' name='R3' value='0' class='btn btn-danger btn-lg'>OFF</button></form></div>");

                    
                    hclient.print("<div class='col-md-4'><h4 class ='text-left'>Relay/4 <span class='badge'> Rele 4 </span></h4></div>");
                    hclient.print("<div class='col-md-4'><form action='/' method='POST'><button type='button submit' name='R4' value='1' class='btn btn-success btn-lg'>ON</button></form></div>");
                    hclient.print("<div class='col-md-4'><form action='/' method='POST'><button type='button submit' name='R4' value='0' class='btn btn-danger btn-lg'>OFF</button></form></div>");
                    
                    hclient.print("</div><br><p><a href='http://www.github.com/iqas/derivador'>www.github.com</p></div></div></div>");
                    
                    hclient.print("<br />\r\n");  
                    hclient.print("<a href=\"/?ver2\"><font color = \"green\">Solax Wifi v1</font></a>\r\n");
                    hclient.print("<a href=\"/?config\"><font color = \"red\">Config</font></a><br />\r\n");   
                    hclient.print("<br />\r\n");     
                    
                    
                    hclient.print("<br />\r\n");
                    hclient.print("</BODY>\r\n");
                    hclient.print("</HTML>\n");

                    // The HTTP response ends with another blank line:
                    hclient.println();
                    
                    break;
                  } else {    // if you got a newline, then clear currentLine:
                    currentLine = "";
                  }
              } else if (c != '\r') {  // if you got anything else but a carriage return character,
                  currentLine += c;      // add it to the end of the currentLine
              }

          // Check to see if the client request was "GET /H" or "GET /L":
          if (currentLine.endsWith("GET /?config")) {
            //digitalWrite(5, HIGH);               // GET /H turns the LED on
            
                    hclient.print("HTTP/1.1 200 OK\r\n"); //send new page
                    hclient.print("Content-Type: text/html\r\n\r\n"); 
                    hclient.print("<!DOCTYPE HTML>\r\n");
                    hclient.print("<HTML>\r\n");//html tag
                    hclient.print("<HEAD>\r\n");
                    hclient.print("<meta name='apple-mobile-web-app-capable' content='yes' />\r\n");
                    hclient.print("<meta name='apple-mobile-web-app-status-bar-style' content='black-translucent' />\r\n");
                    hclient.print("<TITLE>SOLAX openDS+</TITLE>\r\n");
                    hclient.print("</HEAD>\r\n");
                    hclient.print("<BODY>\r\n");
                    hclient.print("<H1>OpenDS+ Derivador de excedentes</H1>\r\n");
                    hclient.print("<H1>para Solax X1 Boost con Wifi Kit</H1>\r\n"); 
                    hclient.print("<br />\r\n"); 
                    hclient.print("<H2>Configuración: </H2>\r\n");
                    hclient.print("<br />\r\n");  
                    hclient.print("<a href=\"/?\"><font color = \"green\">Inicio</font></a>\r\n");   
                    hclient.print("<br />\r\n");     
                    hclient.print("<H2>Config: </H2>\r\n");
                    hclient.print("<p><<form> Tipo de Wifi Pocket (Versi&oacute;n) <input name=\"pocket\" type=\"text\" value=\"V1\" /> <br /></p>\r\n");
                    hclient.print("<input name=\"Enviar\" type=\"submit\" value=\"Enviar\"></form><br /></p>\r\n");
                    hclient.print("<br />\r\n");
                    hclient.print("</BODY>\r\n");
                    hclient.print("</HTML>\n");

          }
          if (currentLine.endsWith("GET /L")) {
            digitalWrite(5, LOW);                // GET /L turns the LED off
          }
        }
        cont++; // If the connection crash, break while
        if (cont == 500) break;

    }
    // close the connection:
    hclient.stop();
    Serial.println("Client Disconnected.");
   }
      delay(1);
  } // End for

 } // End If Wifi
} // End loop
