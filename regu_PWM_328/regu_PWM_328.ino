
#include <TimerOne.h>
#define PIN_1WIRE   3

#define EE_1WIREADR 10
#define NB_1WIRE    16

#define FIRST_TEMP_COL  'e'
uint8_t cnt_col=FIRST_TEMP_COL;

#define PIN_FROID 11
#define CLEAR_FROID PORTB|=0x04; //   0000 1000 D11:PB3
#define SET_FROID PORTB&=0xF7; //   

#define PIN_CHAUD 12
#define CLEAR_CHAUD PORTB|=0x10; //   0001 0000 D12:PB4
#define SET_CHAUD PORTB&=0xEF; //  

#define SET_P0 PORTB|=0x02; //   0000 0010 D9:PB1
#define CLEAR_P0 PORTB&=0xFD; //  

#define NB_REGU 1

#include <EEPROM.h>

#include <OneWire.h>
#include <DallasTemperature.h>
// GPIO where the DS18B20 is connected to
const int oneWireBus = PIN_1WIRE;     
// Setup a oneWire instance to communicate with any OneWire devices
OneWire oneWire(oneWireBus);
// Pass our oneWire reference to Dallas Temperature sensor 
DallasTemperature sensors(&oneWire);
int numberOfDevices=0; 

DeviceAddress tempDeviceAddress; 
//String getStr="";

char id_board='e';
uint8_t verb=0;

float ctA=20;

float temperature[16];
float consigne[16];
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////    IT    ////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    #define PWMOUTCNT 4
    uint8_t pwm_cnt=0;
    uint8_t pwm_ch_pos=50; // instant du mode chauffage (par defaut: 50  -> 0-49 = froid  50-99 = chaud)
    uint8_t pwm_up[]={0,0,0,0,0,0,0,0};    /// instant de declenchement haut 0-100 (>100: off)
    uint8_t pwm_down[]={0,0,0,0,0,0,0,0};    /// instant de declenchement OFF 0-100 (>100: off)
    //uint8_t pwm_ch_pin[]={0,0,0,0,0,0,0,0}; // pin RPWM pont H
    uint8_t pwm_pin[]={0,0,0,0,0,0,0,0}; // pin LPWM pont h
   //uint8_t pwm_chfr[]={0,0,0,0,0,0,0,0};   // 0 - froid 1 - chaud
volatile uint32_t isrCounter = 0;
volatile uint32_t lastIsrAt = 0;
volatile uint32_t isrCnd_pid = 0;
uint8_t decl_pid=0;

void  onTimer()
{

  isrCounter++;
  //lastIsrAt = millis();
  pwm_cnt++;
  if(pwm_cnt==100)
  {
    pwm_cnt=0;
    SET_CHAUD
    CLEAR_FROID
  }
  if(pwm_cnt==pwm_ch_pos)
  {
    SET_FROID
    CLEAR_CHAUD
  }
    if(pwm_up[0]==pwm_cnt) SET_P0
    else if(pwm_down[0]==pwm_cnt) CLEAR_P0
    /*
    if(pwm_up[1]==pwm_cnt) SET_P1
    else if(pwm_down[1]==pwm_cnt) CLEAR_P1
    
    if(pwm_up[2]==pwm_cnt) SET_P2
    else if(pwm_down[2]==pwm_cnt) CLEAR_P2
    */

//#endif
isrCnd_pid++;
if(isrCnd_pid>=10000)
  {
    isrCnd_pid=0;
    decl_pid=1;
  }
}

///////////////////////////////////////////////:
void setup() {
  // put your setup code here, to run once:
    Serial.begin(115200); 

  Timer1.initialize(1000);
  Timer1.attachInterrupt(onTimer); // blinkLED to run every 0.0001 seconds

  
  pinMode(PIN_FROID, OUTPUT);
  pinMode(PIN_CHAUD, OUTPUT);
  pinMode(10, OUTPUT);
  
  pinMode(2, OUTPUT);
  digitalWrite(2,1);
          delay(10);

  for(int i =0;i<16;i++) consigne[i]=20;
  
  if(verb) Serial.print("start:  ");
  
 read_ee();
  
//  EEPROM.begin(1024); //Initialasing EEPROM
  
  sensors.begin();
  // Grab a count of devices on the wire
  numberOfDevices = sensors.getDeviceCount();
  if(verb) Serial.println("nb temp captor ="+String(numberOfDevices));

  if(EEPROM.read(EE_1WIREADR)==255)
{
 if(verb) Serial.println("clearing eeprom");
        for (int i = EE_1WIREADR; i < EE_1WIREADR + 100; ++i) {
          EEPROM.write(i, 0x00);
        }
//        EEPROM.commit();
}
}
int8_t cnt_essai=0;
////////////////////////////////////////////////////////////
void loop() {
    while (Serial.available() > 0) { parser(Serial.read());}
      
    
   if(decl_pid) 
   {
   get1Wire();
     for(uint8_t channel=0;channel<NB_REGU;channel++) 
     {
      set_pwm(channel,calc_pid(channel));//temperature[0],consigne[0]));
     }
    decl_pid=0;
   }

   /*
   set_pwm(0,cnt_essai++);
   if(cnt_essai==50) cnt_essai=-50;
           Serial.print("cnt_essai=");
                  Serial.println(cnt_essai);
   */
          delay(100);
  
}
String tempStr="";


/////////////////////////////////////////////////////////////////////////
///// lit temperature sur 1 wire disponible 
///// si capteur inconnu, ajoute son adresse (2Bytes) en EEprom
///// si connu en EEPROM inscrit sa temperature dans l'ordre des nouveaux capteurs inscrits dans float "temperature[n]"
void get1Wire()
{
   //if(verb) Serial.println("Read 1Wire Temperature ");

  sensors.requestTemperatures(); // Send the command to get temperatures

    delay(200);
    int i;
    int j=0;
    
       uint8_t newtemp=1;
  for(i=0;i<numberOfDevices; i++)
  {
    if(sensors.getAddress(tempDeviceAddress, i))
    {
       newtemp=1;
       j=0;
        while((newtemp==1)&&(EEPROM.read(EE_1WIREADR+2*j)>0)||(EEPROM.read(EE_1WIREADR+2*j+1)>0))
        {
              if( (EEPROM.read(EE_1WIREADR+2*j)==tempDeviceAddress[7])&&(EEPROM.read(EE_1WIREADR+2*j+1)==tempDeviceAddress[6]))
              {
                temperature[j] = sensors.getTempC(tempDeviceAddress);
                newtemp=0;
                
                 if(verb) 
                 {  Serial.print("Temperature [");
                  Serial.print(i);
                  Serial.print("]{");
                  Serial.print(j);
                  Serial.print("}=");
                  Serial.print(temperature[j]);
                  }
              }
              j++;
        }
        if(newtemp) // nouvelle sonde
        {
          if(j<NB_1WIRE)
          {
              EEPROM.write(EE_1WIREADR+2*j,tempDeviceAddress[7]);
              EEPROM.write(EE_1WIREADR+2*j+1,tempDeviceAddress[6]);
              
        //      EEPROM.commit();
        
                  if(verb)
                  {Serial.print("j=");
                  Serial.println(j);
    
              Serial.println("New Temperature ");
                  Serial.print((EE_1WIREADR+2*j));
                  Serial.print("  ");
                  Serial.print(EEPROM.read(EE_1WIREADR+2*j));
                  Serial.print("  ");
        
                  Serial.print(EEPROM.read(EE_1WIREADR+2*j+1));
                  Serial.print("  ");
                  Serial.println("");
                  j-=1;
                  Serial.print((EE_1WIREADR+2*j));
                  Serial.print("  ");
                  Serial.print(EEPROM.read(EE_1WIREADR+2*j));
                  Serial.print("  ");
        
                  Serial.print(EEPROM.read(EE_1WIREADR+2*j+1));
                  Serial.print("  ");
                  Serial.println("");
                  }
                  
           }else{
            if(verb) Serial.println("too much Temperature device !!");//: "+String(i)+" - "+String(j)+"[adr:"+String(tempDeviceAddress[6])+" "+String(tempDeviceAddress[7])+"] = "+wiredata); }
                } 
        }
    }
  }
}


    
    float pid_p[8]={0,0,0,0,0,0,0,0},pid_i[8]={0,0,0,0,0,0,0,0},pid_d[8]={0,0,0,0,0,0,0,0};
    
      float pid_r[8]={0,0,0,0,0,0,0,0};
   // float pid_fp=10,pid_fi=0.01,pid_fd=10;
    uint8_t pid_max_p=40,pid_max_i=40,pid_max_d=40,pid_max=40,pid_min=-40;
    float last_temp[8]={0,0,0,0,0,0,0,0};
    float coef_p=5,coef_i=0.1,coef_d=5;
    
    float calc_pid(uint8_t channel)//float temp,float consigne)
    {
     
      pid_p[channel]=consigne[channel]-temperature[channel];
      pid_i[channel]+=consigne[channel]-temperature[channel];
      pid_d[channel]=temperature[channel]-last_temp[channel];
      if(pid_p[channel]>(float) pid_max_p) pid_p[channel]=pid_max_p;
      if(pid_p[channel]<-(float) pid_max_p) pid_p[channel]=-(float) pid_max_p;
      if(pid_i[channel]>(float) pid_max_i) pid_i[channel]=pid_max_i;
      if(pid_i[channel]<-(float) pid_max_i) pid_i[channel]=-(float) pid_max_i;
      if(pid_d[channel]>(float) pid_max_d) pid_d[channel]=pid_max_d;
      if(pid_d[channel]<-(float) pid_max_d) pid_d[channel]=-(float) pid_max_d;
      pid_r[channel]=pid_p[channel]*coef_p+pid_i[channel]*coef_i+pid_d[channel]*coef_d;
      if(pid_r[channel]>(float) pid_max) pid_r[channel]=pid_max;
      if(pid_r[channel]<-(float) pid_min) pid_r[channel]=(float) pid_min;
                  if(verb) 
                  {Serial.print("   P=");
                  Serial.print(pid_p[channel]);
                  Serial.print("  i=");
                  Serial.print(pid_i[channel]);
                  Serial.print("  D=");
                  Serial.print(pid_d[channel]);
                  Serial.print("           PID=");
                  Serial.println(pid_r[channel]);
                  }
      
      last_temp[channel]=temperature[channel];
      
      return(pid_r[channel]);
    }
    
    void read_ee()
    {
                  if(verb) Serial.println("Read EEprom:");
      for (int i=EE_1WIREADR;i<EE_1WIREADR+16;i+=2)
      {
        
                  if(verb) 
                  {
                    Serial.print(i);
                  
                  Serial.print("    ");
        
                  Serial.print(EEPROM.read(i));
                  Serial.print("  ");
        
                  Serial.print(EEPROM.read(i+1));
                  Serial.print("  ");
                  Serial.println("");
                  }
      }
      
    }
#define CONSIGNE_PWM_MIN  5
#define CONSIGNE_PWM_MAX  50

    void set_pwm(uint8_t temp_no,int8_t consigne) // consigne = -100% -> + 100%
    {
        if(consigne>CONSIGNE_PWM_MIN && consigne<CONSIGNE_PWM_MAX)
        {
            pwm_up[temp_no]=0;
            pwm_down[temp_no]=(uint8_t) consigne;
        }
        else if(consigne<-CONSIGNE_PWM_MIN && consigne>-CONSIGNE_PWM_MAX)
        {
            pwm_up[temp_no]=50;
            pwm_down[temp_no]=(uint8_t) (50-consigne);
        }
        else
        {
            pwm_up[temp_no]=110; // >100 --> jamais appelé
            pwm_down[temp_no]=0;
        }
    }
#define PARSER_SIZE 12
char parser_cmd[PARSER_SIZE],parser_data[PARSER_SIZE];
uint8_t cmd_ptr=0,data_ptr=0,parser_mode=0;

void parser(char car)
{
    if(car=='\n'||car=='r')
    {
      parser_data[data_ptr]=0;
      parser_cmd[cmd_ptr]=0;
      
      parser_decode();
      
      parser_mode=0;
      cmd_ptr=0;
      data_ptr=0;
    }
    else if (car=='=')
    {
      parser_cmd[cmd_ptr]=0;
      parser_mode=1;
    }
    else
    {
      if(parser_mode==0)
      {
        parser_cmd[cmd_ptr]=car;
        if(cmd_ptr<PARSER_SIZE)  cmd_ptr++;
      }
      else
      {
        parser_data[data_ptr]=car;
        if(data_ptr<PARSER_SIZE)  data_ptr++;
      }
    }
  
}
void parser_decode()
{
  if(parser_cmd[0]!=id_board&&parser_cmd[0]!='_') return;
  
  if(strcmp(&parser_cmd[1],"v")==0) verb=atoi(parser_data);
  else if(strcmp(&parser_cmd[1],"ctA")==0) ctA=atof(parser_data);
  else if(strcmp(&parser_cmd[1],"coefP")==0) coef_p=atof(parser_data);
  else if(strcmp(&parser_cmd[1],"coefI")==0) coef_i=atof(parser_data);
  else if(strcmp(&parser_cmd[1],"coefD")==0) coef_d=atof(parser_data);//pid_max_p
  else if(strcmp(&parser_cmd[1],"pid_max_p")==0) pid_max_p=atof(parser_data);
  else if(strcmp(&parser_cmd[1],"pid_max_i")==0) pid_max_i=atof(parser_data);
  else if(strcmp(&parser_cmd[1],"pid_max_d")==0) pid_max_d=atof(parser_data);
  else if(strcmp(&parser_cmd[1],"pid_max")==0) pid_max=atof(parser_data);
  else if(strcmp(&parser_cmd[1],"pid_min")==0) pid_min=atof(parser_data);
  else if(strcmp(&parser_cmd[2],"-ct")==0) consigneTemp(parser_cmd[1],atof(parser_data));
  else if(strcmp(&parser_cmd[1],"tmp")==0) send_mesure(1);
  else if(strcmp(&parser_cmd[1],"pid")==0) send_pid(atoi(parser_data));
  else if(strcmp(&parser_cmd[1],"get")==0) send_mesure(atoi(parser_data));
  else if(strcmp(&parser_cmd[1],"board")==0) send_mesure(0);
  else
  {
    if(verb) { Serial.print("inconnu "); Serial.print(parser_cmd); Serial.print("="); Serial.println(parser_data); }
    return;
  }
  if(verb) { Serial.print("command : "); Serial.print(parser_cmd); Serial.print("="); Serial.println(parser_data); }
    

  
}

void send_mesure(uint8_t type)
{
   if(type==0)
   {
                  Serial.print("y=");
                  Serial.print(id_board); //printfloat(pid_p);
                  Serial.print("&");
   }
  if(type&0x01) 
      for(int i =0;i<numberOfDevices;i++)
      {
          Serial.print((char) (id_board+i)); //'e' + i
          Serial.print("="); //
          Serial.print(temperature[i]); //printfloat(temperature[i]);
          Serial.print("&"); //
      }
   if(type&0x02)
   {
                 
   }
   
}
void send_pid(uint8_t voie)
{
                  Serial.print("u=");
                  Serial.print(pid_p[voie]); //printfloat(pid_p);
                  Serial.print("&v=");
                  Serial.print(pid_i[voie]); //printfloat(pid_i);
                  Serial.print("&w=");
                  Serial.print(pid_d[voie]); //printfloat(pid_d);
                  Serial.print("&x=");
                  Serial.print(pid_r[voie]); //printfloat(pid_r);
                  Serial.print("&");
  
}
void printfloat(float f)
{
  /*
  char s[12];
  sprintf(s,"%f",f);
          for(int j=0;j<12;j++) if(s[j]==',') s[j]=='.';
          */
          Serial.print(f); //
  
}

void consigneTemp(char lettre,float value) 
{
  uint8_t notemp=(uint8_t) lettre -(uint8_t)  id_board;
  if(notemp<16) consigne[notemp]=value;
}
