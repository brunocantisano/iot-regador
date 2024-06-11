const char WRONG_AUTHORIZATION[] PROGMEM = "Authorization token errado";
const char WRONG_STATUS[] PROGMEM = "Erro ao atualizar o status";

const char EXISTING_ITEM[] PROGMEM = "Item já existente na lista";
const char REMOVED_ITEM[] PROGMEM = "Item removido da lista";
const char NOT_FOUND_ITEM[] PROGMEM = "Item não encontrado na lista";
const char NOT_FOUND_ROUTE[] PROGMEM = "Rota nao encontrada";
const char PARSER_ERROR[] PROGMEM = "{\"message\": \"Erro ao fazer parser do json\"}";
const char WEB_SERVER_CONFIG[] PROGMEM = "\nConfiguring Webserver ...";
const char WEB_SERVER_STARTED[] PROGMEM = "Webserver started";
const char HTML_MISSING_DATA_UPLOAD[] PROGMEM = "<!DOCTYPE html><html lang=\"en\"><head><title>Regador ESP8266</title>" 
                "<meta charset=\"utf-8\"><meta http-equiv=\"X-UA-Compatible\" content=\"IE=edge\">" 
                "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"></head>"
                "<body><center><img src=\"https://images.tcdn.com.br/img/img_prod/765836/regador_foz_2_litros_vasart_7409_2_b5cf997e67f703c9cd00a0c77dcce8f9.jpg\" width=\"128\"/> </center>"
                "<div class=\"container\">Lembre-se que para rodar a aplicação será necessário, previamente, instalar o plugin: "
                "<b><a src=\"https://randomnerdtutorials.com/install-esp8266-filesystem-uploader-arduino-ide/\">Install ESP8266 Filesystem Uploader in Arduino IDE\"</a></b>"
                " e utilizar o menu no Arduino IDE: <b>Ferramentas->ESP8266 Sketch Data Upload</b>"
                " para gravar o conteúdo do web server (pasta: <b>/data</b>) no <b>Storage</b>.</div></body></html>";

void startWebServer() {
  /* Webserver para se comunicar via browser com ESP32  */
  Serial.println(WEB_SERVER_CONFIG);
  /* 
   *  Rotas sem bloqueios de token na API
   *  Configura as páginas de login e upload 
   *  de firmware OTA 
   */
  // Rotas das imagens a serem usadas na página home e o Health (não estão com basic auth)
  handle_WaterLogo();
  handle_WaterList();
  handle_WaterIco();
  handle_Style();
  handle_RegadorRobo();
  handle_Health();
  handle_Metrics();  
  handle_Home();
  handle_Eventos();
  handle_Swagger();
  handle_SwaggerUI();
  // Rotas bloqueadas pelo token authorization
  handle_Ports();  
  handle_Sensors();
  handle_Lists();
  handle_UpdateSensors();
  handle_InsertItemList();
  handle_DeleteItemList();
  // ------------------------------------ //
  // se não se enquadrar em nenhuma das rotas
  handle_OnError();
 
  // tratando requisições como HTTPS
  //handle_SSL();

  // permitindo todas as origens. O ideal é trocar o '*' pela url do frontend poder utilizar a api com maior segurança
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Headers", "Access-Control-Allow-Headers, Origin, Accept, X-Requested-With, Content-Type, Access-Control-Request-Method, Access-Control-Request-Headers, Authorization");
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Credentials", "true");
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Methods", "GET,HEAD,OPTIONS,POST,PUT");
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");

  // startup web server
  server.begin();

  MDNS.addService("http", "tcp", 80);
  
  #ifdef DEBUG
    Serial.println(WEB_SERVER_STARTED);
  #endif
}

void startWifiManagerServer() {
  Serial.println("\nConfigurando o gerenciador de Wifi ...");
  
  handle_Style();
  handle_WifiManager();
  handle_WifiInfo();
  server.serveStatic("/", LittleFS, "/");
  
  server.begin();
}

void handle_OnError(){
  server.onNotFound([](AsyncWebServerRequest *request) {
    char filename[] = "/error.html";
    request->send(HTTP_NOT_FOUND, getContentType(filename), getContent(filename)); // otherwise, respond with a 404 (Not Found) error
  });
}

void handle_WaterLogo(){
  server.on("/water-logo", HTTP_GET, [](AsyncWebServerRequest *request) {
   request->send(LittleFS, "/water-logo.png", getContentType("/water-logo.png"));  
  });
}

void handle_WaterList(){
  server.on("/water-list", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(LittleFS, "/water-list.png", getContentType("/water-list.png"));
  });
}
 
void handle_WaterIco(){
  server.on("/water-ico", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(LittleFS, "/water-ico.ico", getContentType("/water-ico.ico"));   
  });
}

void handle_Style(){
  server.on("/style", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(LittleFS, "/style.css", getContentType("/style.css"));
  });
}


void handle_RegadorRobo(){
  server.on("/regador-robo", HTTP_GET, [](AsyncWebServerRequest *request) {
   request->send(LittleFS, "/regador-robo.png", getContentType("/regador-robo.png"));
  });
}

void handle_WifiManager(){
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(LittleFS, "/wifimanager.html", getContentType("/wifimanager.html"));
  });
}

void handle_WifiInfo(){
  server.on("/", HTTP_POST, [](AsyncWebServerRequest *request) {
    int params = request->params();
    for(int i=0;i<params;i++){
      AsyncWebParameter* p = request->getParam(i);
      if(p->isPost()){
        // HTTP POST ssid value
        if (p->name() == PARAM_INPUT_1) {
          ssid = p->value().c_str();
          Serial.print("SSID set to: ");
          Serial.println(ssid);
          preferences.putString(PARAM_INPUT_1, ssid.c_str());
        }
        // HTTP POST pass value
        if (p->name() == PARAM_INPUT_2) {
          pass = p->value().c_str();
          Serial.print("Password set to: ");
          Serial.println(pass);
          preferences.putString(PARAM_INPUT_2, pass.c_str());
        }
        // HTTP POST ip value
        if (p->name() == PARAM_INPUT_3) {
          ip = p->value().c_str();
          Serial.print("IP Address set to: ");
          Serial.println(ip);
          preferences.putString(PARAM_INPUT_3, ip.c_str());
        }
        // HTTP POST gateway value
        if (p->name() == PARAM_INPUT_4) {
          gateway = p->value().c_str();
          Serial.print("Gateway set to: ");
          Serial.println(gateway);
          preferences.putString(PARAM_INPUT_4, gateway.c_str());
        }
        //Serial.printf("POST[%s]: %s\n", p->name().c_str(), p->value().c_str());
      }
    }
    request->send(200, "text/plain", "Concluido. O ESP vai reiniciar, entao conecte-se em seu roteador e va para o endereco: http://" + String(HOST) + ".local");
    delay(3000);
    ESP.restart();
  });
}

void handle_Home(){
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    char filename[] = "/home.html";
    String html = getContent(filename);
    if(html.length() > 0) {
      // versao do firmware: https://semver.org/
      html.replace("0.0.0",String(version));
      html.replace("AIO_USERNAME",String(MQTT_USERNAME));
      html.replace("HOST_WATER",String(HOST));
    } else {
      html = HTML_MISSING_DATA_UPLOAD;  
    }
    request->send(HTTP_OK, getContentType(filename), html);
  });
}

void handle_Eventos(){
  server.on("/eventos", HTTP_GET, [](AsyncWebServerRequest *request) {
    char filename[] = "/eventos.html";    
    String html = getContent(filename);
    if(html.length() > 0) {
      html.replace("AIO_SERVER",String(MQTT_BROKER));
      html.replace("AIO_USERNAME",String(MQTT_USERNAME));
      html.replace("AIO_KEY",String(MQTT_PASSWORD));
    } else {
      html = HTML_MISSING_DATA_UPLOAD;  
    }
    request->send(HTTP_OK, getContentType(filename), html);
  });
}

void handle_Swagger(){
  server.on("/swagger.json", HTTP_GET, [](AsyncWebServerRequest *request) {
    char filename[] = "/swagger.json";
    String json = getContent(filename);
    if(json.length() > 0) {
      json.replace("0.0.0",version);
      json.replace("HOST_WATER",String(HOST)+".local");
    } else {
      json = HTML_MISSING_DATA_UPLOAD;  
    }
    request->send(HTTP_OK, getContentType(filename), json);
  });
}

void handle_SwaggerUI(){
  server.on("/swaggerUI", HTTP_GET, [](AsyncWebServerRequest *request) {
    char filename[] = "/swaggerUI.html";
    String html = getContent(filename);    
    if(html.length() > 0) {
      html.replace("HOST_WATER",String(HOST)+".local");
    } else {
      html = HTML_MISSING_DATA_UPLOAD;
    }
    request->send(HTTP_OK, getContentType(filename), html);
  });  
}

void handle_Health(){
  server.on("/health", HTTP_GET, [](AsyncWebServerRequest *request) {
    //String mqttConnected = mqttClient.connected()?"true":"false";
    //String JSONmessage = "{\"greeting\": \"Bem vindo ao Regador ESP8266 REST Web Server\",\"date\": \""+getDataHora()+"\",\"url\": \"/health\",\"mqtt\": \""+mqttConnected+"\",\"version\": \""+version+"\",\"ip\": \""+String(IpAddress2String(WiFi.localIP()))+"\"}";
    String JSONmessage = "{\"greeting\": \"Bem vindo ao Regador ESP8266 REST Web Server\"}";
    request->send(HTTP_OK, getContentType(".json"), JSONmessage);
  });
}

void handle_Metrics(){
  server.on("/metrics", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(HTTP_OK, getContentType(".txt"), getMetrics());
  });
}

void handle_Ports(){
  server.on("/ports", HTTP_GET, [](AsyncWebServerRequest *request) {
    if(check_authorization_header(request)) {
      String JSONmessage;
      ArduinoSensorPort *arduinoSensorPort;    
      for(int i = 0; i < sensorListaEncadeada.size(); i++){
        // Obtem a aplicação da lista
        arduinoSensorPort = sensorListaEncadeada.get(i);
        arduinoSensorPort->status = digitalRead(arduinoSensorPort->gpio);
        JSONmessage += "{\"id\": \""+String(arduinoSensorPort->id)+"\",\"gpio\": \""+String(arduinoSensorPort->gpio)+"\",\"status\": \""+String(arduinoSensorPort->status)+"\",\"name\": \""+String(arduinoSensorPort->name)+"\"},";
      }
      request->send(HTTP_OK, getContentType(".json"), '['+JSONmessage.substring(0, JSONmessage.length()-1)+']');
    } else {
      request->send(HTTP_UNAUTHORIZED, getContentType(".txt"), WRONG_AUTHORIZATION);
    }
  });  
}

void handle_Sensors() {
  server.on("/sensors", HTTP_GET, [](AsyncWebServerRequest *request) {
    //"/sensors?type=water"
    //"/sensors?type=light"
    //"/sensors?type=level"
    if(check_authorization_header(request)) {
      int relayPin = RelayWater;
      int paramsNr = request->params();
      for(int i=0;i<paramsNr;i++){
        AsyncWebParameter* p = request->getParam(i);
        if (strcmp("light", p->value().c_str())==0){
          relayPin = RelayLight;
        } else if(strcmp("level", p->value().c_str())==0){
          relayPin = RelayLevel;
        }
      }
      request->send(HTTP_OK, getContentType(".json"), readSensor(relayPin));
    } else {
      request->send(HTTP_UNAUTHORIZED, getContentType(".txt"), WRONG_AUTHORIZATION);
    }
  });
}

void handle_Lists(){
  server.on("/lists", HTTP_GET, [](AsyncWebServerRequest *request) {
    if(check_authorization_header(request)) {
      String JSONmessage;
      Agenda *agd;
      for(int i = 0; i < agendaListaEncadeada.size(); i++){
        // Obtem a aplicação da lista
        agd = agendaListaEncadeada.get(i);
        JSONmessage += "{\"id\": "+String(i+1)+",\"dataAgenda\": \""+String(agd->dataAgenda)+"\"}"+",";
      }
      request->send(HTTP_OK, getContentType(".json"), "["+JSONmessage.substring(0, JSONmessage.length()-1)+"]");
    } else {
      request->send(HTTP_UNAUTHORIZED, getContentType(".txt"), WRONG_AUTHORIZATION);
    }
  });
}

void handle_UpdateSensors(){
  //"/sensor?type=water"
  server.on("/sensor", HTTP_PUT, [](AsyncWebServerRequest * request){}, NULL,
    [](AsyncWebServerRequest * request, uint8_t *data, size_t len, size_t index, size_t total) {
    if(check_authorization_header(request)) {
      int sensor = RelayWater;
      String feedName = "water";
      int paramsNr = request->params();
      /*
      for(int i=0;i<paramsNr;i++){
        AsyncWebParameter* p = request->getParam(i);
        feedName = p->value();
        if(strcmp("light", p->value().c_str())==0){
          sensor = RelayLight;
        }
      }
      */
      DynamicJsonDocument doc(MAX_STRING_LENGTH);
      String JSONmessageBody = getData(data, len);
      DeserializationError error = deserializeJson(doc, JSONmessageBody);
      if(error) {
        request->send(HTTP_BAD_REQUEST, getContentType(".json"), PARSER_ERROR);
      } else {
        String JSONmessage;
        if(readBodySensorData(doc["status"], sensor)) {
          ArduinoSensorPort *arduinoSensorPort = searchListSensor(sensor);
          if(arduinoSensorPort != NULL) {
            JSONmessage="{\"id\":\""+String(arduinoSensorPort->id)+"\",\"name\":\""+String(arduinoSensorPort->name)+"\",\"gpio\":\""+String(arduinoSensorPort->gpio)+"\",\"status\":\""+String(arduinoSensorPort->status)+"\"}";
          }
          digitalWrite(arduinoSensorPort->gpio, arduinoSensorPort->status);
          // publish
          mqttClient.publish((String(MQTT_USERNAME)+String("/feeds/")+feedName).c_str(), arduinoSensorPort->status==0?"OFF":"ON");
          doc.clear();
          request->send(HTTP_OK, getContentType(".json"), JSONmessage);
        } else {
          request->send(HTTP_BAD_REQUEST, getContentType(".txt"), WRONG_STATUS);
        }
      }
    } else {
      request->send(HTTP_UNAUTHORIZED, getContentType(".txt"), WRONG_AUTHORIZATION);
    }
  });
}

// handles uploads to storage
void handleUploadStorage(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
  String logmessage = "Cliente:" + request->client()->remoteIP().toString() + "-" + request->url() + "-" + filename;
  #ifdef DEBUG
    Serial.println(logmessage);
  #endif
  if (!index) {
    logmessage = "Upload Iniciado: " + String(filename);
    // open the file on first call and store the file handle in the request object
    request->_tempFile = LittleFS.open("/" + filename, "w");
    #ifdef DEBUG
      Serial.println(logmessage);
    #endif
  }

  if (len) {
    // stream the incoming chunk to the opened file
    request->_tempFile.write(data, len);
    logmessage = "Escrevendo arquivo: " + String(filename) + " index=" + String(index) + " len=" + String(len);
    #ifdef DEBUG
      Serial.println(logmessage);
    #endif
  }

  if (final) {
    logmessage = "Upload Completo: " + String(filename) + ",size: " + String(index + len);
    // close the file handle as the upload is now done
    request->_tempFile.close();
    #ifdef DEBUG
      Serial.println(logmessage);
    #endif
    request->redirect("/");
  }
}

void handle_InsertItemList(){
  server.on("/list", HTTP_POST, [](AsyncWebServerRequest * request){}, NULL,
    [](AsyncWebServerRequest * request, uint8_t *data, size_t len, size_t index, size_t total) {
    if(check_authorization_header(request)) {
      DynamicJsonDocument doc(MAX_STRING_LENGTH);
      String JSONmessageBody = getData(data, len);
           
      DeserializationError error = deserializeJson(doc, JSONmessageBody);
      if(error) {
        request->send(HTTP_BAD_REQUEST, getContentType(".json"), PARSER_ERROR);
      } else {
        String hora = doc["dataAgenda"];
        if (validaHora(hora)){
          //busco para checar se aplicacao já existe
          int index = searchList(hora);
          if(index == -1) {
            String JSONmessage;
            // não existe, então posso inserir
            // adiciona item na lista de aplicações jenkins 
            addAgenda(hora);
  
            // Grava no Storage
            saveAgendaList();
          
            Agenda *agd;
            for(int i = 0; i < agendaListaEncadeada.size(); i++){
              // Obtem a aplicação da lista
              agd = agendaListaEncadeada.get(i);
              JSONmessage="{\"dataAgenda\": \""+String(agd->dataAgenda)+"\"}";
              if((i < agendaListaEncadeada.size()) && (i < agendaListaEncadeada.size()-1)) {
                JSONmessage+=",";
              }
            }
            JSONmessage="[" + JSONmessage +"]";
            #ifdef DEBUG
              Serial.println("handle_InsertItemList:"+JSONmessage);
            #endif
            // Grava no adafruit
            // publish
            mqttClient.publish((String(MQTT_USERNAME)+String("/feeds/list")).c_str(), JSONmessage.c_str());
            doc.clear();
            request->send(HTTP_OK, getContentType(".json"), JSONmessage);
          } else {
            request->send(HTTP_CONFLICT, getContentType(".txt"), EXISTING_ITEM);
          }
        }
        else {
          request->send(HTTP_CONFLICT, getContentType(".txt"), PARSER_ERROR);
        }
      }
   } else {
    request->send(HTTP_UNAUTHORIZED, getContentType(".txt"), WRONG_AUTHORIZATION);
   }
  });
}

void handle_DeleteItemList(){
  server.on("/list/del", HTTP_DELETE, [](AsyncWebServerRequest * request){}, NULL,
    [](AsyncWebServerRequest * request, uint8_t *data, size_t len, size_t index, size_t total) {
    if(check_authorization_header(request)) {
      DynamicJsonDocument doc(MAX_STRING_LENGTH);
      String JSONmessageBody = getData(data, len);
      DeserializationError error = deserializeJson(doc, JSONmessageBody);
      if(error) {
        request->send(HTTP_BAD_REQUEST, getContentType(".json"), PARSER_ERROR);
      } else {
        String hora = doc["dataAgenda"];
        if (validaHora(hora)){
        //busco pela aplicacao a ser removida
        int index = searchList(hora);
        if(index != -1) {
          String JSONmessage;
          //removo
          agendaListaEncadeada.remove(index);
          
          // Grava no Storage
          saveAgendaList();
          
          Agenda *agd;
          for(int i = 0; i < agendaListaEncadeada.size(); i++){
            // Obtem a aplicação da lista
            agd = agendaListaEncadeada.get(i);
            JSONmessage="{\"dataAgenda\": \""+String(agd->dataAgenda)+"\"}";
            if((i < agendaListaEncadeada.size()) && (i < agendaListaEncadeada.size()-1)) {
              JSONmessage+=",";
            }
          }
          JSONmessage="[" + JSONmessage +"]";
          #ifdef DEBUG
            Serial.println("handle_DeleteItemList:"+JSONmessage);
          #endif
          // Grava no adafruit
          // publish
          mqttClient.publish((String(MQTT_USERNAME)+String("/feeds/list")).c_str(), JSONmessage.c_str());
          doc.clear();
          request->send(HTTP_OK, getContentType(".txt"), REMOVED_ITEM);
        } else {
          request->send(HTTP_NOT_FOUND, getContentType(".txt"), NOT_FOUND_ITEM);
        }
      } else {
        request->send(HTTP_CONFLICT, getContentType(".txt"), PARSER_ERROR);
      }
    }
   } else {
    request->send(HTTP_UNAUTHORIZED, getContentType(".txt"), WRONG_AUTHORIZATION);
   }
  });
}

bool check_authorization_header(AsyncWebServerRequest * request){
  int headers = request->headers();
  int i;
  for(i=0;i<headers;i++){
    AsyncWebHeader* h = request->getHeader(i);
    //Serial.printf("_HEADER[%s]: %s\n", h->name().c_str(), h->value().c_str());
    if(h->name()=="Authorization" && h->value()=="Basic "+String(API_WATER_TOKEN)){
      return true;
    }
  }
  return false;
}
