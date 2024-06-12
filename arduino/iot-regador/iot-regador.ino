#include "Credentials.h"
#include "Tipos.h"
#include "ListaEncadeada.h"
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <AsyncElegantOTA.h>
#include <WiFiClientSecure.h>
#include <ESP8266mDNS.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <Preferences.h>
#include <LittleFS.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <Regexp.h>

const char LITTLEFS_ERROR[] PROGMEM = "Erro ocorreu ao tentar montar LittleFS";

#define DEBUG
#define SERIAL_PORT                115200
//Rest API
#define HTTP_REST_PORT             80

#define DEFAULT_VOLUME             70
/* ports */
#define D0                         16
#define D1                         5
#define D2                         4
#define D3                         0
#define D4                         2
#define D5                         14
#define D6                         12
#define D7                         13
#define D8                         15
#define D9                         16

#define RelayWater                 D8
#define RelayLight                 D7
#define RelayLevel                 D6

#define MAX_STRING_LENGTH          2000
#define MAX_PATH                   256
#define MAX_FLOAT                  5

/* 200 OK */
#define HTTP_OK                    200
/* 204 No Content */
#define HTTP_NO_CONTENT            204
/* 400 Bad Request */
#define HTTP_BAD_REQUEST           400
#define HTTP_UNAUTHORIZED          401
#define HTTP_INTERNAL_SERVER_ERROR 500
#define HTTP_NOT_FOUND             404
#define HTTP_CONFLICT              409

Preferences preferences;
//---------------------------------//

/* versão do firmware */
const char version[] PROGMEM = API_VERSION;

// Lista de sensores
ListaEncadeada<ArduinoSensorPort*> sensorListaEncadeada = ListaEncadeada<ArduinoSensorPort*>();

// Lista de aplicacoes do jenkins
ListaEncadeada<Agenda*> agendaListaEncadeada = ListaEncadeada<Agenda*>();

// Create an ESP8266 WiFiClient class to connect to the MQTT server.
WiFiClient espClient;
PubSubClient mqttClient(espClient);

AsyncWebServer server(HTTP_REST_PORT);               // initialise webserver

// Search for parameter in HTTP POST request
const char* PARAM_INPUT_1 = "ssid";
const char* PARAM_INPUT_2 = "pass";
const char* PARAM_INPUT_3 = "ip";
const char* PARAM_INPUT_4 = "gateway";

//Variables to save values from HTML form
String ssid;
String pass;
String ip;
String gateway;

IPAddress localIP;
//IPAddress localIP(192, 168, 1, 200); // hardcoded
// Set your Gateway IP address
IPAddress localGateway;
//IPAddress localGateway(192, 168, 1, 1); //hardcoded
IPAddress subnet(255, 255, 255, 0);
IPAddress dns(1, 1, 1, 1);               // CloudFlare DNS server

// Timer variables
unsigned long previousMillis = 0;
const long interval = 10000;  // interval to wait for Wi-Fi connection (milliseconds)
bool WIFI_CONFIG = false;

//Fuso Horário
int timeZone = -3;

// Define NTP Client to get time
WiFiUDP ntpUDP;
NTPClient timeClient(
                      ntpUDP,                 //socket udp
                      "0.br.pool.ntp.org");

String getContent(const char* filename) {
  String payload="";  
  bool exists = LittleFS.exists(filename);
  if(exists){
    File file = LittleFS.open(filename, "r"); 
    String mensagem = "Falhou para abrir para leitura";
    if(!file){    
      #ifdef DEBUG
        Serial.println(mensagem);
      #endif
      return mensagem;
    }
    while (file.available()) {
      payload += file.readString();
    }
    file.close();
  } else {
    Serial.println(LITTLEFS_ERROR);
  }  
  return payload;
}

String getContentType(String filename) { // convert the file extension to the MIME type
  if (filename.endsWith(".html")) return "text/html";
  else if (filename.endsWith(".css")) return "text/css";
  else if (filename.endsWith(".js")) return "application/javascript";
  else if (filename.endsWith(".ico")) return "image/x-icon";
  else if (filename.endsWith(".png")) return "image/png";
  else if (filename.endsWith(".jpg")) return "image/jpg";
  else if (filename.endsWith(".json")) return "application/json";
  return "text/plain";
}

bool writeContent(String filename, String content){
  File file = LittleFS.open(filename, "w");
  return writeContent(&file, content);
}

bool writeContent(File * file, String content){
  if (!file) {    
    #ifdef DEBUG
      Serial.println(F("Falhou para abrir para escrita"));
    #endif
    return false;
  }
  if (file->print(content)) {    
    #ifdef DEBUG
      Serial.println(F("Arquivo foi escrito"));
    #endif
  } else {    
    #ifdef DEBUG
      Serial.println(F("Falha ao escrever arquivo"));
    #endif
  }
  file->close();
  return true; 
}

// Make size of files human readable
// source: https://github.com/CelliesProjects/minimalUploadAuthESP32
String humanReadableSize(const size_t bytes) {
  if (bytes < 1024) return String(bytes) + " B";
  else if (bytes < (1024 * 1024)) return String(bytes / 1024.0) + " KB";
  else if (bytes < (1024 * 1024 * 1024)) return String(bytes / 1024.0 / 1024.0) + " MB";
  else return String(bytes / 1024.0 / 1024.0 / 1024.0) + " GB";
}

String getDataHora() {
    time_t nowSecs = millis()/1000;
    struct tm timeinfo;
    char time_converted[80];
    gmtime_r(&nowSecs, &timeinfo);
    // HH:MM:SS
    strftime (time_converted,80,"%H:%M:%S",&timeinfo);    
    return String(time_converted);
}

time_t getHoraAgora() {
    struct tm myTime;
       
    time_t epochTime = timeClient.getEpochTime();  
    String formattedTime = timeClient.getFormattedTime(); 
    int currentHour = timeClient.getHours(); 
    int currentMinute = timeClient.getMinutes();    
    int currentSecond = timeClient.getSeconds();
    //Get a time structure
    struct tm *ptm = gmtime ((time_t *)&epochTime);   
    int monthDay = ptm->tm_mday; 
    int currentMonth = ptm->tm_mon;
    int currentYear = ptm->tm_year;
    myTime.tm_sec = currentSecond;       // seconds after the minute [0-60]
    myTime.tm_min = currentMinute;      // minutes after the hour [0-59]
    myTime.tm_hour = currentHour;     // hours since midnight [0-23]
    myTime.tm_mday = monthDay;     // day of the month [1-31]
    myTime.tm_mon = currentMonth;       // months since January [0-11]
    myTime.tm_year = currentYear;    // years since 1900
    myTime.tm_isdst = -1;    // daylight saving time flag (-1 for unknown)
    time_t myTime_t = mktime(&myTime);

    return myTime_t;
}

int searchList(String dataAgenda) {
  Agenda *agd;  
  for(int i = 0; i < agendaListaEncadeada.size(); i++){
    // Obtem a aplicação da lista
    agd = agendaListaEncadeada.get(i);
    if (dataAgenda==agd->dataAgenda) {
      return i;
    }
  }  
  return -1;
}

String getData(uint8_t *data, size_t len) {
  char raw[len];
  for (size_t i = 0; i < len; i++) {
    //Serial.write(data[i]);
    raw[i] = data[i];
  }
  raw[len]=0x00;
  return String(raw);
}

String IpAddress2String(const IPAddress& ipAddress)
{
    return (String(ipAddress[0]) + String(".") +
           String(ipAddress[1]) + String(".") +
           String(ipAddress[2]) + String(".") +
           String(ipAddress[3]));
}

void setClock() {
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
}

bool addSensor(byte id, byte gpio, byte status, const char* name) {
  ArduinoSensorPort *arduinoSensorPort = new ArduinoSensorPort(); 
  arduinoSensorPort->id = id;
  arduinoSensorPort->gpio = gpio;
  arduinoSensorPort->status = status;
  arduinoSensorPort->name = name;
  pinMode(gpio, OUTPUT);

  // Adiciona sensor na lista
  sensorListaEncadeada.add(arduinoSensorPort);
  return true;
}

bool loadSensorList()
{
  bool ret = false;
  ret=addSensor(1, RelayWater, LOW, "water");
  if(!ret) return false;
  ret=addSensor(2, RelayLight, LOW, "light");
  if(!ret) return false;
  ret=addSensor(3, RelayLevel, LOW, "level");
  if(!ret) return false;  
  return true;
}

bool readBodySensorData(byte status, byte gpio) {
  #ifdef DEBUG
    Serial.println(status);
  #endif
  ArduinoSensorPort * arduinoSensorPort = searchListSensor(gpio);
  if(arduinoSensorPort!=NULL) {    
    arduinoSensorPort->status = status;
    return true;
  }
  return false;
}

ArduinoSensorPort * searchListSensor(byte gpio) {
  ArduinoSensorPort *arduinoSensorPort;
  for(int i = 0; i < sensorListaEncadeada.size(); i++){
    // Obtem a aplicação da lista
    arduinoSensorPort = sensorListaEncadeada.get(i);
    if (gpio == arduinoSensorPort->gpio) {
      return arduinoSensorPort;
    }
  }
  return NULL;
}

String readSensor(byte gpio){
  String data="";
  ArduinoSensorPort *arduinoSensorPort = searchListSensor(gpio);  
  if(arduinoSensorPort != NULL) {
    arduinoSensorPort->status=digitalRead(gpio);
    data="{\"id\":\""+String(arduinoSensorPort->id)+"\",\"name\":\""+String(arduinoSensorPort->name)+"\",\"gpio\":\""+String(arduinoSensorPort->gpio)+"\",\"status\":\""+String(arduinoSensorPort->status)+"\"}";
  }
  return data;
}

String readSensorStatus(byte gpio){
  return String(digitalRead(gpio));
}

bool validaHora(String hora) {
  char buf[MAX_PATH];
  memset(buf, 0x00, MAX_PATH);
  strcpy(buf, hora.c_str());
  MatchState ms;
  ms.Target (buf);
  unsigned int count = ms.MatchCount ("[0-9][0-9]:[0-9][0-9]");
  Serial.println("count="+String(count));
  if(count == 1) return true;
  return false;
}

void addAgenda(String dataAgenda) {
  int ind = dataAgenda.indexOf(':');
  String hora = dataAgenda.substring(0, ind);
  String minutos = dataAgenda.substring(ind+1, dataAgenda.length());  
  Agenda *agdnew = new Agenda();  
  agdnew->dataAgenda = dataAgenda;
  agendaListaEncadeada.add(agdnew);
}

void saveAgendaList() {
  Agenda *agd;
  String JSONmessage;
  for(int i = 0; i < agendaListaEncadeada.size(); i++){
    // Obtem a aplicação da lista
    agd = agendaListaEncadeada.get(i);
    JSONmessage += "{\"dataAgenda\": \""+agd->dataAgenda+"\"}"+",";
  }
  JSONmessage = "["+JSONmessage.substring(0, JSONmessage.length()-1)+"]";
  // Grava no storage
  writeContent("/lista.json",JSONmessage); 
  // Grava no adafruit
  mqttClient.publish((String(MQTT_USERNAME)+String("/feeds/list")).c_str(), JSONmessage.c_str());
}

int loadAgendaList() {
  // Carrega do storage
  String JSONmessage = getContent("/lista.json");
  if(JSONmessage == "") {    
    #ifdef DEBUG
      Serial.println(F("Lista local de eventos vazia"));
    #endif
    return -1;
  } else {
    DynamicJsonDocument doc(MAX_STRING_LENGTH);
    DeserializationError error = deserializeJson(doc, JSONmessage);
    if (error) {
      return 1;
    }
    for(int i = 0; i < doc.size(); i++){
      addAgenda(doc[i]["dataAgenda"]);
    }    
  }
  return 0;
}

// Called when there's a warning or error (like a buffer underflow or decode hiccup)
void StatusCallback(void *cbData, int code, const char *string)
{
  const char *ptr = reinterpret_cast<const char *>(cbData);
  // Note that the string may be in PROGMEM, so copy it to RAM for printf
  char s1[64];
  strncpy_P(s1, string, sizeof(s1));
  s1[sizeof(s1)-1]=0;
  Serial.printf("STATUS(%s) '%d' = '%s'\n", ptr, code, s1);
  Serial.flush();
}

// Initialize WiFi
bool initWiFi() {
  if(ssid=="" || ip==""){
    Serial.println("SSID ou endereço IP indefinido.");
    return false;
  }

  WiFi.mode(WIFI_STA);
  localIP.fromString(ip.c_str());
  localGateway.fromString(gateway.c_str());

  if (!WiFi.config(localIP, localGateway, subnet, dns)){
    Serial.println("STA Falhou para configurar");
    return false;
  }
  WiFi.begin(ssid.c_str(), pass.c_str());
  Serial.println("Conectando ao WiFi...");

  unsigned long currentMillis = millis();
  previousMillis = currentMillis;

  while(WiFi.status() != WL_CONNECTED) {
    currentMillis = millis();
    Serial.println("currentMillis: "+currentMillis);
    Serial.println("previousMillis: "+previousMillis);
    Serial.println("interval: "+interval);    
    if (currentMillis - previousMillis >= interval) {
      Serial.println("Falhou para conectar.");
      return false;
    }
  }
  Serial.println(WiFi.localIP());
  return true;
}

void setup() {
  Serial.begin(SERIAL_PORT);

  // métricas para prometheus
  setupStorage();
  incrementBootCounter();
  //
  
  #ifdef DEBUG
    Serial.println(F("modo debug"));
  #else
    Serial.println(F("modo produção"));
  #endif
  
  if(!LittleFS.begin()){
    #ifdef DEBUG
      Serial.println(LITTLEFS_ERROR);
    #endif      
  }

  // carrega sensores
  bool load = loadSensorList();
  if(!load) {
    #ifdef DEBUG
      Serial.println(F("Nao foi possivel carregar a lista de sensores!"));
    #endif
  }
  
  ssid = preferences.getString(PARAM_INPUT_1);
  pass = preferences.getString(PARAM_INPUT_2);
  ip = preferences.getString(PARAM_INPUT_3);
  gateway = preferences.getString(PARAM_INPUT_4);
  
  Serial.println(ssid);
  Serial.println(pass);
  Serial.println(ip);
  Serial.println(gateway);

  if(initWiFi()) {    
    #ifdef DEBUG
      Serial.println("\n\nNetwork Configuration:");
      Serial.println("----------------------");
      Serial.print("         SSID: "); Serial.println(WiFi.SSID());
      Serial.print("  Wifi Status: "); Serial.println(WiFi.status());
      Serial.print("Wifi Strength: "); Serial.print(WiFi.RSSI()); Serial.println(" dBm");
      Serial.print("          MAC: "); Serial.println(WiFi.macAddress());
      Serial.print("           IP: "); Serial.println(WiFi.localIP());
      Serial.print("       Subnet: "); Serial.println(WiFi.subnetMask());
      Serial.print("      Gateway: "); Serial.println(WiFi.gatewayIP());
      Serial.print("        DNS 1: "); Serial.println(WiFi.dnsIP(0));
      Serial.print("        DNS 2: "); Serial.println(WiFi.dnsIP(1));
      Serial.print("        DNS 3: "); Serial.println(WiFi.dnsIP(2));   
    #endif
    startWebServer();
    // exibindo rota /update para atualização de firmware e filesystem
    AsyncElegantOTA.begin(&server, USER_FIRMWARE, PASS_FIRMWARE);
    /* Usa MDNS para resolver o DNS */
    Serial.println("mDNS configurado e inicializado;");
    setClock();
    if (!MDNS.begin(HOST)) 
    { 
        //http://regador.local (linux) e http://regador (windows)
        #ifdef DEBUG
          Serial.println("Erro ao configurar mDNS. O ESP32 vai reiniciar em 1s...");
        #endif
        delay(1000);
        ESP.restart();        
    }
    // carrega dados
    loadAgendaList();

    //connecting to a mqtt broker
    mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
    mqttClient.setCallback(callback);
            
    // Initialize a NTPClient to get time
    timeClient.begin();
    // Set offset time in seconds to adjust for your timezone, for example:
    // GMT +1 = 3600
    // GMT +8 = 28800
    // GMT -1 = -3600
    // GMT 0 = 0
    timeClient.setTimeOffset(timeZone*(3600));
  
    //Espera pelo primeiro update online
    Serial.println("Esperando para sincronizar com o NTP");
    while(!timeClient.update())
    {
        Serial.print(".");
        timeClient.forceUpdate();
        delay(500);
    }
    Serial.println("Sincronizado com o servidor NTP");
     
    WIFI_CONFIG = true;
    Serial.println("Regador esta funcionando!");
  }
  else {
    // Conecta a rede Wi-Fi com SSID e senha
    Serial.println("Atribuindo Ponto de Acesso");
    // NULL sets an open Access Point
    WiFi.softAP("ESP-WIFI-MANAGER", NULL);
    IPAddress IP = WiFi.softAPIP();
    Serial.print("Endereço IP do ponto de acesso: ");
    Serial.println(IP);
    startWifiManagerServer();    
  }
}

void callback(char *topic, byte *payload, unsigned int length) {
  byte gpio;
  String message;
  
  #ifdef DEBUG
    Serial.print(F("Mensagem que chegou no tópico: "));
    Serial.println(topic);
    Serial.print(F("Mensagem:"));
  #endif
          
  for (int i = 0; i < length; i++) {
     message+=(char) payload[i];
  }
  #ifdef DEBUG
    Serial.println(message);
  #endif

  if(String(topic) == (String(MQTT_USERNAME)+String("/feeds/water")).c_str()) {
    digitalWrite(RelayWater, message=="ON"?1:0);
  }
  if(String(topic) == (String(MQTT_USERNAME)+String("/feeds/light")).c_str()) {
    digitalWrite(RelayLight, message=="ON"?1:0);
  }
  if(String(topic) == (String(MQTT_USERNAME)+String("/feeds/level")).c_str()) {
    digitalWrite(RelayLevel, message=="ON"?1:0);
  }
  if(String(topic) == (String(MQTT_USERNAME)+String("/feeds/list")).c_str()) {
    DynamicJsonDocument doc(MAX_STRING_LENGTH);
    // ler do feed list no adafruit
    if(message == "") {    
      #ifdef DEBUG
        Serial.println(F("Lista de agendamentos vazia"));
      #endif
      // carrega lista a partir do storage      
      if(loadAgendaList()>=0) saveAgendaList();
    } else {
      DeserializationError error = deserializeJson(doc, message);
      if (error) {
        #ifdef DEBUG
          Serial.println(F("Erro ao fazer o parser da lista vindo do Adafruit"));
        #endif
      }
      /*
      for(int i = 0; i < doc.size(); i++){
        addAgenda(doc[i]["dataAgenda"]);
      }
      */
    }
  }
  #ifdef DEBUG
    Serial.println(F("-----------------------"));
  #endif
}

void reconnect() {
  // Loop até que esteja reconectado
  while (!mqttClient.connected()) {
    Serial.println("Tentando conexão com o servidor MQTT...");
    String client_id = String(HOST)+".local-"+String(WiFi.macAddress());
    #ifdef DEBUG
      Serial.printf("O cliente %s conecta ao mqtt broker publico\n", client_id.c_str());
    #endif      
    if (mqttClient.connect(client_id.c_str(), MQTT_USERNAME, MQTT_PASSWORD)) {
      Serial.println(F("Adafruit mqtt broker conectado"));
      // Subscribe
      mqttClient.subscribe((String(MQTT_USERNAME)+String("/feeds/water")).c_str());
      mqttClient.subscribe((String(MQTT_USERNAME)+String("/feeds/light")).c_str());
      mqttClient.subscribe((String(MQTT_USERNAME)+String("/feeds/level")).c_str());
      mqttClient.subscribe((String(MQTT_USERNAME)+String("/feeds/list")).c_str());      //
    } else {
      #ifdef DEBUG
        Serial.printf("Falhou com o estado %d\nNao foi possivel conectar com o broker mqtt.\nPor favor, verifique as credenciais e instale uma nova versão de firmware.\nTentando novamente em 5 segundos.", mqttClient.state());
      #endif
      delay(5000);
    }
  }
}

void nivelBaixo() {
  // acendo a luz
  Serial.println("Acendo a luz");
  digitalWrite(RelayLight, LOW);
  mqttClient.publish((String(MQTT_USERNAME)+String("/feeds/light")).c_str(), "ON");
    
  // se a bomba estiver ligada
  if(digitalRead(RelayWater) == LOW){
    // desligo a bomba
    Serial.println("Desligo a bomba");
    digitalWrite(RelayWater, HIGH);
    mqttClient.publish((String(MQTT_USERNAME)+String("/feeds/water")).c_str(), "OFF");
  }
}

void nivelAlto() {
  time_t horaAtual = getHoraAgora();

  struct tm timeinfo;
  char buffer[80];
  gmtime_r(&horaAtual, &timeinfo);
  //exemplo: 14:12
  strftime (buffer,80,"%H:%M",&timeinfo);
  
  // ligo a bomba
  Serial.println("Ligo a bomba");
  digitalWrite(RelayWater, LOW);
  mqttClient.publish((String(MQTT_USERNAME)+String("/feeds/water")).c_str(), "ON");

  // removo da fila

  
  //apago a luz
  Serial.println("Apago a luz");
  digitalWrite(RelayLight, HIGH);
  mqttClient.publish((String(MQTT_USERNAME)+String("/feeds/light")).c_str(), "OFF");
}

void loop(void) {
  if (!mqttClient.connected()) {
    // tento conectar no MQTT somente se já tiver rede
    if(WIFI_CONFIG) reconnect();
  }
  mqttClient.loop();
  
  if(WIFI_CONFIG) {
    MDNS.update();

    time_t horaAtual = getHoraAgora();

    struct tm timeinfo;
    char horaTemp[80];
    gmtime_r(&horaAtual, &timeinfo);
    //exemplo: 14:12
    strftime (horaTemp,80,"%H:%M",&timeinfo);
    if(searchList(String(horaTemp)) > 0) {
      Serial.println("bateu com a hora do agendamento");
    }
/*
    // se nivel de agua baixou
    if(digitalRead(RelayLevel) == LOW) {      
      nivelBaixo();
    } else {      
      nivelAlto();
    }
*/    
  }
}
