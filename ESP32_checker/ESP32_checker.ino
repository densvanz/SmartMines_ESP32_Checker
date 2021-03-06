#include "BluetoothSerial.h"

#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth is not enabled! Please run `make menuconfig` to and enable it
#endif


#define RXD2 14
#define TXD2 12
String RxdChar;
String Rxd_String;

const char apn[]      = "http.globe.com.ph"; 
const char gprsUser[] = ""; 
const char gprsPass[] = ""; 
const char simPIN[]   = "1234"; 
const char server[] = "smartmines-csu.net"; 
String resource = "";       
const int  port = 80;  
String apiKeyValue = "SmartyMines1234";
#define MODEM_RST            5
#define MODEM_PWKEY          4
#define MODEM_POWER_ON       23
#define MODEM_TX             27
#define MODEM_RX             26
#define SerialMon Serial
#define SerialAT Serial1
#define TINY_GSM_MODEM_SIM800      
#define TINY_GSM_RX_BUFFER   1024 

#define I2C_SDA              21
#define I2C_SCL              22

//#define I2C_SDA_2            18
//#define I2C_SCL_2            19


#define uS_TO_S_FACTOR 1000000UL  
#define TIME_TO_SLEEP  3600        
#define IP5306_ADDR          0x75
#define IP5306_REG_SYS_CTL0  0x00

#define LEDPin            13
#define BT_LED            32
//#include <SoftwareSerial.h> 

BluetoothSerial SerialBT;
// Bt_Status callback function
void Bt_Status (esp_spp_cb_event_t event, esp_spp_cb_param_t *param) {

  if (event == ESP_SPP_SRV_OPEN_EVT) {
    Serial.println ("Client Connected");
    digitalWrite (BT_LED, HIGH);
    // Do stuff if connected
  }

  else if (event == ESP_SPP_CLOSE_EVT ) {
    Serial.println ("Client Disconnected");
    digitalWrite (BT_LED, LOW);
    // Do stuff if not connected
  }
}

#include <Wire.h>
#include <TinyGsmClient.h>

// I2C for SIM800 (to keep it running when powered from battery)
TwoWire I2CPower = TwoWire(0);

#ifdef DUMP_AT_COMMANDS
  #include <StreamDebugger.h>
  StreamDebugger debugger(SerialAT, SerialMon);
  TinyGsm modem(debugger);
#else
  TinyGsm modem(SerialAT);
#endif
TinyGsmClient client(modem);

bool setPowerBoostKeepOn(int en){
  I2CPower.beginTransmission(IP5306_ADDR);
  I2CPower.write(IP5306_REG_SYS_CTL0);
  if (en) {
    I2CPower.write(0x37); // Set bit1: 1 enable 0 disable boost keep on
  } else {
    I2CPower.write(0x35); // 0x37 is default reg value
  }
  return I2CPower.endTransmission() == 0;
}


void setup()
{
  //Serial.begin(Baud Rate, Data Protocol, Rxd pin, R=Txd pin);
  //Serial.begin(115200);
  Serial2.begin(9600, SERIAL_8N1, RXD2, TXD2);
  pinMode(LEDPin, OUTPUT);
  pinMode(BT_LED, OUTPUT);
  digitalWrite (BT_LED, LOW);

  SerialBT.begin("SM_Checker"); //Bluetooth device name
  SerialBT.register_callback (Bt_Status);

  SerialMon.begin(115200);
  I2CPower.begin(I2C_SDA, I2C_SCL, 400000);
  //I2CBME.begin(I2C_SDA_2, I2C_SCL_2, 400000);
  bool isOk = setPowerBoostKeepOn(1);
  SerialMon.println(String("IP5306 KeepOn ") + (isOk ? "OK" : "FAIL"));
  pinMode(MODEM_PWKEY, OUTPUT);
  pinMode(MODEM_RST, OUTPUT);
  pinMode(MODEM_POWER_ON, OUTPUT);
  digitalWrite(MODEM_PWKEY, LOW);
  digitalWrite(MODEM_RST, HIGH);
  digitalWrite(MODEM_POWER_ON, HIGH);
  SerialAT.begin(115200, SERIAL_8N1, MODEM_RX, MODEM_TX);
  delay(3000);
  SerialMon.println("Initializing modem...");
  modem.restart();
  if(strlen(simPIN) && modem.getSimStatus() != 3 ) {
    modem.simUnlock(simPIN);
  }
   
}

void loop() {
 
  
  while (Serial2.available()>0) {
    RxdChar = char(Serial2.read());
    delay(1); //wait for the next byte, if after this nothing has arrived it means the text was not part of the same stream entered by the user
    Rxd_String+=RxdChar;
  }

  while(SerialBT.available()>0) {
    RxdChar = (char)SerialBT.read();
    //delay(1); //wait for the next byte, if after this nothing has arrived it means the text was not part of the stream
    Rxd_String+=RxdChar; 
  }

  if(Rxd_String.length()<10&&Rxd_String!=""){
    SerialBT.print(Rxd_String);
    Serial.println(Rxd_String);
  }
 
 if(Rxd_String.length()>15){
    digitalWrite(LEDPin, HIGH);
    
    String raw_res  = getValue(Rxd_String, '?', 0);
    String app_data = getValue(Rxd_String, '?', 1);
        
    resource = "/api/" + raw_res + "?" ; //  "/api/checker/data?";
    
    Serial.println("Resource: " + resource);
    Serial.println("Data: " + app_data);
    Serial.println();
    
    String httpRequestData = "&api_key=" + apiKeyValue +"&"+
                          app_data +"";
    
    SerialMon.println(httpRequestData);
    Serial.println();
    String web_response = SendtoServer(httpRequestData);
    SerialBT.print(web_response); // Send response back to Bluetooth Module in Arduino
    delay(10);
    Serial.println(web_response);
    digitalWrite(LEDPin, LOW);
     
  }
 Rxd_String="";
 delay(10);
   
}

String SendtoServer(String httpRequestData_local){
  String web_response="";
  SerialMon.print("Connecting to APN: ");
  SerialMon.print(apn);
  if (!modem.gprsConnect(apn, gprsUser, gprsPass)) {
    SerialMon.println(" fail");
  }
  else {
    SerialMon.println(" OK");
    
    SerialMon.print("Connecting to ");
    SerialMon.print(server);
    if (!client.connect(server, port)) {
      SerialMon.println(" fail");
    }
    else {
      SerialMon.println(" OK");
      SerialMon.println("Performing HTTP POST request...");
       //String httpRequestData ="api_key=SmartyMines1234&str=httpRequestData_local";
      client.print(String("POST ") + resource + " HTTP/1.1\r\n");
      client.print(String("Host: ") + server + "\r\n");
      client.println("Connection: close");
      client.println("Content-Type: application/x-www-form-urlencoded");
      client.print("Content-Length: ");
      client.println(httpRequestData_local.length());
      client.println();
      client.println(httpRequestData_local);
      //SerialMon.println(httpRequestData_local);
      unsigned long timeout = millis();
      bool start_save = false;
      while (client.connected() && millis() - timeout < 10000L) {
        while (client.available()) {
          char c = client.read();
          if (c == '{')
            start_save = true;
          if(start_save)
            web_response+=String(c);
          
          SerialMon.print(c);
          timeout = millis();
        }
      }
     
      SerialMon.println();
      client.stop();
      SerialMon.println(F("Server disconnected"));
      modem.gprsDisconnect();
      SerialMon.println(F("GPRS disconnected"));
      return web_response;
    }
  }
    
    
}


String getValue(String data, char separator, int index)
{
    int found = 0;
    int strIndex[] = { 0, -1 };
    int maxIndex = data.length() - 1;

    for (int i = 0; i <= maxIndex && found <= index; i++) {
        if (data.charAt(i) == separator || i == maxIndex) {
            found++;
            strIndex[0] = strIndex[1] + 1;
            strIndex[1] = (i == maxIndex) ? i+1 : i;
        }
    }
    return found > index ? data.substring(strIndex[0], strIndex[1]) : "";
}
