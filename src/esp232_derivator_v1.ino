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

#define HTTPCLIENT_DEFAULT_TCP_TIMEOUT = 500;
#define DEBUG true
#define OLED true
// !!!!Turn to true when oled screen is available!!!! --> WIFI KIT 32

#if OLED
#include "SSD1306.h"                                                             
#endif

#if OLED 
SSD1306 display(0x3c, 4, 15); //For OLED
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
uint8_t pin_rx = 16;
uint8_t pin_tx = 17;

#endif
uint16_t solax_pwm = 0;          // a value from 0 to 65535 representing the pwm value
String espResp;
boolean flash = true;

// Set up relay output
#define PIN_RL1 13
#define PIN_RL2 12
#define PIN_RL3 14
#define PIN_RL4 27

// END USER CONFIG

//  MQTT TOPICS:
// /solax/pwm -> PWM Value
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

uint8_t nerror = 0; // Nº de erHaror
int cerror = 0; // Nº de errores de conexión a red
uint8_t merror = 0; // Nº de errores de conexión a mqtt
boolean werror = false; // Error en conexión Wifi
boolean serror = true; // Error conexión Solax
unsigned long wdt;
unsigned long src;
boolean rjson=false;
uint8_t soff=0; // Soft off Relay


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
boolean pwm_man; // Encendido manual de la salida pwm

String esp01_version="";
int esp01_status=0;
String esp01_payload="";
int httpcode;
int counter;

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
  boolean dhcp;
  char ip[16];
  char gw[16];
  char mask[16];
  char dns1[16];
  char dns2[16];
  boolean P01_on; //PWM active
  int pwm_min; 
  int pwm_max;
  boolean R01_man; //Relay 1 control output
  int R01_min;
  boolean R02_man; //Relay 2 control output
  int R02_min;
  boolean R03_man; //Relay 3 control output
  int R03_min;
  boolean R04_man; //Relay 4 control output
  int R04_min;
  char R01_mqtt[50];
  char R02_mqtt[50];
  char R03_mqtt[50];
  char R04_mqtt[50];
  char password[12];
  };

CONFIG dsConfig;

WiFiClient espClient;
WiFiServer server(80);
PubSubClient client(espClient);

/******** void ParseJson***********************
  this funcition receives json data from
   http requfest and decode and print it
 ***********************************************/

void parseJson(String json) {
  USE_SERIAL.println("JSON:" + json);
  DynamicJsonBuffer  jsonBuffer;
  JsonObject& root = jsonBuffer.parseObject(json);
  if (!root.success()) {
    USE_SERIAL.println("parseObject() failed");
    rjson = false;
    httpcode = -1;
  } else USE_SERIAL.println("parseObject() OK");
  
    //publisher("Json ok ","JsonStatus");
    httpcode = root["Data"][0]; //Error code
    solax_pv1c = root["Data"][1]; //corriente string 1
    solax_pv2c = root["Data"][2]; //corriente string 2
    solax_pv1v = root["Data"][3]; //tension string 1
    solax_pv2v = root["Data"][4]; //tension string 2
    solax_pw1 = root["Data"][5]; //potencia string 1
    solax_pw2 = root["Data"][6]; //potencia string 2
    solax_gridv = root["Data"][7]; // tension de red
    solax_wsolar = root["Data"][8]; // Potencia solar
    solax_wtoday = root["Data"][9]; // Potencia solar diaria
    solax_wgrid = root["Data"][10]; // Potencia de red (Negativo: de red - Positivo: a red)
    solax_wtogrid = root["Data"][11]; // Potencia diaria enviada a red

    USE_SERIAL.print("POWER:");
    USE_SERIAL.println(solax_wgrid);
    rjson = true;
  
}

void parseJsonv1(String json) {
  USE_SERIAL.println("JSON:" + json);
  DynamicJsonBuffer  jsonBuffer;
  JsonObject& root = jsonBuffer.parseObject(json);
  if (!root.success()) {
    USE_SERIAL.println("parseObject() failed");
    rjson = false;
  } else USE_SERIAL.println("parseObject() OK");
  
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

    USE_SERIAL.print("POWER:");
    USE_SERIAL.println(solax_wgrid);
    rjson = true;
  
}

void down_pwm() {
  dsConfig.P01_on = false;
  USE_SERIAL.println("DEACTIVATING PWM");
  while (solax_pwm > 0) {
     solax_pwm--;
     ledcWrite(1, solax_pwm);
  }
  USE_SERIAL.println("PWM DEACTIVATED"); 
}

/******** void callback***********************
   this function receive the info from mqtt
   suscriptionand print the infro that comes
   through
**********************************************/

void callback(char* topic, byte* payload, unsigned int length) {
  USE_SERIAL.print("MQTT Message arrived [");
  USE_SERIAL.print(topic);
  USE_SERIAL.print("] ");
  for (int i = 0; i < length; i++) {
    USE_SERIAL.print((char)payload[i]);
  }
  USE_SERIAL.println();
  if (strcmp(topic,"/solax/pwm")==0) {  // pwm control ON-OFF
       USE_SERIAL.println("Mqtt _ PWM control");
      if ((char)payload[0] == '1') {
        USE_SERIAL.println("PWM ACTIVATED");
        dsConfig.P01_on = true; 
      } else {
        dsConfig.P01_on = false;
        down_pwm();  
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
    int length = topublish.length();
    char bufferdata[length];
    topublish.toCharArray(bufferdata,length+1);
    //client.publish("/solax/pwm", bufferdata);
    
    client.publish(topic, bufferdata);
  } else {
    USE_SERIAL.print(F("failed, rc="));
    USE_SERIAL.print(client.state());
    USE_SERIAL.println(" try again in 5 seconds");
    delay(100);
  }
}


void reconnect() {
  // Loop until we're reconnected
  String mqtt_dir;
  if (!client.connected()) {
    USE_SERIAL.print(F("Attempting MQTT connection..."));
    // Attempt to connect
    if (client.connect("SodeClient", dsConfig.MQTT_user, dsConfig.MQTT_password)) {
      USE_SERIAL.println("connected");
      
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
      client.subscribe("solax/pwm");
    } else {
      USE_SERIAL.print(F("failed, rc="));
      //USE_SERIAL.print(client.stat());
      USE_SERIAL.println("Broker:" + (String)dsConfig.MQTT_broker);
      USE_SERIAL.println("User:" + (String)dsConfig.MQTT_user);
      USE_SERIAL.println("Pass:" + (String)dsConfig.MQTT_password);
      USE_SERIAL.println("Port:" + (String)dsConfig.MQTT_port);      
      client.disconnect();
      client.setServer(dsConfig.MQTT_broker, dsConfig.MQTT_port);
      client.setCallback(callback);
      USE_SERIAL.println(" try again in 5 seconds");
      // Wait 0.1 second before retrying
      delay(100);
    }
  }
}




void m1_com () {

  if (SerieEsp.available()) {
    String currentLine;
    currentLine = SerieEsp.readStringUntil('\n');
    if (currentLine.startsWith("###VERSION")){
      esp01_version=currentLine.substring(currentLine.indexOf("###VERSION")+14,currentLine.indexOf("$") );
    }
    if (currentLine.startsWith("###JSONERROR")){
      USE_SERIAL.println("-----M1: Error decodificando JSON");
    }
    if (currentLine.startsWith("###STATUS")){
      String buf;
      buf=currentLine.substring(currentLine.indexOf("###STATUS:")+10,currentLine.indexOf("$$$") );
      esp01_status = buf.toInt();
      if (esp01_status==0) SerieEsp.println("###SSID="+ String(dsConfig.ssid_esp01) +"$$$");
      else {
        serror=false;
        cerror=0;
      }
      
    }
    if (currentLine.startsWith("###PAYLOAD")){
      serror = false;
      esp01_payload=currentLine.substring(currentLine.indexOf("#PAYLOAD:")+9,currentLine.indexOf("$$$") );
      parseJson(esp01_payload);
      
    }
    if (currentLine.startsWith("##D M1: NOT CONNECT")){
      USE_SERIAL.println("-----M1: Mo conectado a Solax");
      serror = true;   
    }
    if (currentLine.startsWith("###HTTPCODE")){
      String buf;
      buf =currentLine.substring(currentLine.indexOf("###HTTPCODE:")+12,currentLine.indexOf("$$$") );
      httpcode = buf.toInt();
    }
    if (currentLine.startsWith("##D")){ // DEBUG
      USE_SERIAL.println(currentLine.substring(currentLine.indexOf("##D")+3,currentLine.indexOf("\n") ));
    }
  }

}

void v1_com(){
  HTTPClient http;
  if (dsConfig.wversion == 1 && counter==1500) {
     httpcode = -1;
     // configure traged server and url
     String buf = "http://"+ (String)dsConfig.solax_ip_v1 + "/api/realTimeData.htm";
     char bufferdata[buf.length() +1];
     buf.toCharArray(bufferdata,(buf.length() +1));
     http.begin(bufferdata); //HTTP
     // start connection and send HTTP header
     httpcode = http.GET();
     #if DEBUG
         USE_SERIAL.println("HTTPCODE ERROR: " + (String)httpcode);
     #endif
    if (httpcode < 0 || httpcode == 404)  cerror++; // Error in connection with Solax
    String Resp = http.getString();
    if(httpcode == HTTP_CODE_OK ) {
      parseJsonv1(Resp);
      serror=false;
    }
    http.end(); 
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

     // Relays
     pinMode(PIN_RL1,OUTPUT);
     pinMode(PIN_RL2,OUTPUT);
     pinMode(PIN_RL3,OUTPUT);
     pinMode(PIN_RL4,OUTPUT);
     digitalWrite(PIN_RL1, LOW);
     digitalWrite(PIN_RL2, LOW);
     digitalWrite(PIN_RL3, LOW);
     digitalWrite(PIN_RL4, LOW);
    // SERIAL

    USE_SERIAL.begin(115200);
    USE_SERIAL.println();
    USE_SERIAL.println();
    USE_SERIAL.println();

    SerieEsp.begin(115200,SERIAL_8N1, pin_rx, pin_tx);
    #if OLED
      display.clear();
      display.setTextAlignment(TEXT_ALIGN_CENTER);
      display.setFont(ArialMT_Plain_24);
      display.drawString(64, 0, ("openDS+"));
      display.setFont(ArialMT_Plain_16);
      display.drawString(64, 36, "Setup init...");
      display.display();
    #endif
    
    USE_SERIAL.println("Welcome to openDS+");
    for(uint8_t t = 4; t > 0; t--) {
        USE_SERIAL.printf("[SETUP] WAIT %d...\n", t);
        USE_SERIAL.flush();
        delay(1000);
    }
    pwm_man = false; // Manual control of outputs, default false

    // Read config
    USE_SERIAL.println("\nTesting EEPROM Library\n");
    if (!EEPROM.begin(sizeof(dsConfig))) {
      USE_SERIAL.println("Failed to initialise EEPROM");
      USE_SERIAL.println("Restarting...");
      delay(1000);
      ESP.restart();
    }
    EEPROM.get(0, dsConfig);
    if (dsConfig.eeinit != 04 ) {
      USE_SERIAL.println("Configuration Not Found, initializing");
      dsConfig.eeinit = 04;
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
      strcpy(dsConfig.ssid_esp01, "SOLAXX");
      strcpy(dsConfig.password_esp01, "");
      strcpy(dsConfig.solax_ip_v1, "192.168.0.100");
      dsConfig.dhcp = true;
      strcpy(dsConfig.ip, "192.168.3.99");
      strcpy(dsConfig.gw, "192.168.3.1");
      strcpy(dsConfig.mask, "255.255.255.0");
      strcpy(dsConfig.dns1, "208.67.222.222");
      strcpy(dsConfig.dns2, "208.67.220.220");
      dsConfig.pwm_min = -60;
      dsConfig.pwm_max = -90;
      dsConfig.P01_on = true;
      dsConfig.R01_man = false;
      dsConfig.R01_min = 9999;
      dsConfig.R02_man = false;
      dsConfig.R02_min = 9999;
      dsConfig.R03_man = false;
      dsConfig.R03_min = 9999;
      dsConfig.R04_man = false;
      dsConfig.R04_min = 9999;
      strcpy(dsConfig.R01_mqtt, "/solax/relay/1/cmnd/POWER");
      strcpy(dsConfig.R02_mqtt, "/solax/relay/2/cmnd/POWER");
      strcpy(dsConfig.R03_mqtt, "/solax/relay/3/cmnd/POWER");
      strcpy(dsConfig.R04_mqtt, "/solax/relay/4/cmnd/POWER");
      strcpy(dsConfig.password, "admin");
      EEPROM.put(0, dsConfig);
      EEPROM.commit();
    }
      
    #if OLED
      display.clear();
      display.setTextAlignment(TEXT_ALIGN_CENTER);
      display.setFont(ArialMT_Plain_24);
      display.drawString(64, 0, ("openDS+"));
      display.setFont(ArialMT_Plain_16);
      display.drawString(64, 36, "Connecting...");
      display.display();
    #endif

    // WIFI 
    USE_SERIAL.print("Conecting to Wifi.");
    WiFi.mode(WIFI_AP_STA);
    // Fixed IP
    if (dsConfig.dhcp == false) {
      IPAddress local_IP, gateway, subnet, primaryDNS, secondaryDNS;
      local_IP.fromString((String)dsConfig.ip);
      gateway.fromString((String)dsConfig.gw);
      subnet.fromString((String)dsConfig.mask);
      primaryDNS.fromString((String)dsConfig.dns1);
      secondaryDNS.fromString((String)dsConfig.dns2);
      
      if (!WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS)) {
      USE_SERIAL.println("Fixed IP -  Failed to configure");
      }
    }
    wifiMulti.addAP(dsConfig.login1, dsConfig.pass1);
    wifiMulti.addAP(dsConfig.login2, dsConfig.pass2);

    USE_SERIAL.print("\r\nWIFI:");
    int count = 10;
    while (wifiMulti.run() != WL_CONNECTED && count-- > 0) {
        delay(500);
        USE_SERIAL.print(".");
    }
              
    if(wifiMulti.run() == WL_CONNECTED) {
     
     #if OLED
      display.clear();
      display.drawString(64, 0,"CONNECTED:");
      display.drawString(64, 20,"IP ADDRESS :");
      display.drawString(64, 40, WiFi.localIP().toString());
      display.display();
      #endif
      USE_SERIAL.println("");
      USE_SERIAL.println("WiFi connected");
      USE_SERIAL.println("IP address: ");
      USE_SERIAL.println(WiFi.localIP());
      delay(2000);
     
    } else {
      werror = true;
      #if OLED
      display.setTextAlignment(TEXT_ALIGN_CENTER);
      display.clear();
      display.drawString(64, 0,"WIFI");
      display.drawString(64, 20,"ERROR");
      display.display();
     #endif
      USE_SERIAL.println("AP Not valid, SmartConfig actived, please run the app ");
      delay(5000);

    #if OLED
      display.clear();
      display.setTextAlignment(TEXT_ALIGN_CENTER);
      display.setFont(ArialMT_Plain_24);
      display.drawString(64, 0, ("openDS+"));
      display.setFont(ArialMT_Plain_16);
      display.drawString(64, 30, "WAITING FOR");
      display.drawString(64, 46, "SMARTCONFIG");
      display.display();
    #endif
    
      //WiFi.mode(WIFI_AP_STA);
      WiFi.beginSmartConfig();
      USE_SERIAL.println("Waiting for SmartConfig.");
      while (!WiFi.smartConfigDone()) {
          delay(500);
      USE_SERIAL.print(".");
      }
      delay(5000);
      USE_SERIAL.println("WiFi connected");
      USE_SERIAL.println("IP address: ");
      USE_SERIAL.println(WiFi.localIP());

      #if OLED
      display.clear();
      display.drawString(64, 0,"CONNECTED:");
      display.drawString(64, 20,"IP ADDRESS :");
      display.drawString(64, 40, WiFi.localIP().toString());
      display.display();
      #endif
      werror = false;
      delay(3000);
   
      // SAVE WIFI DATA TO EPROM
      {
      String buf = WiFi.SSID();
      char bufferdata[buf.length()+1];
      buf.toCharArray(bufferdata,(buf.length() +1));
      strcpy(dsConfig.login1,bufferdata );
      }
      {
      String buf = WiFi.psk();
      char bufferdata[buf.length()+1];
      buf.toCharArray(bufferdata,(buf.length() +1));
      strcpy(dsConfig.pass1,bufferdata );
      }
      EEPROM.put(0, dsConfig);
      EEPROM.commit(); 
      USE_SERIAL.println ("WIFI DATA SAVED!!!!");
      werror = false;
      ESP.restart();
    }
    
  
    // OUTPUTS
    pinMode(PIN_RL1, OUTPUT);
    pinMode(PIN_RL2, OUTPUT);
    pinMode(PIN_RL3, OUTPUT);
    pinMode(PIN_RL4, OUTPUT);
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

    // Init ESP01 Module M1 firmware
    if (dsConfig.wversion == 2) {
      USE_SERIAL.println("###VERSION");
      USE_SERIAL.println("###SSID="+String(dsConfig.ssid_esp01)+"$$$");
      delay(100);
      m1_com();
    }
    serror=true;
    // Watchdog implementation
    wdt = millis();
    xTaskCreatePinnedToCore(watchDog, "watchdog", 512, NULL, 1, NULL, 0);

}

void watchDog(void *pvParameters)
{
 #define WDtimeout 30000
  unsigned long wdc = millis();
  while (1) {
    //wdt
    if ((millis() - wdc) > 5000) {
      if ((millis() - wdt) > WDtimeout) {
        down_pwm();
        ESP.restart();
      }
      wdc = millis();
    }
  }
}

void relay_control_man() {
  // Active / deactive relays
  if (dsConfig.R01_man) {
    digitalWrite(PIN_RL1, HIGH);
  }
  if (dsConfig.R02_man) {
    digitalWrite(PIN_RL2, HIGH);
  }
  if (dsConfig.R03_man) {
    digitalWrite(PIN_RL3, HIGH);
  }
  if (dsConfig.R04_man) {
    digitalWrite(PIN_RL4, HIGH);
  }
  
}

uint16_t pwm_calc(uint16_t pwm_val) {
  // Calc value with sin and cos
  double value;
  if (pwm_val >= 90) {
    value = (( (1 + cos((180-pwm_val)*3.1416/180))) /2 ) * 65535 ;
  } else {
    value = (( sin(pwm_val*3.1416/180) /2 ) * 65535) ;
  }
  if (pwm_val == 180) value = 65535;
  return ((uint16_t)value);
}

void data_display() {
  
                 #if OLED 
                  String pro = String((solax_pwm * 100) / 180) + "%";
                  int progressbar = ((solax_pwm * 100) / 180);
                  display.clear();
                  display.setTextAlignment(TEXT_ALIGN_CENTER);
                  display.setFont(ArialMT_Plain_10);
                  display.drawString(64, 0, ("Solar --                 --  Grid"));
                  if (flash) display.drawString(64, 0, ( String(serror ? "S " : "S " ) + String(werror ? "W " : "W " ) + String(merror>1 ? "M " : "M  " ) ));
                  else  display.drawString(64, 0, (String(serror ? "_ " : "S " ) + String(werror ? "_ " : "W " ) + String(merror>1 ? "_ " : "M  " ) ));
                  if (flash) display.drawString(85, 0, (String(dsConfig.wversion)));
                  else display.drawString(85, 0, "v");
                  display.setTextAlignment(TEXT_ALIGN_LEFT);
                  display.setFont(ArialMT_Plain_24);
                  display.drawString(0, 12, (String)(int)solax_wsolar);
                  display.drawString(64, 12, (String)(int)solax_wgrid);
                   
                  display.setFont(ArialMT_Plain_10);
                  display.setTextAlignment(TEXT_ALIGN_CENTER);  // draw the percentage as String
                  if (serror) display.drawString(64, 38, WiFi.localIP().toString());
                  else display.drawProgressBar(0, 38, 120, 10, progressbar);    // draw the progress bar    
                  if (flash) display.drawString(64, 52,("PWM "+pro + " Relay: " + (digitalRead(PIN_RL1) ? "1 " : "1 " ) + (digitalRead(PIN_RL2) ? "2 " : "2 " ) + (digitalRead(PIN_RL3) ? "3 " : "3 " )+ (digitalRead(PIN_RL4) ? "4 " : "4 ")  ));
                  else display.drawString(64, 52,("PWM "+pro + " Relay: " + (digitalRead(PIN_RL1) ? "_ " : "1 " ) + (digitalRead(PIN_RL2) ? "_ " : "2 " ) + (digitalRead(PIN_RL3) ? "_ " : "3 " )+ (digitalRead(PIN_RL4) ? "_ " : "4 ")  ));
                  display.display();
                  flash = !flash;
                #endif  
}

void loop() {
    delay(1);
    wdt = millis();
    if (dsConfig.wversion == 2 ) m1_com();
    if (dsConfig.wversion == 1 ) v1_com();
       
    if (cerror >= 60) {
      USE_SERIAL.println("SOLAX ERROR: -- ERROR ---" + (String)cerror);
      down_pwm();
      ESP.restart();
    }
    if (counter==30000 && dsConfig.wversion == 2 ) { 
      SerieEsp.println("###STATUS");
      delay(1);
      counter=0;
    }

    if (counter>=30000 ) counter=0;
    
    if((wifiMulti.run() == WL_CONNECTED) && rjson) { 
        rjson = false;
        USE_SERIAL.println("MQTT ERROR: ----------------------------" + (String)merror);
        
        if (dsConfig.mqtt == true ) {
          if (!client.connected()) {
            reconnect();
            if (!client.connected()) {
              USE_SERIAL.print("Reconnecting MQTT\n");
              #if OLED 
              display.setTextAlignment(TEXT_ALIGN_CENTER);
              display.drawString(64, 38,"MQTT: Connecting...");
              display.display();
              #endif
              merror++;
              USE_SERIAL.print("\r\nMQTT service not work!!!!\n\r");
              if (merror >= 5 ) dsConfig.mqtt = false;
              
            }
          }
          else { merror=0; client.loop(); }
        }
        relay_control_man(); // Control de relays

        #if DEBUG
            USE_SERIAL.println("SERROR................................." + String(serror));
            USE_SERIAL.println("COUNTER ERROR:........................." + String(cerror));
        #endif
              
        if (serror) delay (10);
        if (cerror >= 10) serror = true;
        
        if (httpcode < 0 || httpcode == 404)  cerror++; // Error in connection with Solax

        if(httpcode >= 0) {
            if(httpcode == HTTP_CODE_OK ) {
                serror==false;
                cerror = 0;
                
                // Check pwm_output
                if (dsConfig.P01_on == true) {  
                  if ( solax_wgrid > dsConfig.pwm_min ) { // Default value is < 60 w from grid an pwm < 100
                    USE_SERIAL.println("PWM: SUBIENDO POTENCIA");
                    if  ( solax_wgrid > (dsConfig.pwm_max+250) && solax_pwm < 165 ) solax_pwm+= 15; // Increment 15 in range 250
                    else if  ( solax_wgrid > (dsConfig.pwm_max+120) && solax_pwm < 175 ) solax_pwm+= 5; // Increment 5 in range 120
                    if (solax_pwm >= 180) solax_pwm = 179; // 180 = 100%
                    solax_pwm += 1 ; //Increment value
                    ledcWrite(0, pwm_calc(solax_pwm));  // Write new pwm value
                   }
                  
                  else if  ( solax_wgrid < dsConfig.pwm_max )  { // Value is < 90 w from grid an pwm < 125
                    USE_SERIAL.println("PWM: BAJANDO POTENCIA");
                    if  ( solax_wgrid < (dsConfig.pwm_max-250)  && solax_pwm > 15) solax_pwm-= 15; // Decrement 15 in range 250
                    else if  ( solax_wgrid < (dsConfig.pwm_max-120)  && solax_pwm > 5) solax_pwm-= 5; // Decrement 5 in range 120
                    if (solax_pwm <= 0) solax_pwm = 1;
                    solax_pwm-= 1 ; //Decrement value 
                    ledcWrite(0, pwm_calc(solax_pwm));  // Write new pwm value
                    
                   }
                 }
                 
                // Stop relays if not power
                if (solax_pwm <= 5) {
                  if (digitalRead(PIN_RL4) && dsConfig.R04_man == false)  {
                    if (soff>=6) {
                      digitalWrite(PIN_RL4, LOW);
                      soff=0;
                    } else soff++;
                    if (dsConfig.mqtt == true && merror == 0) publisher(dsConfig.R04_mqtt,String(digitalRead(PIN_RL4) ? "ON" : "OFF" )); 
                  }
                  else if (digitalRead(PIN_RL3) && dsConfig.R03_man == false)  {
                    if (soff>=6) {
                      digitalWrite(PIN_RL3, LOW);
                      soff=0;
                    } else soff++;
                    if (dsConfig.mqtt == true && merror == 0) publisher(dsConfig.R03_mqtt,String(digitalRead(PIN_RL3) ? "ON" : "OFF" ));
                  }
                  else if (digitalRead(PIN_RL2) && dsConfig.R02_man == false)  {
                    if (soff>=6) {
                      digitalWrite(PIN_RL2, LOW);
                      soff=0;
                    } else soff++;
                    if (dsConfig.mqtt == true && merror == 0) publisher(dsConfig.R02_mqtt,String(digitalRead(PIN_RL2) ? "ON" : "OFF" ));
                  }
                  else if (digitalRead(PIN_RL1) && dsConfig.R01_man == false)  {
                    if (soff>=6) {
                      digitalWrite(PIN_RL1, LOW);
                      soff=0;
                    } else soff++;
                    if (dsConfig.mqtt == true && merror == 0) publisher(dsConfig.R01_mqtt,String(digitalRead(PIN_RL1) ? "ON" : "OFF" ));
                  }
                }
                
                if (solax_pwm >= 170) {
                  if (!digitalRead(PIN_RL1) ) {
                    // Start relay1
                    digitalWrite(PIN_RL1, HIGH);
                    if (dsConfig.mqtt == true && merror == 0) publisher(dsConfig.R01_mqtt,String(digitalRead(PIN_RL1) ? "ON" : "OFF" )); 
                  } else if (!digitalRead(PIN_RL2)) {
                    // Start relay2
                    digitalWrite(PIN_RL2, HIGH);
                    if (dsConfig.mqtt == true && merror == 0) publisher(dsConfig.R02_mqtt,String(digitalRead(PIN_RL2) ? "ON" : "OFF" ));                   
                  } else if (!digitalRead(PIN_RL3)) {
                    // Start relay3
                    digitalWrite(PIN_RL3, HIGH);
                    if (dsConfig.mqtt == true && merror == 0) publisher(dsConfig.R03_mqtt,String(digitalRead(PIN_RL3) ? "ON" : "OFF" ));                   
                  } else if (!digitalRead(PIN_RL4)) {
                    // Start relay4
                    digitalWrite(PIN_RL4, HIGH);                   
                    if (dsConfig.mqtt == true && merror == 0) publisher(dsConfig.R04_mqtt,String(digitalRead(PIN_RL4) ? "ON" : "OFF" ));
                  }  
                  
                }

                // Start defined power
                
                if (!digitalRead(PIN_RL1) && (solax_pwm*100/180) >= dsConfig.R01_min) {
                  // Start relay1
                  digitalWrite(PIN_RL1, HIGH);
                  if (dsConfig.mqtt == true && merror == 0) publisher(dsConfig.R01_mqtt,String(digitalRead(PIN_RL1) ? "ON" : "OFF" ));
                } else 
                if (!digitalRead(PIN_RL2) && (solax_pwm*100/180) >= dsConfig.R02_min) {
                  // Start relay2
                  digitalWrite(PIN_RL2, HIGH);
                  if (dsConfig.mqtt == true && merror == 0) publisher(dsConfig.R02_mqtt,String(digitalRead(PIN_RL2) ? "ON" : "OFF" )); 
                }  else
                if (!digitalRead(PIN_RL3) && (solax_pwm*100/180) >= dsConfig.R03_min) {
                  // Start relay3
                  digitalWrite(PIN_RL3, HIGH);
                  if (dsConfig.mqtt == true && merror == 0) publisher(dsConfig.R03_mqtt,String(digitalRead(PIN_RL3) ? "ON" : "OFF" )); 
                } else
                if (!digitalRead(PIN_RL4) && (solax_pwm*100/180) >= dsConfig.R04_min) {
                  // Start relay4
                  digitalWrite(PIN_RL4, HIGH);
                  if (dsConfig.mqtt == true && merror == 0) publisher(dsConfig.R04_mqtt,String(digitalRead(PIN_RL4) ? "ON" : "OFF" ));
                }
        
                // Publish mqtt values
                if (dsConfig.mqtt == true && merror == 0) {
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
                  publisher("/solax/pwm",(String)(solax_pwm*100/180));              
                } 

                // Check Relay control

                String pro = String((solax_pwm * 100) / 180) + "%";
                int progressbar = ((solax_pwm * 100) / 180);
  
                USE_SERIAL.print("\r\nPWM VALUE: % ");
                USE_SERIAL.println(pro);
                USE_SERIAL.print("\r\nPWM VALUE: ");
                USE_SERIAL.println(solax_pwm);
                
            }
        } else {
            USE_SERIAL.println("M1: Error en peticion de datos");
            #if OLED
              display.setTextAlignment(TEXT_ALIGN_CENTER);
              display.drawString(64, 52,"Error en red - 01");
              display.display();
            #endif    
        }

 
     } // End If Wifi

     //Screen
    #if OLED
    if ((millis() - src) > 500) {
      data_display();
      src = millis();
    }
    #endif
    // Http server
      
      WiFiClient hclient = server.available();
      if (hclient) {                             // if you get a client,
        USE_SERIAL.println("New Client.");           // print a message out the serial port
        String currentLine = "";                // make a String to hold incoming data from the client
        unsigned int cont = 0;
        unsigned int page = 0;
        unsigned int post=0;
        int post_length = 0;
        boolean mobile = false; 
        while (hclient.connected()) {            // loop while the client's connected
            if (hclient.available()) {             // if there's bytes to read from the client,
              char c = hclient.read();             // read a byte, then
              Serial.write(c);                    // print it out the serial monitor
                if (c == '\n') {                    // if the byte is a newline character

                  // if the current line is blank, you got two newline characters in a row.
                  // that's the end of the client HTTP request, so send a response:
                  #if DEBUG
                    USE_SERIAL.println ("CURRENTLINE:" + currentLine);
                  #endif
                  if (currentLine.indexOf("Mobile") > 2) mobile = true;
                  
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
                  if (currentLine.startsWith("GET /?reboot")) {
                    page = 6;
                  }
                  if (currentLine.startsWith("GET /?save")) {
                    page = 5;
                  }
                  if (currentLine.startsWith("GET /?api")) {
                    page = 7;
                  }
                  if (currentLine.startsWith("GET /selectversion?data")) {
                    page = 0;
                    {
                      String buf = currentLine.substring(currentLine.indexOf("n?data=")+7,currentLine.indexOf(" HTTP") );
                      dsConfig.wversion=buf.toInt();
                      EEPROM.put(0, dsConfig);
                      EEPROM.commit(); 
                    }
                  }

                  if (currentLine.length() == 0) {

                    if (post!=0) {
                      currentLine = "";
                      while(post_length-- > 0)
                      {
                        char c = hclient.read();
                        currentLine += c;
                      }
                      post=0;
                      post_length=0;
                      #if DEBUG
                        USE_SERIAL.println ("CURRENTLINE POST:" + currentLine);
                      #endif

                      // SALIDAS CONFIG PWM
                      if (currentLine.indexOf("Guardarpwm=") > 2) {
                        if (currentLine.indexOf("mactive=Yes") > 1) {
                          dsConfig.P01_on = true;
                        } else {
                          dsConfig.P01_on = false;
                          down_pwm();
                        }
                                  
                        {
                        String buf = currentLine.substring(currentLine.indexOf("pwmmin=")+7,currentLine.indexOf("&pwmmax") );
                        dsConfig.pwm_min=buf.toInt();
                        }
                        {
                        String buf = currentLine.substring(currentLine.indexOf("pwmmax=")+7,currentLine.indexOf("&Guardar") );
                        dsConfig.pwm_max=buf.toInt();
                        }                     
                        page = 4;
                      } 

                      // SALIDAS CONFIG AUTO RELAY
                      else if (currentLine.indexOf("uardarrelay=") > 1) {        
                        {
                        String buf = currentLine.substring(currentLine.indexOf("r01min=")+7,currentLine.indexOf("&r02min") );
                        dsConfig.R01_min=buf.toInt();
                        USE_SERIAL.println("VALUE "+ buf );
                        }
                        {
                        String buf = currentLine.substring(currentLine.indexOf("r02min=")+7,currentLine.indexOf("&r03min") );
                        dsConfig.R02_min=buf.toInt();
                        USE_SERIAL.println("VALUE "+ buf );
                        }
                        {
                        String buf = currentLine.substring(currentLine.indexOf("r03min=")+7,currentLine.indexOf("&r04min") );
                        dsConfig.R03_min=buf.toInt();
                        USE_SERIAL.println("VALUE "+ buf );
                        }
                        {
                        String buf = currentLine.substring(currentLine.indexOf("r04min=")+7,currentLine.indexOf("&Guardarrelay") );
                        dsConfig.R04_min=buf.toInt();
                        USE_SERIAL.println("VALUE "+ buf );
                        }
                        page = 4;
                      }

                      // SALIDAS CONFIG MAN
                      else if (currentLine.indexOf("ardarman=") > 1) {
                        dsConfig.R01_man = false;
                        dsConfig.R02_man = false;
                        dsConfig.R03_man = false;
                        dsConfig.R04_man = false;
                        if (currentLine.indexOf("01active=Yes") >= 1) {
                          dsConfig.R01_man = true;
                        } else {
                          dsConfig.R01_man = false;
                        }         
                        if (currentLine.indexOf("02active=Yes") >= 1) {
                          dsConfig.R02_man = true;
                        } else {
                          dsConfig.R02_man = false;
                        }         
                        if (currentLine.indexOf("03active=Yes") >= 1) {
                          dsConfig.R03_man = true;
                        } else {
                          dsConfig.R03_man = false;
                        }         
                        if (currentLine.indexOf("04active=Yes") >= 1) {
                          dsConfig.R04_man = true;
                        } else {
                          dsConfig.R04_man = false;
                        }         
                        page = 4;
                      }
                      
                      
                      // MQTT CONF
                      else if (currentLine.indexOf("mqttuser=") > 1) {
                        dsConfig.mqtt = false;            
                        {
                        String buf = currentLine.substring(currentLine.indexOf("tport=")+6,currentLine.indexOf("&mqttr1") );
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
                        { // Mqtt R1
                        String buf = currentLine.substring(currentLine.indexOf("qttr1=")+6,currentLine.indexOf("&mqttr2") );
                        char bufferdata[buf.length()];
                        buf.replace("%2F","\/");
                        buf.toCharArray(bufferdata,(buf.length() +1));
                        strcpy(dsConfig.R01_mqtt,bufferdata );
                        }
                        { // Mqtt R2
                        String buf = currentLine.substring(currentLine.indexOf("qttr2=")+6,currentLine.indexOf("&mqttr3") );
                        char bufferdata[buf.length()];
                        buf.replace("%2F","\/");
                        buf.toCharArray(bufferdata,(buf.length() +1));
                        strcpy(dsConfig.R02_mqtt,bufferdata );
                        }
                        { // Mqtt R3
                        String buf = currentLine.substring(currentLine.indexOf("qttr3=")+6,currentLine.indexOf("&mqttr4") );
                        char bufferdata[buf.length()];
                        buf.replace("%2F","\/");
                        buf.toCharArray(bufferdata,(buf.length() +1));
                        strcpy(dsConfig.R03_mqtt,bufferdata );
                        }
                        { // Mqtt R4
                        String buf = currentLine.substring(currentLine.indexOf("qttr4=")+6,currentLine.indexOf("&Guardar") );
                        char bufferdata[buf.length()];
                        buf.replace("%2F","\/");
                        buf.toCharArray(bufferdata,(buf.length() +1));
                        strcpy(dsConfig.R04_mqtt,bufferdata );
                        }
                        if (currentLine.substring(currentLine.indexOf("ctive=")+6,currentLine.indexOf("&broker") ) == "Yes") {
                          dsConfig.mqtt = true;
                          if (!client.connected()) {
                            reconnect();
                          }
                        }
                        
                        page = 4;
                      }

                      // WIFI CONFIG POST
                      else if (currentLine.indexOf("wifip1=") > 1) {
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
                        String buf = currentLine.substring(currentLine.indexOf("wifis=")+6,currentLine.indexOf("&ip=") );
                         if (dsConfig.wversion == 2 || dsConfig.wversion == 3) {
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

                        {
                        String buf = currentLine.substring(currentLine.indexOf("ip=")+3,currentLine.indexOf("&gw=") ); 
                        char bufferdata[buf.length()];
                        buf.toCharArray(bufferdata,(buf.length() +1));
                        strcpy(dsConfig.ip,bufferdata );
                        }
                        {
                        String buf = currentLine.substring(currentLine.indexOf("gw=")+3,currentLine.indexOf("&mask=") ); 
                        char bufferdata[buf.length()];
                        buf.toCharArray(bufferdata,(buf.length() +1));
                        strcpy(dsConfig.gw,bufferdata );
                        }
                        {
                        String buf = currentLine.substring(currentLine.indexOf("mask=")+5,currentLine.indexOf("&dns1=") ); 
                        char bufferdata[buf.length()];
                        buf.toCharArray(bufferdata,(buf.length() +1));
                        strcpy(dsConfig.mask,bufferdata );
                        }
                        {
                        String buf = currentLine.substring(currentLine.indexOf("dns1=")+5,currentLine.indexOf("&dns2=") ); 
                        char bufferdata[buf.length()];
                        buf.toCharArray(bufferdata,(buf.length() +1));
                        strcpy(dsConfig.dns1,bufferdata );
                        }
                        {
                        String buf;
                        if (currentLine.substring(currentLine.indexOf("dhcp=")+5,currentLine.indexOf("&Guardar") ) == "Yes") {
                          dsConfig.dhcp = true;
                          buf = currentLine.substring(currentLine.indexOf("dns2=")+5,currentLine.indexOf("&dhcp=") );
                          USE_SERIAL.println("BUFFER 1: " + buf);
                        } else {
                          dsConfig.dhcp = false;
                          buf = currentLine.substring(currentLine.indexOf("dns2=")+5,currentLine.indexOf("&Guardar=") );
                          USE_SERIAL.println("BUFFER 2: " + buf);
                        }
                        char bufferdata[buf.length()];
                        buf.toCharArray(bufferdata,(buf.length() +1));
                        strcpy(dsConfig.dns2,bufferdata );
                        }
                        page = 5;

                      }     
                      currentLine ="";
                    } // End POST

                                       
                    
                    hclient.print("HTTP/1.1 200 OK\r\n"); //send new page
                    hclient.print("Content-Type: text/html\r\n\r\n"); 
                    hclient.print("<!DOCTYPE HTML>\r\n");
                    hclient.print("<HTML>\r\n");//html tag
                    hclient.print("<HEAD>\r\n");

                    hclient.println("<link rel=\"icon\" href=\"data:,\">");
                    hclient.println("<style>");

                    if (mobile) hclient.println (".myform { width:80%;padding:30px;margin:40px asuto;background: #FFF; border-radius: 10px; -webkit-border-radius:10px; -moz-border-radius: 10px; box-shadow: 0px 0px 10px rgba(0, 0, 0, 0.13); -moz-box-shadow: 0px 0px 10px rgba(0, 0, 0, 0.13); -webkit-box-shadow: 0px 0px 10px rgba(0, 0, 0, 0.13); }");
                    else hclient.println (".myform { width:450px;padding:30px;margin:40px auto;background: #FFF; border-radius: 10px; -webkit-border-radius:10px; -moz-border-radius: 10px; box-shadow: 0px 0px 10px rgba(0, 0, 0, 0.13); -moz-box-shadow: 0px 0px 10px rgba(0, 0, 0, 0.13); -webkit-box-shadow: 0px 0px 10px rgba(0, 0, 0, 0.13); }");
                    
                    hclient.println (".myform .inner-wrap{ padding: 30px;background: #F8F8F8; border-radius: 6px; margin-bottom: 15px; }");
                    hclient.println (".myform h1{background: #2A88AD;padding: 20px 30px 15px 30px;margin: -30px -30px 30px -30px;border-radius: 10px 10px 0 0;-webkit-border-radius: 10px 10px 0 0;-moz-border-radius: 10px 10px 0 0;color: #fff;text-shadow: 1px 1px 3px rgba(0, 0, 0, 0.12);font: normal 30px Arial, Helvetica;-moz-box-shadow: inset 0px 2px 2px 0px rgba(255, 255, 255, 0.17);-webkit-box-shadow: inset 0px 2px 2px 0px rgba(255, 255, 255, 0.17);box-shadow: inset 0px 2px 2px 0px rgba(255, 255, 255, 0.17);border: 1px solid #257C9E; }");
                    hclient.println (".myform h1 > span{display: block;margin-top: 2px;font: 13px Arial, Helvetica, sans-serif;}");
                    hclient.println (".myform label{display: block;font: 13px Arial, Helvetica, sans-serif;color: #888;margin-bottom: 15px;}");
                    hclient.println (".myform input[type='text'], .myform input[type='date'], .myform input[type='datetime'], .myform input[type='email'], .myform input[type='number'], .myform input[type='search'], .myform input[type='time'], .myform input[type='url'], .myform input[type='password'], .myform textarea, .myform select {display: block; box-sizing: border-box; -webkit-box-sizing: border-box; -moz-box-sizing: border-box; width: 100%; padding: 8px; border-radius: 6px; -webkit-border-radius:6px; -moz-border-radius:6px; border: 2px solid #fff; box-shadow: inset 0px 1px 1px rgba(0, 0, 0, 0.33); -moz-box-shadow: inset 0px 1px 1px rgba(0, 0, 0, 0.33); -webkit-box-shadow: inset 0px 1px 1px rgba(0, 0, 0, 0.33); }");
                    hclient.println (".two-fields input { width:20% !important; }");
                    if (mobile) hclient.println (".myform .section{font: normal 30px Arial, Helvetica;color: #2A88AD;margin-bottom: 5px;}");
                    else hclient.println (".myform .section{font: normal 20px Arial, Helvetica;color: #2A88AD;margin-bottom: 5px;}");
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
                    if (dsConfig.wversion == 2 || dsConfig.wversion == 3) {
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

                    // Red
                    hclient.println ("<div class=\"section\"><span>3</span>Conexi&oacuten de red</div>");
                    hclient.println ("<div class=\"inner-wrap\">");
                    
                    hclient.println ("<label>Direcci&oacuten IP <input type=\"text\" value=\""+String(dsConfig.ip)+"\" name=\"ip\"/></label>");
                    hclient.println ("<label>Puerta de enlace <input type=\"text\" value=\""+String(dsConfig.gw)+"\" name=\"gw\"/></label>");
                    hclient.println ("<label>M&aacutescara de red <input type=\"text\" value=\""+String(dsConfig.mask)+"\" name=\"mask\"/></label>");
                    hclient.println ("<label>Servidor de nombres 1 <input type=\"text\" value=\""+String(dsConfig.dns1)+"\" name=\"dns1\"/></label>");
                    hclient.println ("<label>Servidor de nombres 2 <input type=\"text\" value=\""+String(dsConfig.dns2)+"\" name=\"dns2\"/></label>");
                    {
                    String tmp = "";
                    if (dsConfig.dhcp == true) tmp = " checked ";
                    hclient.println ("<label>Direcci&oacuten DHCP <input type=\"checkbox\" value=\"Yes\" name=\"dhcp\" "+ tmp +"/></label>");
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
                    USE_SERIAL.println("CONFIG DATA...................");
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
                    hclient.println ("<label>Tema R1 <input type=\"textname\" value=\""+String(dsConfig.R01_mqtt)+"\" name=\"mqttr1\"/></label>");
                    hclient.println ("<label>Tema R2 <input type=\"textname\" value=\""+String(dsConfig.R02_mqtt)+"\" name=\"mqttr2\"/></label>");
                    hclient.println ("<label>Tema R3 <input type=\"textname\" value=\""+String(dsConfig.R03_mqtt)+"\" name=\"mqttr3\"/></label>");
                    hclient.println ("<label>Tema R4 <input type=\"textname\" value=\""+String(dsConfig.R04_mqtt)+"\" name=\"mqttr4\"/></label>");
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
                    USE_SERIAL.println("RELAY DATA...................");
    
                    hclient.println ("</head>");
                    hclient.println ("<body>");
                    hclient.println ("<div class=\"myform\">");
                    hclient.println ("<h1>openDS+ #Configuraci&oacuten Salidas<span>Derivador de excedentes para Solax X1</span></h1>");
                    // action=\"/proc\"
                    hclient.println ("<form method=\"post\">");
                    hclient.println ("<div class=\"section\"><span>1</span>Configuraci&oacuten Dimmer PWM</div>");
                    hclient.println ("<div class=\"inner-wrap\">");
                    {
                    String tmp = "";
                    if (dsConfig.P01_on == true) tmp = " checked ";
                    hclient.println ("<label>PWM Dimmer Activo <input type=\"checkbox\" value=\"Yes\" name=\"pwmactive\" "+ tmp +"/></label>");
                    }
                    hclient.println ("<label>Consumo de red, valor m&iacutenimo <input type=\"textname\" value=\""+String(dsConfig.pwm_min)+"\" name=\"pwmmin\"/></label>");
                    hclient.println ("<label>Consumo de red, valor m&aacuteximo <input type=\"textname\" value=\""+String(dsConfig.pwm_max)+"\" name=\"pwmmax\"/></label>");
                    // End    
                    hclient.println ("<div class=\"button-section\">");
                    hclient.println ("  <input type=\"submit\" name=\"Guardarpwm\" value=\"Guardar\"/>");
                    hclient.println ("  <input type=\"button\" name=\"cancel\" value=\"Cancelar\" onClick=\"window.location.href=\'/\'\">");
                    hclient.println ("</div>");
                    hclient.println ("</div>");
                    hclient.println ("</form>");

                    hclient.println ("<form method=\"post\">");
                    hclient.println ("<div class=\"section\"><span>2</span>Configuraci&oacuten Salidas Auto</div>");
                    hclient.println ("<div class=\"inner-wrap\">");

                    hclient.println ("<div class=\"two-fields\"><label>R1 Inicio desde <input type=\"textname\" value=\""+String(dsConfig.R01_min)+"\" name=\"r01min\"/> % sobre PWM</label></div>");
                    hclient.println ("<div class=\"two-fields\"><label>R2 Inicio desde <input type=\"textname\" value=\""+String(dsConfig.R02_min)+"\" name=\"r02min\"/> % sobre PWM + R1</label></div>");
                    hclient.println ("<div class=\"two-fields\"><label>R3 Inicio desde <input type=\"textname\" value=\""+String(dsConfig.R03_min)+"\" name=\"r03min\"/> % sobre PWM + R2</label></div>");
                    hclient.println ("<div class=\"two-fields\"><label>R4 Inicio desde <input type=\"textname\" value=\""+String(dsConfig.R04_min)+"\" name=\"r04min\"/> % sobre PWM + R3</label></div>");

                    // End    
                    hclient.println ("<div class=\"button-section\">");
                    hclient.println ("  <input type=\"submit\" name=\"Guardarrelay\" value=\"Guardar\"/>");
                    hclient.println ("  <input type=\"button\" name=\"cancel\" value=\"Cancelar\" onClick=\"window.location.href=\'/\'\">");
                    hclient.println ("</div>");
                    hclient.println ("</div>");
                    hclient.println ("</form>");

                    // Control manual de encendido 
                    hclient.println ("<form method=\"post\">");
                    hclient.println ("<div class=\"section\"><span>3</span>Configuraci&oacuten Manual</div>");
                    hclient.println ("<div class=\"inner-wrap\">");
                    {
                    String tmp = "";
                    if (dsConfig.R01_man == true) tmp = " checked ";
                    hclient.println ("<label>Rel&eacute 1 Activo <input type=\"checkbox\" value=\"Yes\" name=\"R01active\" "+ tmp +"/>");
                    }
                    {
                    String tmp = "";
                    if (dsConfig.R02_man == true) tmp = " checked ";
                    hclient.println ("  Rel&eacute 2 Activo <input type=\"checkbox\" value=\"Yes\" name=\"R02active\" "+ tmp +"/></label>");
                    }
                    {
                    String tmp = "";
                    if (dsConfig.R03_man == true) tmp = " checked ";
                    hclient.println ("<label>Rel&eacute 3 Activo <input type=\"checkbox\" value=\"Yes\" name=\"R03active\" "+ tmp +"/>");
                    }
                    {
                    String tmp = "";
                    if (dsConfig.R04_man == true) tmp = " checked ";
                    hclient.println ("  Rel&eacute 4 Activo <input type=\"checkbox\" value=\"Yes\" name=\"R04active\" "+ tmp +"/></label>");
                    }
                    // End    
                    hclient.println ("<div class=\"button-section\">");
                    hclient.println ("  <input type=\"submit\" name=\"Guardarman\" value=\"Guardar\"/>");
                    hclient.println ("  <input type=\"button\" name=\"cancel\" value=\"Cancelar\" onClick=\"window.location.href=\'/\'\">");
                    hclient.println ("</div>");
                    hclient.println ("</div>");
                    hclient.println ("</form>");
                    // End Manual control
                    
                    hclient.println ("</div>");
                    
                   } else if (page == 4 || page == 5 || page == 6 ) { // AVISOS
                                      ////////////////////////////////Avisos///////////////////////
                    hclient.println ("<meta http-equiv='refresh' content='30'/>\r\n");
                    hclient.println ("</head>");
                    hclient.println ("<body>");
                    hclient.println ("<form method=\"get\">");
                    hclient.println ("<div class=\"myform\">");
                    hclient.println ("<h1>openDS+<span>Derivador de excedentes para Solax X1 V"+String(dsConfig.wversion)+"</span></h1>");
                   
                    hclient.println ("<div class=\"section\"><span>0</span>INFORMACION</div>");
                    hclient.println ("<div class=\"inner-wrap\">");
                    if ( page == 5) hclient.println ("<label>Reiniciando el dispositivo</label>");
                    if ( page == 4) hclient.println ("<label>Configuraci&oacuten guardada</label>");
                    hclient.println ("<div class=\"button-section\">");
                    hclient.println ("  <input type=\"button\" name=\"cancel\" value=\"Volver\" onClick=\"window.location.href=\'/\'\">");
                    hclient.println ("</div>");
                    hclient.println ("</div>");
                    hclient.println ("</form>"); 
                   
                   } else if (page == 7 ) { // API request
                        hclient.println ("{\"Data\":[" + String(httpcode) + "," +String(solax_pv1c) + "," + String(solax_pv2c) + "," + String(solax_pv1v) + "," + String(solax_pv2v) + "," + String(solax_pw1) + "," + String(solax_pw2) + "," + String(solax_gridv) + "," + String(solax_wsolar) + "," + String(solax_wtoday) + "," + String(solax_wgrid) + "," + String(solax_wtogrid) + "]}");
                  } else {
                                       
                    ////////////////////////////////Main Page///////////////////////
                    hclient.println ("<meta http-equiv='refresh' content='30'/>\r\n");
                    hclient.println ("</head>");
                    hclient.println ("<body>");
                    hclient.println ("<script>");
                    hclient.println ("function run() {");
                    hclient.println ("var Version = document.getElementById(\"version\").value;");
                    hclient.println ("var Url = \"selectversion?data=\" + Version;");
                    hclient.println ("xmlHttp = new XMLHttpRequest(); ");
                    hclient.println ("xmlHttp.open( \"GET\", Url, true );");
                    hclient.println ("xmlHttp.send( null ); }");
                    hclient.println ("</script></form>");
                    
                    hclient.println ("<form method=\"get\">");
                    hclient.println ("<div class=\"myform\">");
                    hclient.println ("<h1>openDS+");
                    hclient.println ("<form name=\"form\"><select style=\"width:100px; float: right;\" onchange=\"run()\" id=\"version\"> <option value=\"1\""+ String((dsConfig.wversion==1) ? " selected=\"selected\" " : " ") + ">Wifi v1</option> <option value=\"2\"" + String((dsConfig.wversion==2) ? " selected=\"selected\" " : " ") + ">Wifi v2</option></select></form>");
                    hclient.println ("<span>Derivador de excedentes para Solax X1 V"+String(dsConfig.wversion)+"</span></h1>");
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
                    hclient.println ("<label>Energ&iacute;a Diaria Solar: <input type=\"textname=\" value=\"" +String(solax_wtoday) + "\" disabled/>KWh</label>");
                    hclient.println ("<label>Potencia String 1: <input type=\"textname=\" value=\"" +String(solax_pw1) + " (" +String(solax_pv1v) + "V) " + "(" +String(solax_pv1c) + "A) "+ "\" disabled/>W</label>");
                    hclient.println ("<label>Potencia String 2: <input type=\"textname=\" value=\"" +String(solax_pw2) + " (" +String(solax_pv2v) + "V) " + "(" +String(solax_pv2c) + "A) " + "\" disabled/>W</label>");
                    hclient.println ("<label>PWM: <input type=\"textname=\" value=\"" +String((solax_pwm * 100) / 180) + "\" disabled/>%</label>");
                    hclient.println ("<label>R1:<b>"+String(digitalRead(PIN_RL1) ? "ON " : "OFF " ) + "</b>R2<b>:"+String(digitalRead(PIN_RL2) ? "ON " : "OFF " ) + "</b>R3:<b>"+String(digitalRead(PIN_RL3) ? "ON " : "OFF " ) + "</b>R4:<b>"+String(digitalRead(PIN_RL4) ? "ON " : "OFF " ) + "</b></label>");
                    hclient.println ("</div>");
                    hclient.println ("<div class=\"button-section\">");
                    hclient.println ("  <input type=\"button\" name=\"cnet\" value=\"Red\" onClick=\"window.location.href=\'/?cnet\'\">");
                    hclient.println ("  <input type=\"button\" name=\"config\" value=\"Config\" onClick=\"window.location.href=\'/?config\'\">");
                    hclient.println ("  <input type=\"button\" name=\"relay\" value=\"Salidas\" onClick=\"window.location.href=\'/?relay\'\">");
                    hclient.println ("  <input type=\"button\" name=\"reboot\" value=\"Reboot\" onClick=\"window.location.href=\'/?reboot\'\">");
                    hclient.println ("</div>");
                    hclient.println ("</div>");
                    hclient.println ("</form>");
                   }
                    hclient.println ("</body> ");
                    
                    hclient.println ("</HTML>");
                    
                    // The HTTP response ends with another blank line:
                    hclient.println();
                    hclient.stop();
                    
                    if (page == 4 || page == 5) {
                      EEPROM.put(0, dsConfig);
                      EEPROM.commit(); 
                      USE_SERIAL.println ("DATA SAVED!!!!");
                       if ( page == 5) { 
                        USE_SERIAL.println ("RESTARTING IN 5 SEC !!!!");
                        byte tcont=6;
                        while( tcont-- >0 ) {
                          #if OLED
                            display.setTextAlignment(TEXT_ALIGN_CENTER);
                            display.clear();
                            display.drawString(64, 0,"DATA SAVED");
                            display.drawString(64, 14,"RESTARTING");
                            display.drawString(64, 28, "IN "+(String)cont+" SECONDS");
                            display.display();
                          #endif
                           USE_SERIAL.print (".."+(String)tcont);
                           delay(1000);
                      }
                    }
                   }
                    if ( page == 5 || page == 6) { 
                      down_pwm();
                      ESP.restart();
                    }
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
    USE_SERIAL.println("Client Disconnected.");
   }
   delay(1);
   counter++;
} // End loop
