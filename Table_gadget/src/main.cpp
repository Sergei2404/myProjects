 /*
  lcd sda - d4
      scl - d3
      photores - d2
      lenta d5
      temp/hum d6
  */


#include "EspMQTTClient.h"

//-------------------------------------------------------------button
#include "GyverButton.h"
GButton myButt(16, LOW_PULL, NORM_OPEN);       //пин кнопки
int butcounter; 

//---------------------------------------------------------------OTA
#include <ArduinoOTA.h>
#include <ESP8266mDNS.h>

//---------------------------------------------------------------LED STRIP
#ifdef ESP32
#pragma message(THIS EXAMPLE IS FOR ESP8266 ONLY!)
#error Select ESP8266 board.
#endif
#include "FastLED.h"          // библиотека для работы с лентой

#define LED_COUNT 60          // число светодиодов в кольце/ленте
#define LED_DT 5             // пин, куда подключен DIN ленты
int max_bright = 150;         // максимальная яркость (0 - 255)
int BOTTOM_INDEX = 0;        // светодиод начала отсчёта
int TOP_INDEX = int(LED_COUNT / 2);
int EVENODD = LED_COUNT % 2;
int idex = 0;                //-LED INDEX (0 to LED_COUNT-1
int ihue = 0;                //-HUE (0-255)
struct CRGB leds[LED_COUNT];
int ledsX[LED_COUNT][3];     //-ARRAY FOR COPYING WHATS IN THE LED STRIP CURRENTLY (FOR CELL-AUTOMATA, MARCH, ETC)
int bright, color1;
int RGBset[3];

//---------------------------------------------------------------TIME
#include <NTPClient.h>
#include <WiFiUdp.h>
const long utcOffsetInSeconds = 3600;
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", utcOffsetInSeconds);
bool daynight=true;

//---------------------------------------------------------------Display
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
LiquidCrystal_I2C lcd(0x27, 20, 4);

//----------------------------------------------------------------BME280
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#define SEALEVELPRESSURE_HPA (1013.25)
Adafruit_BME280 bme;

//---------------------------------------------------------------SENSOR
const int trigPin = 5;  //D4
const int echoPin = 13;  //D3
// defines variables
long duration;
int distance;
int thisdelay = 20;
int lastdist=0;
const int SENSPIN = 4;  // photoresitor
int sensor; //photoresistor

//---------------------------------------------------------------Default off time
int offhour=0, offminutes=0;

//--------------------------------------------------------------MICROPHONE
int noise=0;

EspMQTTClient client( //-------------------------------------MQTT CONNECTION
    "", // name wifi
    "",   // pass wifi
    "192.168.1.75",  // MQTT Broker server ip
    "______",   // login Can be omitted if not needed
    "______",   //  pass Can be omitted if not needed
    "Esp",     // Client name that uniquely identify your device
    1883              // The MQTT port, default to 1883. this line can be omitted
);


int Lmode =3;    // default mode while enable

void onConnectionEstablished() //--------------------------------------MQTT GET INFO
{
      client.subscribe("Lenta/brightness", [](const String & payload) {
      bright= payload.toInt();
      });
      client.subscribe("lenta/off/hour", [](const String & payload) {
      offhour= payload.toInt();
      });
      client.subscribe("lenta/off/minutes", [](const String & payload) {
      offminutes= payload.toInt();
      });
      client.subscribe("Display/state", [](const String & payload) {
      daynight= payload.toInt();
      });
      client.subscribe("Lenta/mode", [](const String & payload) {
      Lmode= payload.toInt();
      Serial.println(payload);
      });
      client.subscribe("Lenta/color", [](const String & payload) {
        int j=0,temp;  
        for(int j=0;j<3;j++)
          {RGBset[j]=0;}
        for(unsigned int i=0;i<payload.length();i++){
          temp = payload[i] - '0';
        if (temp>=0)
        {
          RGBset[j]= (RGBset[j] *10) +temp;    
        }else{j++;}
        }
        for(j=0;j<3;j++)
        {
          Serial.println(RGBset[j]);
        }
        Lmode=6;
        if(RGBset[0]==255 && RGBset[1]==167 && RGBset[2]==89)
          {Lmode=3;}
        if(RGBset[0]==255 && RGBset[1]==217 && RGBset[2]==0)
          {Lmode=2;}
        if(RGBset[0]==124 && RGBset[1]==84 && RGBset[2]==255)
          {Lmode=1;}
        });
}


void enabling(){
  int f=0,s=200,m=0;
  for (int j = 0 ; j < LED_COUNT/2; j++ ) {
    if(j<=10){
      f=j*20;
    }else if(j>=20)
    {
      m=m+20;
    }
    else{
      s=s-20;
    }
    leds[j].setRGB( s, m, f);
    leds[((LED_COUNT)-j)].setRGB( s, f, m);
    LEDS.show();
    delay(110);
  }
}

void presenza(){
  int sumdist=0;
  for(int k=0;k<3;k++)
     {
        digitalWrite(trigPin, LOW);
        delayMicroseconds(2);
        digitalWrite(trigPin, HIGH);
        delayMicroseconds(10);
        digitalWrite(trigPin, LOW);
        duration = pulseIn(echoPin, HIGH);
        distance= duration*0.034/2;
        sumdist=sumdist+distance;
     }
  sumdist=sumdist/3;
  if(sumdist>lastdist+3 || sumdist<lastdist-3)
    {
      Serial.print("Distance: ");
      Serial.println(sumdist);
      lastdist=sumdist;
    }
  if((sumdist<40 && sumdist >3) || sumdist>130)
    {
      Lmode=3;
      enabling();
      daynight=1;
      // LEDS.setBrightness(i);
    }
}

int timer=0;
bool timerON=0;

void send_temp(){
  bme.begin(0x76);
     float temperature = bme.readTemperature();
     client.publish("temp", String(temperature,1).c_str(), true);
     float humidity = bme.readHumidity();
     int hum =(int)humidity;
     client.publish("humidity", String(hum).c_str(), true);
      lcd.setCursor(0, 1);
      lcd.print("Temperature:");
      lcd.setCursor(13, 1);
      lcd.print(temperature,1);
      lcd.setCursor(18, 1);
      lcd.print("C");
      lcd.setCursor(0, 2);
      lcd.print("Humidity:");
      lcd.setCursor(13, 2);
      lcd.print("       ");
      lcd.setCursor(13, 2);
      lcd.print(hum);
      lcd.setCursor(15, 2);
      lcd.print("%");  
      if(timerON){
        timer--;
        lcd.setCursor(7, 3);
        lcd.print("     ");
        lcd.setCursor(7, 3);
        lcd.print(timer);
        if(timer<=0){
          daynight=0;
          Lmode=4;
          timerON=0;
          lcd.setCursor(0, 3);
        lcd.print("          ");
        }
      }
}

void rainbow_loop() {                        //-m3-LOOP HSV RAINBOW
  idex++;
  ihue = ihue + 10;
  if (idex >= LED_COUNT) {
    idex = 0;
  }
  if (ihue > 255) {
    ihue = 0;
  }
  leds[idex] = CHSV(ihue, 255, 255);
  LEDS.show();
  delay(thisdelay-3);
}

void random_burst() {                         //-m4-RANDOM INDEX/COLOR
  idex = random(0, LED_COUNT);
  ihue = random(0, 255);
  leds[idex] = CHSV(ihue, 255, 255);
  LEDS.show();
  delay(thisdelay);
}

void new_rainbow_loop() {                      //-m88-RAINBOW FADE FROM FAST_SPI2
  ihue -= 1;
  fill_rainbow( leds, LED_COUNT, ihue );
  LEDS.show();
  delay(thisdelay);
}

void one_color_all(int cred, int cgrn, int cblu,int del) {       //-SET ALL LEDS TO ONE COLOR
  for (int i = 0 ; i < LED_COUNT; i++ ) {
    leds[i].setRGB( cred, cgrn, cblu);
    delay(del);
    LEDS.show();
  }
}

void writetime(){
    timeClient.update();
    lcd.setCursor(2, 0);
    lcd.print(timeClient.getFormattedDate());
    if(offhour==timeClient.getHours() && offminutes==timeClient.getMinutes()){
      Lmode=4;
      daynight=0;
    }
     
}

void OTA_part(){
    ArduinoOTA.setHostname("main");
  ArduinoOTA.setPassword("admin");
  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_FS
      type = "filesystem";
    }

    // NOTE: if updating FS this would be the place to unmount FS using FS.end()
    Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      Serial.println("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      Serial.println("End Failed");
    }
  });
  ArduinoOTA.begin();
}

void noisepicker(){
  int sumx=0;
  for(int i =0;i<150;i++)
  {
    int x = analogRead(0);
    sumx=sumx+x;
  }
  noise=(sumx/150)+12;
  Lmode=8;
  client.publish("noise", String(noise).c_str(), true);
}

void audiomode(){
  int x = analogRead(0);
  x =x-noise;
 // FastLED.setBrightness(100);
    if (x < 1) {
      leds[(LED_COUNT/2)] = CRGB(255, 0, 0);
     // FastLED.setBrightness(20);
    }
    else if (x > 1 && x <= 3) {
      leds[(LED_COUNT/2)] = CRGB(255, 154, 0);
    }
    else if (x > 3 && x <= 5) {
      leds[(LED_COUNT/2)] = CRGB(255, 255, 0);
    }
    else if (x > 5 && x <= 7) {
      leds[(LED_COUNT/2)] = CRGB(0, 255, 0);
    }
    else if (x > 7 && x <= 9) {
      leds[(LED_COUNT/2)] = CRGB(20, 0, 255);
    }
    else if (x > 9 && x <= 11) {
      leds[(LED_COUNT/2)] = CRGB(150, 102, 255);
    }
    else {
      leds[(LED_COUNT/2)] = CRGB(255, 0, 255);
    }
  FastLED.show();
  delay(10);
  for (int z = LED_COUNT; z > (LED_COUNT/2); z--) {
    leds[z] = leds[z - 1];
  }
  for (int z = 0; z < (LED_COUNT/2); z++) {
    leds[z] = leds[z + 1];
  }

}

void setup(){
    Serial.begin(115200);
    LEDS.setBrightness(max_bright);
    LEDS.addLeds<WS2811, LED_DT, GRB>(leds, LED_COUNT);  // settings for strip (ленты на WS2811, WS2812, WS2812B)
    one_color_all(0, 0, 0,0);          // Turn off all leds
    LEDS.show(); 
    pinMode(trigPin, OUTPUT); // Sets the trigPin as an Output
    pinMode(echoPin, INPUT); // Sets the echoPin as an Input
    pinMode(SENSPIN, INPUT);
    client.enableDebuggingMessages(); // Enable debugging messages sent to serial output
    client.enableHTTPWebUpdater(); // Enable the web updater. User and password default to values of MQTTUsername and MQTTPassword. These can be overrited with enableHTTPWebUpdater("user", "password").
    client.enableLastWillMessage("TestClient/lastwill", "I am going offline");  // You can activate the retain flag by setting the third parameter to true
    timeClient.begin();
    Wire.begin(2,0);
    lcd.init();   // initializing the LCD
    //pinMode(button, INPUT);
    myButt.setTimeout(1500);
    OTA_part();
    bme.begin(0x76);
}


int a =0;
int lastmode=0;
unsigned long lastmillis =0,lastmillis2 =0 ;
int lb=0;

 void loop()
{
  if(a == 0)
    {Lmode = 3;
    a++;
    send_temp();
    enabling();
    butcounter=0;
  }

  ArduinoOTA.handle();
  if(daynight)
  {
    lcd.backlight();
  }else{
    lcd.noBacklight();
  }
  sensor = digitalRead(SENSPIN);
  if(millis() - lastmillis >60000){
    lastmillis=millis();
    send_temp();
  }
  if(millis() - lastmillis2 >10000){
    lastmillis2=millis();
    writetime();
  }
  if(lb!=bright && bright<201){
    lb = bright;
    LEDS.setBrightness(bright);
  }
  myButt.tick();
  if(myButt.isSingle()){
    if(Lmode==5){
      Lmode=3;
      daynight=1;
    }else {Lmode=4; daynight=0;}
  }
  if(myButt.isDouble()){
    if(timer>0){
      timer=0;
      timerON=0;
      lcd.setCursor(0, 3);
      lcd.print("          ");
      }else if(Lmode==5){
    Lmode=3;
    }else {Lmode=4;}
  }

  if(myButt.isHold()){
    butcounter= butcounter+10;
            delay(100);  
            timer=((butcounter)/70)*5;
            lcd.setCursor(0, 3);
            lcd.print("Timer: ");
            lcd.setCursor(7, 3); 
            lcd.print(timer);
            timerON=1;
  }

  if(timerON && timer > 0 && !myButt.isHold()){
    butcounter=0;
  }

  if(lastmode==5 && Lmode!=5 && Lmode!=4)
  { enabling();}

  switch(Lmode){
    case 1: rainbow_loop(); break;
    case 2: random_burst(); break;
    case 3: new_rainbow_loop(); break;
    case 4: one_color_all(0,0,0,40);  Lmode=5; break;
    case 5: if(sensor==1)
                {presenza();} break;
    case 6: one_color_all(RGBset[0],RGBset[1],RGBset[2],0); break;
    case 7: noisepicker(); break;
    case 8: audiomode(); break;
  }
  lastmode=Lmode;
  client.loop();
}