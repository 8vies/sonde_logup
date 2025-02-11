/////////////////////////////////////////////////////////////////////////////////////////////////////////
///     Module ESP32  (eg:ESP32 WROOM-32 usbC HW-394)
///     Thermometre 1 wire DS18B20 [pin D4= fil jaune + pullup 1k vers fil rouge =D2 (sortie 3V3 capteurs)
///     Leds RVB (D13 D12 D14)
///     Dessouder LDO LT1117 et led rouge pwr. alimenter en 3V3 -> conso deep sleep mesuree = 9µA
///     Vers google sheet

///     Type de carte: ESP32 Wrover module
///     speed 921600
///     flash 80MHz / QIO
///     partition 4M
///     core rien
///     erase all flash disable
///     
///
///     HW: 
///       Batterie LiFePO4 sur GND-3V3
///       Mesure tension baterie:
///         R1=1kohm entre D32 et D33
///         R2=3k3   entre D25 et D33
///
///       Temperatures 1 Wire:
///         DS18B20 1: GND  2:D13(DATA) 3:D12(+3-5V)
///         Pull-up 2k D12-D13
///
///      TDS DFrobot SEN0244 - pin 1 (ana)=> io 34
///           remplacer R2=1k2  R4=300k
///           conso: 1,5 - 3mA @ 5V avant LDO  - LDO 3V dropout<0,2V :: alimentable par io ESP32
///           connecter VCC (pin2) sur D12 (sortie alimentation )
///
///     Fil detection fuite FLOW connecté à io 4
///            detection niveau: 0 - 10kohm (3 x 3,3kohm pour niveaux 1000;2000;3000;4000)
///            perturbe TDS!!!
///
///     ILS sur BP (IO0) - GND (+perle ferrite pour garder aimant
///           - demarrage avec aimant + boucle 5X sur envoi data + lance serveur web + boucle indefiniment.
///           - passage de l'aimant pour avoir une valeur
///           - laiiser aimant pour 5 valeurs . attendre 2 min pour creer ap wifi
///     Le SSID et MDP sont stockes en EEprom
///     se connecter au wifi [velolab] mdp=""
///     192.168.4.1 
///     - [nouveau SSID]
///     - [nouveau MDP]
///     - [8V][P][NOM] - "8V" = cle d'autentification P=PREFIX = "A" , B, NOM="sonde beaurivage"
///     8V: enregistre un nouveau reseau WIFI 1 a 4
///     8Z: enregistre un reseau et efface les anciens
///     ZZ: efface la EEprom
///
//////         
///
/*
config      192.168.4.1/setting?ssid=SSID&pass=pwd&check=8ZRS1JU
*/

#define MINUTE_REVEIL 30
//#define COLONNE_TDS "m"
//#define SONDE_BAT
//#define RELAIS_OUT

#define EE_NB_WIFI  500
#define EE_PREFIX  501
#define EE_NAME     502
#define EE_SSID     0
#define EE_PWD      32
#define EE_NB_SSID  3
#define EE_1WIREADR 400
#define NB_1WIRE    16


#include "time.h"
#include "sntp.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <WebServer.h>
#include <EEPROM.h>
#include "var8vies.h"
//Variables

#define NAME  "ESPtest"
///#define PREFIX  "F"

    #define PIN_TDS     34
    #define PIN_FLOW    34         // Fil detecte une fuite de liquide % GND   !!! oxyde la sonde TDS

#ifdef RELAIS_OUT
  #define RELAIS1   14
  #define RELAIS2   27
  #define RELAIS3   26
  #define RELAIS4   25
  #define RELAIS5   33
  #define RELAIS6   32
  #define RELAIS7   19
  #define RELAIS8   18
#endif


#define PIN_1WIRE   13
#define PIN_EN_OUT  12
#define PIN_VCCLOG    4

// #define LED_R   27
// #define LED_V   26
// #define LED_B   14
#define BLED    2

#define LED_ON  0
#define LED_OFF  1


///       Batterie LiFePO4 sur GND-3V3
///       Mesure tension baterie:
///         R1=1kohm entre D32 et D33
///         R2=3k3   entre D25 et D33
#define R1  1000
#define R2  3300
#define VREFI 1100


#define HTTP_RETRY  3


#include <ESP32Time.h>

uint8_t httpretry=3;
uint16_t httptimeout=3000;
ESP32Time rtc(3600);  // offset in seconds GMT+1
uint16_t flow=0;
String networks="";
const char* ntpServer1 = "pool.ntp.org";
const char* ntpServer2 = "time.nist.gov";
const long  gmtOffset_sec = 3600;
const int   daylightOffset_sec = 3600;
const char* time_zone = "CET-1CEST,M3.5.0,M10.5.0/3";  // TimeZone rule for Europe/Rome including daylight adjustment rules (optional)
uint8_t interval=0;
uint8_t cnt_col='e';

String argStr="";
uint8_t nb_wifi=0;
uint8_t cnt_testmode=0;
String device_name="";  ///
String prefix="";  /// remplacé par la clef CV : 8V[prefix] 
int i = 0;
int statusCode;
const char* ssid = "";
const char* passphrase = "";
String st;
String content;
String esid;
String epass = "";
String logstate="";
String sheet="data";
//Function Decalration
bool testWifi(void);
void launchWeb(void);
void setupAP(void);
//Establishing Local server at port 80
WebServer server(80);

const char* apssid           = "sonde.8Vies.fr";                // SSID Name
const char* appassword       = "";   // !!! SSID invisible ou ne fonctionne pas si pwd !!!!! SSID Password - Set to NULL to have an open AP
const int   apchannel        = 10;                        // WiFi Channel number between 1 and 13
const bool  hide_SSID      = false;                     // To disable SSID broadcast -> SSID will not appear in a basic WiFi scan
const int   max_connection = 2;                         // Maximum simultaneous connected clients on the AP

 double mesure=0;
 
int8_t heure=-1,minute=-1,seconde=-1;
//String SPREADSHEET_ID = "";

const int sendInterval = 5000;
//#define LCD

/////////////////////  DEEP LSEEP 
//https://www.upesy.fr/blogs/tutorials/how-to-use-deep-sleep-on-esp32-to-reduce-power-consumption?shpxid=fdabc5c2-2602-4d4e-81b6-de570e71ca05#
//////////////////////////////////////////////////////////////
#define uS_TO_S_FACTOR 1000000
#define TIME_TO_SLEEP  1800 // seconde
#define TIME_TO_SLEEP_FAIL  1800 // seconde
#define TIME_BEFORE_SLEEP  60 

uint16_t timeBeforeSleep=TIME_BEFORE_SLEEP*10; // nb dizieme de secondes 
RTC_DATA_ATTR int bootCount = 0;
RTC_DATA_ATTR int bootCountEn = 0;

//////////////////////   1 WIre Temp
/*********
  Rui Santos  
  Complete project details at https://RandomNerdTutorials.com  
*********/

#include <OneWire.h>
#include <DallasTemperature.h>
// GPIO where the DS18B20 is connected to
const int oneWireBus = PIN_1WIRE;     
// Setup a oneWire instance to communicate with any OneWire devices
OneWire oneWire(oneWireBus);
// Pass our oneWire reference to Dallas Temperature sensor 
DallasTemperature sensors(&oneWire);
int numberOfDevices; 
DeviceAddress tempDeviceAddress; 
String getStr="";

//////////////////////////


hw_timer_t * timer = NULL;
volatile SemaphoreHandle_t timerSemaphore;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

volatile uint32_t isrCounter = 0;
volatile uint32_t lastIsrAt = 0;
bool blinkl=0;
uint8_t isr_cnt=0;
uint32_t ledcode=0x0101;
uint32_t  wdt=0;
void ARDUINO_ISR_ATTR onTimer(){
  // Increment the counter and set the time of ISR
  portENTER_CRITICAL_ISR(&timerMux);
  isrCounter++;
  lastIsrAt = millis();
  portEXIT_CRITICAL_ISR(&timerMux);
  // Give a semaphore that we can check in the loop
  xSemaphoreGiveFromISR(timerSemaphore, NULL);
  //blinkl=!blinkl;
  isr_cnt++;
  if(isr_cnt==16) isr_cnt=0;
  wdt++;
  if(wdt>3000) deep_sleep(10); // wdt 15 mnutes = 90x10 s
  digitalWrite(BLED,(ledcode>>isr_cnt)&0x0001);
  // It is safe to use digitalRead/Write here if you want to toggle an output
}

void timer_setup() {
  // Create semaphore to inform us when the timer has fired
  timerSemaphore = xSemaphoreCreateBinary();

  // Use 1st timer of 4 (counted from zero).
  // Set 80 divider for prescaler (see ESP32 Technical Reference Manual for more
  // info).
  timer = timerBegin(0, 80, true);

  // Attach onTimer function to our timer.
  timerAttachInterrupt(timer, &onTimer, true);

  // Set alarm to call onTimer function every second (value in microseconds).
  // Repeat the alarm (third parameter)
  timerAlarmWrite(timer, 100000, true);

  // Start an alarm
  timerAlarmEnable(timer);
}
  struct tm time_st;
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
uint32_t voltage=0;
void setup()
{    
  logstate="";//prefix+";";
  

  Serial.begin(115200); //Initialising if(DEBUG)Serial Monitor
  Serial2.begin(9600); //openLog
  
  time_st = rtc.getTimeStruct();
  Serial.print("RTC:  ");
  Serial.println(&time_st, "%A, %B %d %Y %H:%M:%S");   //  (tm struct) Sunday, January 17 2021 07:24:38
  Serial.print(" bootCount = ");Serial.print(bootCount);Serial.print(" / "); Serial.println(bootCountEn);
    
  if(bootCount%4!=bootCountEn)  deep_sleep(900); // se rendort 3x15min ////

  
//  test_openlog();
 
  
  analogReadResolution(12);
  analogSetAttenuation(ADC_0db);
  
  timer_setup();
  Serial.println("Disconnecting current wifi connection");
  WiFi.disconnect();
  EEPROM.begin(1024); //Initialasing EEPROM
  delay(10);
  pinMode(15, INPUT);
  pinMode(PIN_EN_OUT, OUTPUT);
  // pinMode(LED_R, OUTPUT);
  // pinMode(LED_V, OUTPUT);
  // pinMode(LED_B, OUTPUT);
  pinMode(BLED, OUTPUT);

  
#ifdef RELAIS_OUT
  pinMode(RELAIS1, OUTPUT);
  pinMode(RELAIS2, OUTPUT);
  pinMode(RELAIS3, OUTPUT);
  pinMode(RELAIS4, OUTPUT);
  pinMode(RELAIS5, OUTPUT);
  pinMode(RELAIS6, OUTPUT);
  pinMode(RELAIS7, OUTPUT);
  pinMode(RELAIS8, OUTPUT);
  digitalWrite(RELAIS1,1);
  digitalWrite(RELAIS2,1);
  digitalWrite(RELAIS3,1);
  digitalWrite(RELAIS4,1);
  digitalWrite(RELAIS5,1);
  digitalWrite(RELAIS6,1);
  digitalWrite(RELAIS7,1);
  digitalWrite(RELAIS8,1);
#endif
  
#ifdef SONDE_BAT
  pinMode(PIN_VCCLOG, OUTPUT);
  digitalWrite(PIN_VCCLOG,1);
  pinMode(PIN_FLOW, INPUT_PULLUP);
///       Mesure tension baterie:
  pinMode(32, OUTPUT);
  pinMode(25, OUTPUT);
  pinMode(33, INPUT);
  digitalWrite(25,1);
  digitalWrite(32,0);
  voltage=(int) ( 100*analogRead(33)*(1.1*(R1+R2))/(R1*4096));
  Serial.print("voltage=");    Serial.println(voltage/100.0);
#endif

  
  digitalWrite(PIN_EN_OUT,1);
  // digitalWrite(LED_R,LED_OFF);
  // digitalWrite(LED_V,LED_OFF);
  // digitalWrite(LED_B,LED_ON);
  ledcode=0x0101;
  Serial.println("Startup");

if(EEPROM.read(400)==255)
{
 Serial.println("clearing eeprom");
        for (int i = 400; i < 600; ++i) {
          EEPROM.write(i, 0x00);
        }
        EEPROM.commit();
}
  

 // pinMode(35, INPUT_PULLUP);Serial.print("35=");    Serial.println(analogRead(33));
  //pinMode(15, INPUT_PULLUP);Serial.print("15=");    Serial.println(analogRead(15));
  //pinMode(2, INPUT_PULLUP);Serial.print("2=");    Serial.println(analogRead(2));
 flow=4096-analogRead(PIN_FLOW);
// eau @ 40g/L detection de niveau resistif
// NC ->  1
// +9k9 -> ~1000
// +6k6 -> ~ 2000
//  +3k3 -> ~3000
//  +0 -> ~4000

 
 Serial.print("FLOW=");    Serial.println(flow);

  //////////////////////////////////////
  sensors.begin();
  // Grab a count of devices on the wire
  numberOfDevices = sensors.getDeviceCount();
  Serial.println("nb temp captor ="+String(numberOfDevices));

  argStr="";
  
    mesure= mesure_tds();
    Serial.println("TDS="+String(mesure));
    cnt_col='f';
    addStr(mesure);
    cnt_col='o';
    get1Wire();



        
  //---------------------------------------- Read eeprom for ssid and pass
  nb_wifi=(uint8_t ) EEPROM.read(EE_NB_WIFI);
  Serial.print("\n Nb wifi in EE=");   Serial.println (nb_wifi); 

  if(nb_wifi>4) nb_wifi=4;
  if(nb_wifi>1)
  { // recherche d'un wifi connu parmi la EE
  String pssid="",wssid="";
    WiFi.mode(WIFI_STA);
     WiFi.disconnect();
        delay(100);
    int n = WiFi.scanNetworks();
    if (n == 0)  Serial.println("no networks found");
    else
      {
        Serial.print(n); Serial.println(" networks found");
        for (int i = 0; i < n; ++i)
        {
          pssid=String(WiFi.SSID(i));
          Serial.print(pssid);Serial.println("...");
          for(int wee=0;wee<nb_wifi;wee++)
          {
            wssid=ReadStringEE(EE_SSID+i*96,32);
           // Serial.print(wssid); Serial.print("=?=");Serial.println(pssid);
            if(wssid==pssid)
            {
                  Serial.print(pssid); Serial.println(" connu");
                  esid=wssid;
                  epass=ReadStringEE(i*96+EE_PWD,64);
                  i=n;
                  wee=nb_wifi;
                  break;
            }
            //else {Serial.print(wssid); Serial.println(" inconnu");}
          }
          delay(10);
        }
      }
  
  WiFi.disconnect();
  }
  else
  {
      Serial.println("Reading EEPROM ssid");
      esid=ReadStringEE(EE_SSID,32);
      epass=ReadStringEE(EE_PWD,64);
  }
 // prefix=String((char) EEPROM.read(501));
 prefix=ReadStringEE(EE_PREFIX,1);
  if(prefix=="") prefix='z';
  device_name=ReadStringEE(EE_NAME,10);
  if(device_name=="") device_name="sans_nom";
  sheet=device_name;
  
  Serial.print("\n SSID: ");   Serial.print (esid);   
  Serial.print("\n PASS: "); Serial.print(epass);
  Serial.print("\n PREFIX: ");Serial.print(prefix);
  Serial.print("\n NAME: ");Serial.println(device_name);
  logstate+=prefix;
  if(bootCount%(24*4)==0) // scan les SSID + RSSI toutes les 24h
  {
    scan_network();
    //logstate+=networks;
  }
  WiFi.disconnect();

  
  WiFi.begin(esid.c_str(), epass.c_str());
    delay(1000);
logstate+=String(rtc.getMinute())+";"+String(bootCount)+";";
  
if(bootCount%8==0) // tous les x quart dheures - remise a l heure par NTP
  {
    Serial.println("NTP st");
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer1, ntpServer2);
    sntp_set_time_sync_notification_cb( timeavailable );
    sntp_servermode_dhcp(1);    // (optional)
  }
  
 
}
     char s2[64];
/////////////////////////////////////////:::
int rssiw=0;
/////////////////////////////////////////
/////////////////////////////////////////
/////////////////////////////////////////
/////////////////////////////////////////
/////////////////////////////////////////
uint8_t nbcli=0;
void loop() {
  wdt=0;
  if ((WiFi.status() == WL_CONNECTED))
  {
    
      delay(1000);
      Serial.print("Connected to ");
      Serial.print(esid);
      Serial.print(" Successfully \n RSSI=");
      rssiw=WiFi.RSSI();
      Serial.println(rssiw);
     // logstate+=String(WiFi.RSSI())+"dB;";

      
  // digitalWrite(LED_R,LED_OFF);
  // digitalWrite(LED_V,LED_ON);
  // digitalWrite(LED_B,LED_OFF);
  ledcode=0x0005;

 // logstate+=String(voltage/100.0)+"V"+";";

  #ifdef RELAIS_OUT
httpretry=5;
httptimeout=10000;
#else
httpretry=1;
httptimeout=3000;
#endif

     //String stds=String(tds);
     String svolt=String(voltage/100.0);
     //stds.replace('.',',');
     svolt.replace('.',',');
     String ccoment=String(rtc.getMinute())+";"+String(bootCount)+";"+String(numberOfDevices)+";"+ svolt+"V;"+String(WiFi.RSSI())+"dB;"+String(flow)+";"+networks;
     //argStr  //String(COLONNE_TDS)+"="+stds

     spreadsheet_get("hour"+argStr+"&key="+prefix+"="+"&val="+ccoment+"&state="+logstate+String(device_name),1);

     sprintf(s2,"\n%02d%02d%02d;%02d:%02d:%02d;%d;%d;%s;%d;%d;;;;;;;;;;;", rtc.getYear(), rtc.getMonth(), rtc.getDay(),rtc.getHour(), rtc.getMinute(), rtc.getSecond(),bootCount,numberOfDevices,svolt,WiFi.RSSI(),flow);
     Serial2.print(s2); //openlog
     
     //1,String(voltage),"ESP32",String(bootCount)+";"+String(voltage));//(uint8_t p,String state,String key,String val);
    //  send_sheet();

      delay(1000);
  #ifdef RELAIS_OUT
httpretry=5;
httptimeout=10000;
      spreadsheet_get("get4&hour&",1);
    gpio_deep_sleep_hold_dis();
  gpio_hold_dis((gpio_num_t) RELAIS1);  
  gpio_hold_dis((gpio_num_t) RELAIS2);  
  gpio_hold_dis((gpio_num_t) RELAIS3);  
  gpio_hold_dis((gpio_num_t) RELAIS4);  
  gpio_hold_dis((gpio_num_t) RELAIS5);  
  gpio_hold_dis((gpio_num_t) RELAIS6);  
  gpio_hold_dis((gpio_num_t) RELAIS7);  
  gpio_hold_dis((gpio_num_t) RELAIS8);  

#endif
      delay(1000);
      uint16_t tts=calc_time2sleep(MINUTE_REVEIL,0);
      
      Serial.println(String(heure)+":"+String(minute)+"  reveil dans : "+String(tts));
        ledcode=0x0001;
#ifdef RELAIS_OUT

      //deep_sleep(tts);


          Serial.println(tts); //openlog
          Serial.println(wdt); //openlog
      uint8_t thesec=0;
      for(uint16_t sec=0;sec<tts;sec++)
      {
                  Serial.print("."); //openlog
          while(thesec==rtc.getSecond())       delay(100);
          thesec=rtc.getSecond();
          //sprintf(s2,"\n%02d%02d%02d;%02d:%02d:%02d;%d;%d;%s;%d;%d;;;;;;;;;;;", rtc.getYear(), rtc.getMonth(), rtc.getDay(),rtc.getHour(), rtc.getMinute(), rtc.getSecond(),bootCount,numberOfDevices,svolt,WiFi.RSSI(),flow);
          //Serial.print(s2); //openlog
     
          wdt=0;
      }
      networks="";
      logstate="";


#else
      deep_sleep(tts);
      cnt_testmode++;
#endif
    
  }
  else
  {
  }
  if (testWifi() && cnt_testmode<5)
  {
    Serial.print(" tm = ");
    Serial.print(cnt_testmode);
    Serial.println(" connection status positive");
    
    return;
  }
  else
  {
    if(cnt_testmode>=5)   Serial.println("5 reboot en mode test :  ");
    else    Serial.println("Connection Status Negative ");
    Serial.println("Turning the HotSpot On");
    cnt_testmode=0;
  ledcode=0x0017;  // ___ .
    
  // digitalWrite(LED_R,LED_ON);
  // digitalWrite(LED_V,LED_OFF);
  // digitalWrite(LED_B,LED_ON);
  
    launchWeb();
    setupAP();// Setup HotSpot
  }
  Serial.println();
  Serial.println("Waiting.");
  while ((WiFi.status() != WL_CONNECTED))
  {
   // Serial.print(".");
   
    delay(100);
    server.handleClient();
    if(nbcli!=WiFi.softAPgetStationNum())
    {
      nbcli=WiFi.softAPgetStationNum();
      timeBeforeSleep=TIME_BEFORE_SLEEP*10;
      Serial.println("Nombre de connexions WiFi:"+String( nbcli));
    }
    //Serial.println( nbcli);
    
    if(timeBeforeSleep%10==0)
    {
      Serial.println("time before sleep: " + String(timeBeforeSleep/10));
    }
    if(timeBeforeSleep--==0)
    {
      
      Serial.println("  reveil dans : "+String(TIME_TO_SLEEP_FAIL));
      deep_sleep(TIME_TO_SLEEP_FAIL);
      timeBeforeSleep=600; // au cas ou deep sleep inhibé
    }

    
  }
  delay(1000);
}
//----------------------------------------------- Fuctions used for WiFi credentials saving and connecting to it which you do not need to change
bool testWifi(void)
{
  int c = 0;
  //Serial.println("Waiting for Wifi to connect");
  while ( c < 20 ) {
    if (WiFi.status() == WL_CONNECTED)
    {
      return true;
    }
    delay(500);
    Serial.print("*");
    c++;
  }
  Serial.println("");
  Serial.println("Connect timed out, opening AP");
  return false;
}
void launchWeb()
{
  Serial.println(" web ");
  if (WiFi.status() == WL_CONNECTED)
    Serial.println("WiFi connected");
  Serial.print("Local IP: ");
  Serial.println(WiFi.localIP());
  Serial.print("SoftAP IP: ");
  Serial.println(WiFi.softAPIP());
  /*  IPAddress ip = WiFi.softAPIP();
      String ipStr = String(ip[0]) + '.' + String(ip[1]) + '.' + String(ip[2]) + '.' + String(ip[3]);
      content = "<!DOCTYPE HTML>\r\n<html>Welcome to Wifi Credentials Update page";
      content += "<form action=\"/scan\" method=\"POST\"><input type=\"submit\" value=\"scan\"></form>";
      content += ipStr;
      content += "<p>";
      content += st;
      content += "</p><form method='get' action='setting'><br><label>SSID: </label><input name='ssid' length=32><br><label>PWD: </label><input name='pass' length=64><br><label>CV: </label><input name='check' length=64><br><input type='submit'></form>";
      content += "</html>";
      server.send(200, "text/html", content);
*/
  createWebServer();
  // Start the server
  server.begin();
  Serial.println("Server started");
}
void setupAP(void)
{
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  int n = WiFi.scanNetworks();
  Serial.println("scan done");
  if (n == 0)
    Serial.println("no networks found");
  else
  {
    Serial.print(n);
    Serial.println(" networks found");
    for (int i = 0; i < n; ++i)
    {
      // Print SSID and RSSI for each network found
      Serial.print(i + 1);
      Serial.print(": ");
      Serial.print(WiFi.SSID(i));
      Serial.print(" (");
      Serial.print(WiFi.RSSI(i));
      Serial.print(")");
      //Serial.println((WiFi.encryptionType(i) == ENC_TYPE_NONE) ? " " : "*");
      delay(10);
    }
  }
  Serial.println("");
  st = "<ol>";
  for (int i = 0; i < n; ++i)
  {
    // Print SSID and RSSI for each network found
    st += "<li>";
    st += WiFi.SSID(i);
    st += " (";
    st += WiFi.RSSI(i);
    st += ")";
    //st += (WiFi.encryptionType(i) == ENC_TYPE_NONE) ? " " : "*";
    st += "</li>";
  }
  st += "</ol>";
  WiFi.mode(WIFI_AP);
  WiFi.disconnect();
  delay(500);
  WiFi.softAP(apssid, appassword, apchannel, hide_SSID, max_connection);
  Serial.println("Initializing_softap_for_wifi credentials_modification");
  delay(2000);
  launchWeb();
  Serial.println("over");
}
void createWebServer()
{
  {
    
  // digitalWrite(LED_R,LED_ON);
  // digitalWrite(LED_B,LED_ON);
    server.on("/", []() {
      timeBeforeSleep=TIME_BEFORE_SLEEP*10;
      Serial.println(" req "+String(timeBeforeSleep));
      IPAddress ip = WiFi.softAPIP();
      String ipStr = String(ip[0]) + '.' + String(ip[1]) + '.' + String(ip[2]) + '.' + String(ip[3]);
      content = "<!DOCTYPE HTML>\r\n<html>Welcome to Wifi Credentials Update page";
      content += "<form action=\"/scan\" method=\"POST\"><input type=\"submit\" value=\"scan\"></form>";
      content += ipStr;
      content += "<p>";
      content += st;
      content += "</p><form method='get' action='setting'><br><label>SSID: </label><input name='ssid' length=32><br><label>PWD: </label><input name='pass' length=64><br><label>CV: </label><input name='check' length=64><br><input type='submit'></form>";
      content += "</html>";
      server.send(200, "text/html", content);
    });
    server.on("/scan", []() {
      timeBeforeSleep=TIME_BEFORE_SLEEP*10;
      //setupAP();
      IPAddress ip = WiFi.softAPIP();
      String ipStr = String(ip[0]) + '.' + String(ip[1]) + '.' + String(ip[2]) + '.' + String(ip[3]);
      content = "<!DOCTYPE HTML>\r\n<html>go back";
      server.send(200, "text/html", content);
    });
    server.on("/sd", []() {
      timeBeforeSleep=TIME_BEFORE_SLEEP*10;
      String cmd = server.arg("cmd");
      //setupAP();
      IPAddress ip = WiFi.softAPIP();
      String ipStr = String(ip[0]) + '.' + String(ip[1]) + '.' + String(ip[2]) + '.' + String(ip[3]);
      content = "<!DOCTYPE HTML>\r\n<html>go back";
      content+=test_openlog(cmd);
      server.send(200, "text/html", content);
    });
    //////192.168.4.1/setting?ssid=toto&pass=tata&check=vie
    server.on("/setting", []() {
      timeBeforeSleep=TIME_BEFORE_SLEEP*10;
      Serial.println(" setting ");
      String qsid = server.arg("ssid");
      String qpass = server.arg("pass");
      String qcheck = server.arg("check");
       if (qcheck.substring(0,2) =="EE")
      {
        char eec;
        content="";
        for(int i=0;i<1024;i++)
        {
            if(i%96==0)  content+="\n<BR>";
            eec=EEPROM.read(i);
            if(eec==0) eec='_';
            if(eec<32) eec='.';
            if(eec==255) eec='°';
            if(eec>128) eec='#';
            content+=eec;
        }
        content+="<BR><BR>\n1Wire:<BR>";
        for(int i=400;i<500;i++)
        {
            eec=EEPROM.read(i);
            if(eec>0) { content+= (uint8_t) eec +String(" "); if(i%2==1)  content+="\n<BR>";}
           //content+=eec;
        }
        content+="\nnb Wifi=";
        content+= (uint8_t) EEPROM.read(500);
        content+="\n<BR>\nprefix=";
        content+= (char) EEPROM.read(501);
        content+="\n<BR>\nname=";
        for(int i=502;i<511;i++)
        {
            eec=EEPROM.read(i);
            if(eec>0) { content+= (char) eec;}
        }
        content+="\n<BR>\n";
      server.send(200, "text/html", content);
      }
      if (qcheck.substring(0,2) =="ZZ")
      {
            nb_wifi=0;
          for (i = 0; i < 511; ++i) {
              EEPROM.write(i, 0);
            }
            
            EEPROM.commit();
      }
      else if (qcheck.substring(0,2) =="8V"||qcheck.substring(0,2) =="8Z")
      {
        
        if(qsid.length() > 0 && qpass.length() > 0) 
        {
          uint16_t i=0;
          if(qcheck.substring(0,2) =="8Z")  nb_wifi=0;
          else
          {
            for(i=288;i>0;i--) // decale banques (2,1) - > //(3,2)  banque= 128B=[32[SSID]+96[MDP]]
            {
              EEPROM.write(i+96, EEPROM.read(i));
            }
          }
          
            Serial.println(qsid);
            Serial.println(qsid.length());
            Serial.println(qpass);
            Serial.println(qpass.length());
            Serial.println("writing eeprom ssid:");
            for (i = 0; i < qsid.length(); ++i)
            {
              EEPROM.write(i, qsid[i]);//+nb_wifi*100
              Serial.print("Wrote: ");
              Serial.print(String(i));
              Serial.print(" : ");
              Serial.println(qsid[i]);
            }
            EEPROM.write(i, 0);
              Serial.print("Wrote: ");
              Serial.print(String(i));
              Serial.print(" : ");
              Serial.println(0);
            Serial.println("writing eeprom pass:");
            for (i = 0; i < qpass.length(); ++i)
            {
              EEPROM.write(32 + i, qpass[i]);//+nb_wifi*100
              Serial.print("Wrote ");
              Serial.print(String(32+i));
              Serial.print(" : ");
              Serial.println(qpass[i]);
            }
            EEPROM.write(i+32, 0);
            Serial.print("writing eeprom prefix:");
            prefix=qcheck.substring(2);
            Serial.println(prefix);
            if(prefix.length()==1) EEPROM.write(501, prefix[0]);
            else if(prefix.length()>1)
            {
                  for (i = 0; i < prefix.length(); ++i)
                  {
                    EEPROM.write(501 + i, prefix[i]);
                    Serial.print("Wrote: ");
                    Serial.print(String(501+i));
                    Serial.print(" : ");
                    Serial.println(prefix[i]);
                  }
                  EEPROM.write(i+501, 0);
            }
            if(nb_wifi<3) nb_wifi++;

            EEPROM.write(500, nb_wifi);
            EEPROM.commit();
            
             Serial.print("NB WIFI ");
             Serial.println(nb_wifi);
            content = "{\"Success\":\"saved to eeprom... reset to boot into new wifi\"}";
            statusCode = 200;
            
            ledcode=0x3333;
            delay(5000);
            ESP.restart();
            
          } 
          else 
            {
                content = "{\"Error\":\"404 not found\"}";
                statusCode = 404;
                Serial.println("Sending 404");
                    delay(500);
              
            
            } 
      }
         else 
      {
         content= "EE="+qcheck.substring(0,2)+String("+")+qcheck.substring(2);
       // content = "{\"Error\":\"CV != found\"}";
        statusCode = 404;
        Serial.println("Sending 404");//qcheck.substring(0,2)
        Serial.println(content);
            delay(500);
      }
      server.sendHeader("Access-Control-Allow-Origin", "*");
      server.send(statusCode, "application/json", content);
      
  Serial.println(" ! ");
  
  // digitalWrite(LED_R,LED_ON);
  // digitalWrite(LED_V,LED_OFF);
  // digitalWrite(LED_B,LED_OFF);
    });
  }
}

/////////////////////////////////////////////////////////////////////////////////
///  ESP SLEEP

void print_wakeup_reason(){
   esp_sleep_wakeup_cause_t source_reveil;
   source_reveil = esp_sleep_get_wakeup_cause();

   switch(source_reveil){
      case ESP_SLEEP_WAKEUP_EXT0 : Serial.printf("Réveil causé par un signal externe avec RTC_IO [%d]\n",source_reveil); break;
      case ESP_SLEEP_WAKEUP_EXT1 : Serial.println("Réveil causé par un signal externe avec RTC_CNTL"); break;
      case ESP_SLEEP_WAKEUP_TIMER : Serial.println("Réveil causé par un timer"); break;
      case ESP_SLEEP_WAKEUP_TOUCHPAD : Serial.println("Réveil causé par un touchpad"); break;
      default : Serial.printf("Réveil pas causé par le Deep Sleep: %d\n",source_reveil); break;
   }
}




////////////////////// LCD
void testdrawstyles(String str) {
#ifdef LCD
    display.clearDisplay();
  
    display.setTextSize(1);             // Normal 1:1 pixel scale
    display.setTextColor(SSD1306_WHITE);        // Draw white text
    display.setCursor(0, 0);            // Start at top-left corner
    //  display.println(F("GSheet C2:"));
    //  display.setCursor(0,1);             // Start at top-left corner
    display.println(F(str.c_str()));
    display.display();
    delay(2000);
#endif 
}

/*
void send_sheet()
{
  
sensors.requestTemperatures(); // Send the command to get temperatures
   for(int i=0;i<numberOfDevices; i++)
  {
    // Search the wire for address
    if(sensors.getAddress(tempDeviceAddress, i))
    {
      
      // Print the data
      float tempC = sensors.getTempC(tempDeviceAddress);
      String keyy="tmp"+String(tempDeviceAddress[7]);
      String temp=String(tempC);
     // temp.replace('.',',');
     // Serial.println("Temperature for device: "+String(i)+" = "+temp);
     // spreadsheet_get(0,String(numberOfDevices),keyy,temp);//(uint8_t p,String state,String key,String val);
      
    }
  }
    delay(2000);
    int i;
     for(i=0;i<numberOfDevices; i++)
  {
    // Search the wire for address
    if(sensors.getAddress(tempDeviceAddress, i))
    {
      
      // Print the data
      float tempC = sensors.getTempC(tempDeviceAddress);
      String keyy=String(prefix)+String(tempDeviceAddress[7]);
      String temp=String(tempC);
      temp.replace('.',',');
      Serial.println("Temperature for device: "+String(i)+" = "+temp);
      //spreadsheet_get(0,String(numberOfDevices)+";"+String(voltage)+";",keyy,temp);//(uint8_t p,String state,String key,String val);
      spreadsheet_get("key="+keyy+"&val="+temp,0);
      sprintf(s2,"%s%s",keyy,temp);
     Serial2.print(s2); //openlog
     
    }
  }
  for(;i<10; i++)   Serial2.print(";"); //openlog
  Serial2.print(networks); //openlog
   Serial.println("NB Temperature  device: "+String(numberOfDevices));
}
*/

////////////////:deep sleep
void deep_sleep(uint16_t timetosleep)
{
if(digitalRead(0)==0) return; // pas de deep sleep si bouton activé

  // digitalWrite(LED_R,LED_OFF);
  // digitalWrite(LED_V,LED_OFF);
  // digitalWrite(LED_B,LED_OFF);
  digitalWrite(PIN_EN_OUT,0);
  pinMode(PIN_EN_OUT, INPUT);
  // pinMode(LED_R, INPUT);
  // pinMode(LED_V, INPUT);
  // pinMode(LED_B, INPUT);
  pinMode(BLED, INPUT);
  pinMode(PIN_VCCLOG, INPUT);
  digitalWrite(PIN_VCCLOG,0);

  
   ++bootCount;
   Serial.println("----------------------");
   Serial.println(String(bootCount)+ "eme Boot ");

   //Affiche la raison du réveil
   print_wakeup_reason();

   //Configuration du timer
   esp_sleep_enable_ext0_wakeup(GPIO_NUM_0     , 0);
   esp_sleep_enable_timer_wakeup(timetosleep * uS_TO_S_FACTOR);
   Serial.println("ESP32 réveillé dans " + String((int) timetosleep/60) + " minutes" +String((int) timetosleep%60));

   //Rentre en mode Deep Sleep
   Serial.println("Rentre en mode Deep Sleep");
   Serial.println("----------------------");
   delay(10000);
#ifdef RELAIS_OUT
    gpio_deep_sleep_hold_en();
  gpio_hold_en((gpio_num_t) RELAIS1);  
  gpio_hold_en((gpio_num_t) RELAIS2);  
  gpio_hold_en((gpio_num_t) RELAIS3);  
  gpio_hold_en((gpio_num_t) RELAIS4);  
  gpio_hold_en((gpio_num_t) RELAIS5);  
  gpio_hold_en((gpio_num_t) RELAIS6);  
  gpio_hold_en((gpio_num_t) RELAIS7);  
  gpio_hold_en((gpio_num_t) RELAIS8);  

#endif
   esp_deep_sleep_start();
   Serial.println("Ceci ne sera jamais affiché");
}

String parsestr(String data)
{

    int found = 0;
    int strIndex[] = { 0, -1};
    int posegal=0;
    int maxIndex = data.length() - 1;

    for (int i = 0; i <= maxIndex ; i++) 
    {
        if (data.charAt(i) == '=') posegal=i;
        if (data.charAt(i) == ';' || i == maxIndex) 
        {
            found++;
            strIndex[0] = strIndex[1] + 1;
            strIndex[1] = (i == maxIndex) ? i+1 : i;
            String line=data.substring(strIndex[0], strIndex[1]);
            Serial.println("line:" + line);
            if(posegal>0)
            {
              ////Serial.println("==>0");
              line=data.substring(strIndex[0], posegal);
              String arg=data.substring( posegal+1,strIndex[1]); 
              if(line=="h")
              {
             //// Serial.println("==h");
                line=data.substring( posegal+1,strIndex[1]);  
               //// Serial.println("h=!!! = " + line);
                heure=line.substring( 6,8).toInt();  
                minute=line.substring( 8,10).toInt(); 
                seconde=line.substring( 10,12).toInt();
                Serial.println(String(heure)+":" +String(minute)+":" +String(seconde));
                
             //// Serial.println("==h fin");
              }
              else if(line=="interval"&&arg.toInt()<=30*60&&arg.toInt()>=0) interval =(uint16_t) arg.toInt();
              

#ifdef RELAIS_OUT


              else if(line=="R1") {digitalWrite(RELAIS1,!(arg.toInt())); delay(100);}
              else if(line=="R2") {digitalWrite(RELAIS2,!(arg.toInt())); delay(100);}
              else if(line=="R3") {digitalWrite(RELAIS3,!(arg.toInt())); delay(100);}
              else if(line=="R4") {digitalWrite(RELAIS4,!(arg.toInt())); delay(100);}
              else if(line=="R5") {digitalWrite(RELAIS5,!(arg.toInt())); delay(100);}
              else if(line=="R6") {digitalWrite(RELAIS6,!(arg.toInt())); delay(100);}
              else if(line=="R7") {digitalWrite(RELAIS7,!(arg.toInt())); delay(100);}
              else if(line=="R8") {digitalWrite(RELAIS8,!(arg.toInt())); delay(100);}
#endif

          ////  Serial.println("k=" + line);
            }
             //// Serial.println("posegal0");
            posegal = 0;
        }
        
             //// Serial.println(".!.");
    }
            ////  Serial.println("fin!!");
   // return found > index ? data.substring(strIndex[0], strIndex[1]) : "";
   return("");
}


char spreadsheet_get(String g_payload,uint8_t p) {
  int8_t error=-1;
  char retry=0;
  do{
    HTTPClient http;
    //http.setConnectTimeout(10000);
    http.setTimeout(httptimeout);
    String url = "https://script.google.com/macros/s/" + GOOGLE_SCRIPT_ID + "/exec?"+SPREADSHEET_ID+"sheet="+sheet+"&"+g_payload;
    //if(p==1) url+="&hour=1";
    //if(p==2) url+="&p=1";
    //?key=esp&val=8266&state=2&p=12"; //&date=240202
    Serial.println(url);
    Serial.println(g_payload);
    Serial.print("\n Making a request N°: ");
    Serial.println((int) retry);
    http.begin(url.c_str()); //Specify the URL and certificate
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    int httpCode = http.GET();
    String payload;
    if (httpCode > 0) { //Check for the returning code
      payload = http.getString();
  
      Serial.println(httpCode);
      Serial.println(payload);
      if (p>0 ) {
        heure=-1;
        minute=-1;
        seconde=-1;
        parsestr(payload);
       // testdrawstyles(payload);
       
      ledcode=0x0AA0;
      }
      error=1;
    }
    else {
      Serial.println("Error on HTTP request");
      error = -1;
      ledcode=0x0AAF;
    }
    http.end();
  }while( error<0 && ++retry<httpretry);
    return(error);
}

uint16_t calc_time2sleep(uint8_t gmin,uint8_t gsec) // renvoie le nb de secondes avant reveil
{
  if(interval>0) 
  {
    bootCountEn=(1+bootCount)%4; // actif au prochain reveil
    return (interval);
  }
  if(rtc.getMinute()<0 || rtc.getSecond()<0) return 1790; // 30 min - 10s
  uint16_t tts=gmin*60+gsec + 3600 ;
  uint16_t now=rtc.getMinute()*60+rtc.getSecond();
  uint16_t dif=(tts-now-5+3600)%3600; // secondes avant prochaine alarme (heure alarme = gmin*60+gsec secondes) 
  //+ 25sec (> temps execution) pour changer de tranche
  //if( (int) dif<2700) //  
  //{
      bootCountEn=(1+bootCount+(int) dif/900)%4;
    Serial.print("dif  = ");
    Serial.print(dif);
    Serial.print(" bootCount = ");
    Serial.print(bootCount);
    Serial.print(" / ");
    Serial.println(bootCountEn);
  //}
    Serial.print("repos = 15min= x ");
    Serial.print((7+bootCountEn-bootCount)%4);  /// 0 ->3  1-2  2-1 3-0
    Serial.print(" + ");
    Serial.println(((tts-now)%900)/60.0);
  //dif=(tts-now)%3600; // secondes avant prochaine alarme (heure alarme = gmin*60+gsec secondes)
  return((tts-now)%900); /// valeur <2100  pour variable !!! sinon ca plante
}

// Callback function (get's called when time adjusts via NTP)
void timeavailable(struct timeval *t)
{
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){   /*Serial.println("No time available (yet)");*/     return;}
  Serial.print("Got time adjustment NTP:  ");
  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
  rtc.offset = 0; // change offset value
  Serial.print("dif=");
  Serial.println(3600*24*difftime(mktime(&timeinfo),mktime(&time_st)));
  rtc.setTimeStruct(timeinfo);  // NTP --> RTC Mise à l'heure ESP
  int32_t dif= difftime(mktime(&timeinfo),mktime(&time_st));
  if( dif>1000000) logstate+="NTP-RESTART;";
  else logstate+="NTP"+String(dif)+";";
  /*struct tm time_st = rtc.getTimeStruct();
  Serial.print("RTC:    ");
  Serial.println(&time_st, "%A, %B %d %Y %H:%M:%S");   //  (tm struct) Sunday, January 17 2021 07:24:38*/
  
}
#define TDS_N_ECH 20
double mesure_tds()
{
  double ttds=0;
  pinMode(PIN_TDS, INPUT);
      
  for(int i=0;i<TDS_N_ECH;i++)
  {
   // attendre 200ms apres mise sous tension digitalWrite(PIN_EN_OUT,1);
    ttds+=(int) analogRead(PIN_TDS);
      delay(1);
  }
  ttds=ttds/TDS_N_ECH;
  /// conversion ADC -> g/L NaCl:
  /// 0g-> ADC=0
  /// 40g/L ~ 1053.3 
  
  return (ttds*40/1053);
}

void scan_network()
{
  networks="";
   WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  int n = WiFi.scanNetworks();
  Serial.println("scan done");
  if (n == 0)
    Serial.println("no networks found");
  else
  {
    Serial.print(n);
    Serial.println(" networks found");
    for (int i = 0; i < n; ++i)
    {
      // Print SSID and RSSI for each network found
      networks+=String(i)+":"+String(WiFi.SSID(i))+"["+String(WiFi.RSSI(i))+"dB];";
      Serial.print(i + 1);
      Serial.print(": ");
      Serial.print(WiFi.SSID(i));
      Serial.print(" (");
      Serial.print(WiFi.RSSI(i));
      Serial.print(")");
      //Serial.println((WiFi.encryptionType(i) == ENC_TYPE_NONE) ? " " : "*");
      delay(10);
    }
  }
  
  WiFi.disconnect();
}

void gotoCommandMode(void) 
{
  //Send three control z to enter OpenLog command mode
  Serial2.write(26);
  Serial2.write(26);
  Serial2.write(26);
  int tout=0;
  while(tout++<1000) {
    if(Serial2.available())
      if(Serial2.read() == '>') break;
     delay(1); 
  }
  Serial.print("cmd mode=");
  Serial.println(tout);
}
String test_openlog(String cmd)
{
  Serial.print("openlog ");
  gotoCommandMode();
  //Serial2.print("read periscope.gcode");
  Serial2.print(cmd);
  Serial2.write(13);
  String ret="";
   for(int timeOut = 0 ; timeOut < 1000 ; timeOut++) 
   {
    if(Serial2.available()) 
    {
      ret+=Serial2.read(); //Take the character from OpenLog and push it to the Arduino terminal
      timeOut = 0;
    }
     // delay(1);
   }
   
  Serial.print("fin openlog=");
    delay(1000);
  return (ret);
}
String ReadStringEE(int start,int len)
{
  String str="";
  char cee=' ';
  for (int i = start; cee!=0&&i < start+len; ++i)
  {
    cee =EEPROM.read(i);
    if( cee!=0) str += cee;
  }
  return (str);
}
#define FIRST_TEMP_COL  'e'
String get1Wire()
{
   Serial.println("Read 1Wire Temperature ");

  sensors.requestTemperatures(); // Send the command to get temperatures
 /* for(int i=0;i<numberOfDevices; i++)
  {
    // Search the wire for address
    if(sensors.getAddress(tempDeviceAddress, i))
    {
      float tempC = sensors.getTempC(tempDeviceAddress);
      String keyy="tmp"+String(tempDeviceAddress[7]);
      String temp=String(tempC); 
    }
  }*/
    delay(200);
    int i;
    int j=0;
   // String get_Str="";
  for(i=0;i<numberOfDevices; i++)
  {
    if(sensors.getAddress(tempDeviceAddress, i))
    {

//#define EE_1WIREADR 400
//#define NB_1WIRE    16
        j=0;
        String wiredata="";
        while((wiredata=="")&&(EEPROM.read(EE_1WIREADR+2*j)>0)||(EEPROM.read(EE_1WIREADR+2*j+1)>0))
        {
              if( (EEPROM.read(EE_1WIREADR+2*j)==tempDeviceAddress[7])&&(EEPROM.read(EE_1WIREADR+2*j+1)==tempDeviceAddress[6]))
              {
                float tempC = sensors.getTempC(tempDeviceAddress);
                  wiredata=String('&')+String((char) (j+FIRST_TEMP_COL))+String('=')+String(tempC);
                  wiredata.replace('.',',');
                  Serial.println("Temperature for device: "+String(i)+" - "+String(j)+"[adr:"+String(tempDeviceAddress[6])+" "+String(tempDeviceAddress[7])+"] = "+wiredata);

              }
              j++;
        }
    if(wiredata=="") // nouvelle sonde
    {
      float tempC = sensors.getTempC(tempDeviceAddress);
      wiredata=String('&')+String((char) (++cnt_col))+String('=')+String(tempC);
      wiredata.replace('.',',');
      if(j<NB_1WIRE)
      {
          EEPROM.write(EE_1WIREADR+2*j,tempDeviceAddress[7]);
          EEPROM.write(EE_1WIREADR+2*j+1,tempDeviceAddress[6]);
          
          EEPROM.commit();

          Serial.println("New Temperature for device: "+String(i)+" - "+String(j)+"[adr:"+String(tempDeviceAddress[6])+" "+String(tempDeviceAddress[7])+"] = "+wiredata);
      }else{Serial.println("too much Temperature device !!: "+String(i)+" - "+String(j)+"[adr:"+String(tempDeviceAddress[6])+" "+String(tempDeviceAddress[7])+"] = "+wiredata); }
    }
    argStr+=wiredata;
    // Serial2.print(s2); //openlog  
    }
  }
   // return(get_Str);
}
void addStr(double val)
{
  String s=String(val);
  s.replace('.',',');
  argStr+='&'+String(++cnt_col)+'='+s;
}
