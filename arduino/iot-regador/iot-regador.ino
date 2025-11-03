
// ====== SEUS HEADERS ======
#include "Config.h"
#include "WebServerHandler.h"
const char LITTLEFS_ERROR[] PROGMEM = "Erro ocorreu ao tentar montar LittleFS";

//---------------------------------//

// ====== Objetos do seu projeto ======
ArduinoUtilsCds utilscds;
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
  //Serial.begin(SERIAL_PORT);
  Serial.begin(115200);
  delay(5000);
  Serial.println("Boot...");
  delay(5000);

  // === Carrega credenciais de firmware/host etc. (credentials.enc) === 
  static const char* required[] = {
    "MQTT_BROKER", "MQTT_USERNAME", "MQTT_USERNAME_LENGTH", "MQTT_PASSWORD", "MQTT_PASSWORD_LENGTH", "MQTT_PORT",
    "USER_FIRMWARE", "USER_FIRMWARE_LENGTH", "PASS_FIRMWARE", "PASS_FIRMWARE_LENGTH",
    "HOST", "API_TOKEN", "API_TOKEN_LENGTH", "API_VERSION", "CALLER_ORIGIN"
  };

  const size_t requiredSize = sizeof(required) / sizeof(required[0]);
  String payload;
  if (!utilscds.iniciaStorage()) {
    Serial.println("ERRO: Falha ao inicializar o sistema de arquivos!");
    return;
  }
  // Verifica se o arquivo existe
  String credentialsPath = "/credentials.enc";
  
  if (utilscds.verificaArquivoExiste(credentialsPath)) {
      Serial.println("=== Arquivo encontrado! ===\n");
      
      // Lê e imprime o conteúdo do arquivo
      Serial.println("=== Conteúdo do arquivo credentials ===");
      Serial.println("-------------------------------------------");
      
      String content=utilscds.lerArquivo(credentialsPath);
      creds = utilscds.quebraValidaCredenciais(content, required, requiredSize, true, false);
      Serial.println("-------------------------------------------\n");

      if (creds.valid) {
        decrypted_userMqtt = utilscds.decrypta(creds.mqttUsername, creds.mqttUsernameLength);
        decrypted_passMqtt = utilscds.decrypta(creds.mqttPassword, creds.mqttPasswordLength);
        decrypted_userFirmware = utilscds.decrypta(creds.userFirmware, creds.userFirmwareLength);
        decrypted_passFirmware = utilscds.decrypta(creds.passFirmware, creds.passFirmwareLength);
        decrypted_apiToken     = utilscds.decrypta(creds.apiToken, creds.apiTokenLength);

        //Serial.println("decrypted_userMqtt: "+decrypted_userMqtt);
        //Serial.println("decrypted_passMqtt: "+decrypted_passMqtt);
        //Serial.println("decrypted_userFirmware: "+decrypted_userFirmware);
        //Serial.println("decrypted_passFirmware: "+decrypted_passFirmware);
        //Serial.println("decrypted_apiToken: "+decrypted_apiToken);
        const String hostName = creds.host.isEmpty() ? String("device") : creds.host;

        // === Servidor principal e OTA (só quando conectado) ===
        websrvhdl = new WebServerHandler(
          decrypted_apiToken.c_str(), 
          creds.apiVersion, 
          hostName, 
          creds.callerOrigin,
          &utilscds
        );
        // === Wi-Fi: tenta STA; se falhar, abre portal ===
        isWiFiConnected = websrvhdl->connectSTA(hostName);
        if (!isWiFiConnected) {
          String apName = hostName.isEmpty() ? String("device-setup") : (hostName + "-setup");
          websrvhdl->startWebServerWifiManager(apName);
          Serial.println("WiFi não configurado!");
          Serial.println("Por favor, conecte-se em: " + apName + " e entre em: http://" + hostName + " para configuração do WiFi.");
        } else {
#ifdef USE_MQTT
          // === Servidor principal e OTA (só quando conectado) ===
          //Serial.println("mqttBroker: "+creds.mqttBroker);
          //Serial.println("decrypted_userMqtt: "+decrypted_userMqtt);
          //Serial.println("decrypted_passMqtt: "+decrypted_passMqtt);
          // inicio o mqtt
          if(!utilscds.iniciaMqtt(creds.mqttBroker, decrypted_userMqtt, decrypted_passMqtt, &wifiClient)){
            Serial.println("Não conseguiu se conectar no MQTT");
          } 
#endif
          char usuario[64];
          char senha[64];
          String ssid = WiFi.SSID();
          String pass = WiFi.psk();
          strncpy(usuario, ssid.c_str(), sizeof(usuario));
          strncpy(senha, pass.c_str(), sizeof(senha));
          utilscds.salvaCredenciaisWiFi(usuario, senha);          

          utilscds.iniciaOta(&server, decrypted_userFirmware, decrypted_passFirmware);
          Serial.println("OTA inicializado");
          
          websrvhdl->startWebServer();   // registra rotas no 'server' e chama server->begin() lá dentro
          Serial.println("Web Server inicializado");

          const char * hostname = hostName.c_str();
          MDNS.end();
          // Atribuindo clock para conseguir usar datetime nos arquivos de log
          utilscds.atribuiRelogio();

          if(!MDNS.begin(hostname)){
            Serial.println("mDNS falhou");
            delay(1000);
            delete websrvhdl;
            ESP.restart();
          }
          MDNS.addService("http", "tcp", 80);
          Serial.print(F("mDNS ok: http://"));
          Serial.println(hostname);
        }
      } else {
        Serial.println("Credenciais inválidas");
      }
  } else {
    Serial.println("Arquivo de credenciais não existe!");
  }
}

//  LOOP
void loop() {
  if (isWiFiConnected) {
    MDNS.update();
    utilscds.loopOta();
    
    #ifdef USE_MQTT
      utilscds.atualizaMqtt();
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
