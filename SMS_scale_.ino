#include <SoftwareSerial.h>      
#include <DHT.h>                        
#include <HX711.h>                      
#include <Wire.h>                   
#include <RTClib.h>  

#define GSM_TX 9                           
#define GSM_RX 10                          
#define GSM_EN 8                           
SoftwareSerial simSerial(GSM_TX, GSM_RX);

#define DHT_PIN 16                        
#define DHT_ENABLE 17                     
DHT dht(DHT_PIN, DHT22);

#define LOADCELL_DOUT_PIN 6            
#define LOADCELL_SCK_PIN 7                 
HX711 scale; 

#define RTC_SQW 2                        
RTC_PCF8563 rtc;   

#define DIP1 3

float calibration_factor=1000.00 ;         
float offset=1000.00;  

struct Measurement{
  int hour;
  float temperature;
  float humidity;
  float weight;
};
Measurement measurements[3];
int measurementBrojac=0;
bool messageSentThisMinute = false;

bool waitForResponse(String expectedResponse, unsigned long timeout) {
  unsigned long startTime = millis();
  String response = "";
  
  while (millis() - startTime < timeout) {
    if (simSerial.available()) {
      char c = simSerial.read();
      response += c;
      
      if (response.endsWith(expectedResponse)) {
        return true;
      }
    }
  }
  
  return false;
}

void setup() {                           
  Serial.begin(9600);                      
  simSerial.begin(9600);                   

  delay(1000);
  Serial.println("Krecemo!!");             
  
  pinMode(DIP1, OUTPUT);                   
  digitalWrite(DIP1,LOW);                  
  Serial.println("Ugašen DIP1!!");         
    
  pinMode(GSM_EN, OUTPUT);                
  pinMode(DHT_ENABLE, OUTPUT);             
  pinMode(RTC_SQW, INPUT_PULLUP);         
  
  if (! rtc.begin()) {                     
    Serial.println("Ne može se inicijalizirati RTC");  
    Serial.flush();                       
    while (1) delay(10);                  
  }
  delay(1000);                             
  Serial.println("Postavljanje dht i rtc");   
  dht.begin();                            
  delay(1000);

  scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);     
  
  rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  delay(2000);
  Serial.println("Započinje rad");      

  mjerenje();
  
  digitalWrite(DHT_ENABLE, LOW);                          
  digitalWrite(GSM_EN, LOW);                              
  Serial.println("Ugašeni senzor i dht22 modul");        
}

void loop() {                             
  DateTime now=rtc.now();             
  Serial.print(now.hour(), DEC);       
  Serial.print(":");
  Serial.print(now.minute(), DEC);      
  int currentHour=now.hour();           
  int currentMinute=now.minute();       
  int currentSecond=now.second();
  
  String provjera;                     
  String odgovor="+CMGS:";
  
  sendAtSpecificTime();         
  if (currentHour == 12 && currentMinute == 50 && currentSecond==0) {
    if(!messageSentThisMinute){
      sendCollectedData();

      messageSentThisMinute=true;
      digitalWrite(DHT_ENABLE, LOW);
      digitalWrite(GSM_EN, LOW);
    }
    else{
      messageSentThisMinute=false;
    }
  }
}

void sendAtSpecificTime(){   
  DateTime now=rtc.now();               
  int currentHour=now.hour();           
  int currentMinute=now.minute();       
  Serial.println("Još ne čitamo vrijem");  
  Serial.print(now.hour(), DEC);
  Serial.print(":");
  Serial.print(now.minute(), DEC);

  String provjera;                      
  String odgovor="+CMGS:";             
  
  if(currentHour==12 && currentMinute==44 ||
     currentHour==12 && currentMinute==46 ||
     currentHour==12 && currentMinute==48){  
      
    Serial.println("Isto je vrijeme zapocinjem mjerenje");   
    mjerenja();

    digitalWrite(DHT_ENABLE, LOW);                          
    digitalWrite(GSM_EN, LOW);                             
    Serial.println("Ugašeni senzor i dht22 modul");         
    
    provjera=simSerial.readString();                        
  }
}

void measureTemperatureAndHumidity(float &temperature, float &humidity){    
  float temp;                           
  float hum;

  digitalWrite(DHT_ENABLE, HIGH);      
  delay(15000);                         
  
  temp=dht.readTemperature();           
  delay(3000);                          
  hum=dht.readHumidity();              
  delay(3000);

  if(isnan(hum) || isnan(temp)){                   
    Serial.println("ne može se procitati podaci sa senzora!"); 
    return;
  }

  temperature=temp;                    
  humidity=hum;
}


void sendSMS(String message) {          
  digitalWrite(GSM_EN, HIGH);          
  delay(3000);
  
  Serial.println("Initializing...");    
  delay(1000);

  simSerial.println("AT");              
  delay(2000);
  updateSerial();                      

  simSerial.println("AT+CMGF=1");      
  delay(2000);
  updateSerial();

  simSerial.println("AT+CMGS=\"+38761890378\"");    
  updateSerial();

  simSerial.print(message);             
  updateSerial();

  simSerial.write(26);                  
  delay(5000);                          

  Serial.println("Poruka poslana.");    
}


void updateSerial() {                   
  delay(1000);
  while (Serial.available()) {          
    simSerial.write(Serial.read());     
  }delay(1000);
  while (simSerial.available()) {       
    Serial.write(simSerial.read());    
  }
}

void MeasurementWeight(float weight){    
                                          
  int numReadings=20;                    
  long sum=0;                           
  for(int i=0;i<numReadings;i++){      
    sum+=scale.read_average();         
    delay(1000);
  }
  long averageReading=sum/numReadings;   

  weight=(averageReading-offset)/calibration_factor;  
  return weight;
}


void mjerenja(){
  DateTime now=rtc.now();
  float temperature;                  
  float humidity;                     
  float weight;                       
  int currentHour=now.hour();

  Serial.println("započelo je mjerenje u ciljane sate");
  measureTemperatureAndHumidity(temperature, humidity);   
  MeasurementWeight(weight);

  if(measurementBrojac<3){
    measurements[measurementBrojac] = {rtc.now().hour(), temperature, humidity, weight};
    measurementBrojac++;
  }
  delay(10000);
}

String dataStorage[3];

void saveDataToMemory(String data){
    for(int i=0; i<3; i++){
      if(dataStorage[i].length()==0){
        dataStorage[i]=data;
        break;
      }
    }
}

void sendDataFromMemory(){
  for(int i=0; i<3; i++){
    if(dataStorage[i].length()>0){
      sendSMS(dataStorage[i]);
      dataStorage[i]="";
    }
  }
  digitalWrite(GSM_EN, LOW);
}

String createDataString(int hour, float temperature, float humidity, float weight){
  String data=String(hour)+"h: "+ "Temp: "+String(temperature)+" °C, " +
  "Vlaga: " + String(humidity) + "%, " +
                "Tezina: " + String(weight) + " kg";
  return data;
}

void mjerenje(){
  float temperature;
  float humidity;
  float weight;

  Serial.println("započinje mjerenje funkcija");
  measureTemperatureAndHumidity(temperature, humidity);
  MeasurementWeight(weight);

  sendSMS(message(temperature, humidity, weight));
}

String message(float temperature, float humidity, float weight){
  String message="Temp: "+String(temperature)+" °C \n Vlaga: "+String(humidity)+"% \n Tezina je: "+String(weight)+"kg \n Vaga je spremna za rad";
  return message;
}

void sendCollectedData(){
  Serial.println("započinje sendCollectData");
  delay(3000);
  String message="Dnevni izvještaj:\n";
  for(int i=0;i<measurementBrojac;i++){
    message += createDataString(measurements[i].hour, measurements[i].temperature, measurements[i].humidity, measurements[i].weight) + "\n";
  }
  delay(5000);
  sendSMS(message);
  waitForResponse("+CMGS:", 5000);
  Serial.println("završava sendCollectData");
  measurementBrojac=0;
}
