/* wiring the MFRC522 to ESP8266 (ESP-12)
RST     = GPIO5
SDA(SS) = GPIO4 
MOSI    = GPIO13
MISO    = GPIO12
SCK     = GPIO14
GND     = GND
3.3V    = 3.3V
*/
#include <SPI.h>
#include "MFRC522.h"                
#include <ESP8266WiFi.h>
#include <WiFiClient.h> 
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
boolean laststate =  false;
boolean statedoor =  false;
boolean access = false;
#define SENSOR     15
//#define LED_OPEN   10 
//#define LED_CLOSE  9 
#define OPEN_PIN   0 
#define CLOSE_PIN  2 
#define RST_PIN    5          // Configurable, see typical pin layout above
#define SS_1_PIN   4         // Configurable, take a unused pin, only HIGH/LOW required, must be diffrent to SS 2
#define SS_2_PIN   16 
#define NR_OF_READERS   2 // количество считывателей
unsigned long uidDec;
String code, side, masterkey;
// unsigned long uidDec, uidDecTemp;
 
const char *ssid = ""; //Your wifi name 
const char *password = ""; // Your fiwi Pass
byte ssPins[] = {SS_1_PIN, SS_2_PIN};
MFRC522 mfrc522[NR_OF_READERS];   // Create MFRC522 instance.  
//Web/Server address to read/write from 
const char* host = "http://iot.rex.knu.ua/";   
void setup() {
  delay(1000);
  Serial.begin(115200);
  WiFi.mode(WIFI_OFF);        //Prevents reconnection issue (taking too long to connect)
  delay(1000);
  WiFi.mode(WIFI_STA);        //This line hides the viewing of ESP as wifi hotspot
  WiFi.begin(ssid, password);     //Connect to your WiFi router
  Serial.println("wifi connected");
  SPI.begin();           // Init SPI bus
  for (uint8_t reader = 0; reader < NR_OF_READERS; reader++) 
  {
    mfrc522[reader].PCD_Init(ssPins[reader], RST_PIN); // Init each MFRC522 card
    Serial.print(F("Reader "));
    Serial.print(reader);
    Serial.print(F(": "));
    mfrc522[reader].PCD_DumpVersionToSerial();
  }
  // Wait for connection
  int a = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    a++;
    if(a==120){
      break;        
    }
  }
  //If connection successful show IP address in serial monitor
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());  //IP address assigned to your ESP
  pinMode(OPEN_PIN, OUTPUT);
  pinMode(CLOSE_PIN, OUTPUT);
          
}
//--------------------------------------------------------------------------------------------------------------------------------
int waitingtimer = 0;
int initvar =0;
int wifitimer = 0;
int timer = 0;
void loop() {
    if( initvar<1)
    {
      Serial.println("start");
      masterkey = "56a0725b";
      initvar++;
    }
    if(WiFi.status() != WL_CONNECTED) // if wifi is off 
    {        
      wifitimer++;
    }
    if (wifitimer == 120)                       // reconnect wifi every 1 min
    {
      WiFi.mode(WIFI_OFF);        //Prevents reconnection issue (taking too long to connect) 
      delay(1000);
      WiFi.mode(WIFI_STA);        //This line hides the viewing of ESP as wifi hotspot
      WiFi.begin(ssid, password);
      wifitimer = 0;
    }  
    statedoor = !digitalRead(SENSOR);
    if (laststate == 0 && statedoor == 1)
    {
      delay(1500);
      CLOSE();
    }
    while (laststate == 1 && statedoor == 1 && access ==1)
    {
      statedoor = !digitalRead(SENSOR);
      delay (100);
      Serial.print(waitingtimer);
      Serial.print(" ");
      waitingtimer++;
      if(waitingtimer > 100){
        CLOSE();
      }
    }
    if (statedoor ==0)
    {
      laststate = false;
      delay(100);
    }    
    for (uint8_t reader = 0; reader < NR_OF_READERS; reader++) 
    {
      if (mfrc522[reader].PICC_IsNewCardPresent() && mfrc522[reader].PICC_ReadCardSerial()) 
      {
        code = "";
        dump_byte_array(mfrc522[reader].uid.uidByte, mfrc522[reader].uid.size);
        Serial.println ();
        Serial.print ("code: ");
        Serial.println (code);
        if (code == masterkey){
          OPEN();
        }
        mfrc522[reader].PICC_HaltA();
        //--------------------------------- Stop encryption on PCD
        mfrc522[reader].PCD_StopCrypto1();
        if(reader == 0){
          side = "&action=INPUT";
          Serial.println("go in");
        }
        if(reader == 1){
          side = "&action=OUTPUT";
          Serial.println("go out");
        }                                   
        String getData, Link, test,postData, secret;
        secret = "&secret=esp8266SecretKey";     //Security code
        //--------------------- GET Data
        test = "carduid=";
        getData = (test + code);  //Note "?" added at front
        postData = (getData + side + secret);
        Serial.print("postData:  ");
        Serial.println(postData);
        HTTPClient http;    //Declare object of class HTTPClient
        http.begin(host);     //Specify request destination
        http.addHeader("Content-Type", "application/x-www-form-urlencoded");
        
        int httpCode = http.POST(postData);            //Send the request
        String payload = http.getString();    //Get the response payload
        Serial.print("HTTP code: ");
        Serial.println(httpCode);   //Print HTTP return code
        Serial.print("payload: ");
        Serial.println(payload); //Print request response payload
        String ACCESS = "Access";
        if( ACCESS == payload)
        { 
          OPEN();
          Serial.print("Waiting card: ");
          delay (50);
        }
        else{
          Serial.println("NO ACCESS");
          Serial.print("Waiting card: "); 
        }   
        http.end();  //Close connection
      }
      else{
        Serial.print(".");
        delay (500);
      }
    }
}
void dump_byte_array(byte *buffer, byte bufferSize) {
  for (byte i = 0; i < bufferSize; i++) {
  // Serial.print(buffer[i] < 0x10 ? " 0" : " ");
  //Serial.print(buffer[i], HEX);
  code = code + String(buffer[i], HEX);
  }
}
void OPEN()
{
  Serial.println("OPEN LOCK");
  digitalWrite(CLOSE_PIN, HIGH);
  digitalWrite(OPEN_PIN, LOW);
  delay (400);
  digitalWrite(CLOSE_PIN, LOW);
  digitalWrite(OPEN_PIN, LOW);
  // digitalWrite(LED_OPEN, LOW);
  waitingtimer = 0;
  access = 1;
}
void CLOSE()
{ 
  Serial.println("close lock");
  digitalWrite(CLOSE_PIN, LOW);
  digitalWrite(OPEN_PIN, HIGH);
  delay (400);
  digitalWrite(CLOSE_PIN, LOW);
  digitalWrite(OPEN_PIN, LOW);
  // digitalWrite(LED_CLOSE, LOW);
  laststate = 1;
  access = 0;
}
