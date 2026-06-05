#include <Arduino.h>
#include "WebServerHandler.h"
#include <ArduinoUtilsCds.h>
#include <ElegantOTA.h>
#include <ESP8266mDNS.h>
//---------------------------------//

// ====== Objetos do seu projeto ======
ArduinoUtilsCds * utilscds = nullptr;
WiFiClient wifiClient;
AsyncWebServer server(HTTP_REST_PORT);
WebServerHandler * websrvhdl = nullptr;
Credentials creds;
String decrypted_userFirmware;
String decrypted_passFirmware;
String decrypted_userMqtt;
String decrypted_passMqtt;
String decrypted_apiToken;
bool isWiFiConnected = false;

//---------------------------------//
//  SETUP
void setup() {
Serial.begin(SERIAL_BAUD);
  delay(100);

  // Força inicialização completa do subsistema Wi-Fi
  WiFi.mode(WIFI_STA);
  WiFi.begin("init", "init");  // credenciais inválidas, só para inicializar
  delay(500);
  WiFi.disconnect(false);  // false = não apaga credenciais do NVS
  WiFi.mode(WIFI_OFF);
  delay(300);
  Serial.println("WiFi subsystem ok");
  Serial.flush();

  utilscds = new ArduinoUtilsCds();
  Serial.println("\nBoot...");

  // === 1. Leitura de TODOS os campos brutos primeiro ===
  const String raw_userFirmware       = utilscds->getCampoCredencial(USER, "USER_FIRMWARE");
  const int    len_userFirmware       = utilscds->getCampoCredencial(USER, "USER_FIRMWARE_LENGTH").toInt();

  const String raw_passFirmware       = utilscds->getCampoCredencial(USER, "PASS_FIRMWARE");
  const int    len_passFirmware       = utilscds->getCampoCredencial(USER, "PASS_FIRMWARE_LENGTH").toInt();

  const String hostName               = utilscds->getCampoCredencial(USER, "HOST");
  const String apiVersion             = utilscds->getCampoCredencial(USER, "API_VERSION");
  const String callerOrigin           = utilscds->getCampoCredencial(USER, "CALLER_ORIGIN");
  const String raw_apiToken           = utilscds->getCampoCredencial(USER, "API_TOKEN");
  const int    len_apiToken           = utilscds->getCampoCredencial(USER, "API_TOKEN_LENGTH").toInt();


  const String mqttBroker             = utilscds->getCampoCredencial(USER, "MQTT_BROKER");  
  const String raw_mqttUser           = utilscds->getCampoCredencial(USER, "MQTT_USERNAME");
  const int    len_mqttUser           = utilscds->getCampoCredencial(USER, "MQTT_USERNAME_LENGTH").toInt();
 const String raw_mqttPass            = utilscds->getCampoCredencial(USER, "MQTT_PASSWORD");
  const int    len_mqttPass           = utilscds->getCampoCredencial(USER, "MQTT_PASSWORD_LENGTH").toInt();
  const int    mqttPort               = utilscds->getCampoCredencial(USER, "MQTT_PORT").toInt();

  // === 2. Descriptografia depois, com todos os dados já em memória ===
  const String decrypted_userFirmware    = utilscds->decrypta(raw_userFirmware, len_userFirmware);
  const String decrypted_passFirmware    = utilscds->decrypta(raw_passFirmware, len_passFirmware);
  const String decrypted_apiToken        = utilscds->decrypta(raw_apiToken, len_apiToken);
  const String decrypted_mqttUser        = utilscds->decrypta(raw_mqttUser, len_mqttUser);
  const String decrypted_mqttPass        = utilscds->decrypta(raw_mqttPass, len_mqttPass);
  #ifdef DEBUG
    Serial.println("userFirmware: "    + decrypted_userFirmware);
    Serial.println("passFirmware: "    + decrypted_passFirmware);
    Serial.println("apiToken: "        + decrypted_apiToken);
    Serial.println("mqttUser: "        + decrypted_mqttUser);
    Serial.println("mqttPass: "        + decrypted_mqttPass);
  #endif
  
  // === Servidor principal e OTA (só quando conectado) ===
  websrvhdl = new WebServerHandler(
    decrypted_apiToken.c_str(),
    apiVersion,
    hostName,
    "Cisterna 1",
    utilscds
  );

  isWiFiConnected = websrvhdl->connectSTA(hostName);
  if (!isWiFiConnected) {
    String apName = hostName.isEmpty() ? String("device-setup") : (hostName + "-setup");
    #ifdef DEBUG
      Serial.printf("Heap livre antes do AP: %d bytes\n", ESP.getFreeHeap());
    #endif
    websrvhdl->startWebServerWifiManager(apName);
    #ifdef DEBUG
      Serial.println("WiFi não configurado!");
       IPAddress apIP = WiFi.softAPIP();
      Serial.println("Por favor, conecte-se em: " + apName + " e entre em: http://" + apIP.toString().c_str() + " para configuração do WiFi.");
    #endif
  } else {   
    const char * hostname = hostName.c_str();
    MDNS.end();

    // Atribuindo clock para conseguir usar datetime nos arquivos de log
    utilscds->atribuiRelogio();

    utilscds->iniciaStorage();
    utilscds->exibeMensagem("Inicializando o storage");

    websrvhdl->startWebServer();   // registra rotas no 'server' e chama server->begin() lá dentro

    #ifdef DEBUG
      Serial.println("Web Server inicializado");
      Serial.printf("Heap após startWebServer: %d\n", ESP.getFreeHeap());
      Serial.printf("Heap maior bloco: %d\n", ESP.getMaxFreeBlockSize());
    #endif
    
    ElegantOTA.begin(websrvhdl->getWebServer(), decrypted_userFirmware.c_str(), decrypted_passFirmware.c_str());
    #ifdef DEBUG
      Serial.println("OTA inicializado");
      Serial.printf("Heap após OTA: %d\n", ESP.getFreeHeap());
      Serial.printf("Heap maior bloco: %d\n", ESP.getMaxFreeBlockSize());
      Serial.println("host: http://"+hostName+".local");
    #endif

    if(!MDNS.begin(hostname)){
      #ifdef DEBUG
        Serial.println("mDNS falhou");
      #endif
      delay(1000);
      delete websrvhdl;
      ESP.restart();
    }
    MDNS.addService("http", "tcp", 80);
    #ifdef DEBUG
      Serial.print(F("mDNS ok: http://"));
      Serial.print(hostname);     // hostname = const char* ou String
      Serial.println(F(".local"));
    #endif
  }
}

//  LOOP
void loop() {
  if (isWiFiConnected) {
    MDNS.update();
    ElegantOTA.loop();
    websrvhdl->loop();
    #ifdef USE_MQTT
      utilscds->atualizaMqtt();
    #endif
    /*
    // Report every 1 minuto.
    if(timeSinceLastRead > 1000) {
      time_t horaAtual = getHoraAgora();  
      struct tm timeinfo;
      char horaTemp[80];
      gmtime_r(&horaAtual, &timeinfo);
      //exemplo: 14:12
      strftime (horaTemp,80,"%H:%M",&timeinfo);
      if(searchList(String(horaTemp)) >= 0) {
        Serial.println("bateu com a hora do agendamento");
          // se nivel de agua baixou
          if(digitalRead(RelayLevel) == HIGH) {
            nivelBaixo();
          } else {
            nivelAlto();
            // removo da fila
            if(removeItemLista(horaTemp)) {
              Serial.println("Removido agendamento apos regar as plantas");
            }
          }
      }
      timeSinceLastRead = 0;
    }
    timeSinceLastRead += 100;
    */
  }
}
