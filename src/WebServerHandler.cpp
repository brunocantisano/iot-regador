// WebServerHandler.cpp
#include "WebServerHandler.h"

static const char* MSG_ARQUIVO_NAO_ENCONTRADO = "Provavelmente voce nao carregou os arquivos da pasta \"data\" (LittleFS) para o servidor!";
// Ajuste aqui conforme sua fiação:
// true  -> boia fecha em GND (INPUT_PULLUP; ativo=LOW)
// false -> boia vai a VCC (ativo=HIGH)
static constexpr bool ACTIVE_LOW = true;

// Lê várias vezes e decide por maioria: robusto contra ruído/boot
static bool readLevelStable(int pin) {  
  return digitalRead(pin);
}

WebServerHandler::WebServerHandler(
    const String& token, 
    const String& version, 
    const String& hostServer, 
    const String& caller,
    ArduinoUtilsCds * cds): 
                      apiToken(token),
                      apiVersion(version),
                      host(hostServer),
                      callerOrigin(caller),
                      utilscds(cds)
{
  server = new AsyncWebServer(HTTP_REST_PORT);
  ws = new AsyncWebSocket("/ws");
  utilshdl = utilscds->obtemUtilitarios();
  strhdl   = utilscds->obtemStorage();
  prefshdl = utilscds->obtemPreferences();
  utilscds->criaNovoArquivoLog();
}

WebServerHandler::~WebServerHandler() {
    delete server;  // Free the allocated memory
    delete ws;
    delete utilscds;
}

String WebServerHandler::obtemMetricas() {
  String p = "";
  int sketch_size = ESP.getSketchSize();
  int flash_size =  ESP.getFreeSketchSpace();
  int available_size = flash_size - sketch_size;
  int heap_size = ESP.getFreeContStack();
  int free_heap = ESP.getFreeHeap();
  uint32_t heap_max_bloco  = ESP.getMaxFreeBlockSize();
  uint8_t  heap_frag       = ESP.getHeapFragmentation();
  const String boardName = "esp8266";
   
  atribuiMetrica(&p, boardName+"_uptime", String(millis()));
  atribuiMetrica(&p, boardName+"_wifi_rssi", String(WiFi.RSSI()));
  atribuiMetrica(&p, boardName+"_sketch_size", String(sketch_size));
  atribuiMetrica(&p, boardName+"_flash_size", String(flash_size));
  atribuiMetrica(&p, boardName+"_available_size", String(available_size));
  atribuiMetrica(&p, boardName+"_heap_size", String(heap_size));
  atribuiMetrica(&p, boardName+"_free_heap", String(free_heap));
  atribuiMetrica(&p, boardName+"_heap_max_bloco", String(heap_max_bloco));
  atribuiMetrica(&p, boardName+"_heap_frag", String(heap_frag)); 
  atribuiMetrica(&p, boardName+"_boot_counter", String(obtemContagemBoots()));

  atribuiMetrica(&p, boardName+"_water", obtemEstadoSensor(RelayWater));
  atribuiMetrica(&p, boardName+"_level", obtemEstadoSensor(RelayLevel));

  return p;
}

String WebServerHandler::obtemEstadoSensor(int sensor) {
  return readLevelStable(sensor) ? "1" : "0";
}

/**
   Layout

   # heltec_lora32_uptime
   # TYPE heltec_lora32_uptime gauge
   heltec_lora32_uptime 23899

*/
void WebServerHandler::atribuiMetrica(String *p, String metric, String value) {
  *p += "# " + metric + "\n";
  *p += "# TYPE " + metric + " gauge\n";
  *p += "" + metric + " ";
  *p += value;
  *p += "\n";
}

int WebServerHandler::obtemContagemBoots() {
  String boot = prefshdl->loadDataPreferentials("storage", "boot", "0");
  return boot.toInt();
}

void WebServerHandler::incrementaContagemBoots() {
  int boot = obtemContagemBoots()+1;
  char buffer[10];
  sprintf(buffer, "%d", boot);
  const char* texto = buffer;
  prefshdl->saveDataPreferentials("storage", "boot", texto);
}

bool WebServerHandler::check_authorization_header(AsyncWebServerRequest * request){
  int headers = request->headers();
  for(int i=0;i<headers;i++){
    const AsyncWebHeader* h = request->getHeader(i);
    if(h->name().equalsIgnoreCase("Authorization") && h->value()=="Basic "+String(apiToken)){
      return true;
    }
  }
  return false;
}

void WebServerHandler::handleFileServing(void){
  server->on("/get-file", HTTP_GET, [this](AsyncWebServerRequest *request) {
    // "/get-file?name=delete.png"
    char filename[MAX_PATH];
    memset(filename, 0x00, MAX_PATH);

    if (request->hasParam("name")) {
      String file = request->getParam("name")->value();
      String safeFile = "/" + utilshdl->sanitizeFilename(file);
      strlcpy(filename, safeFile.c_str(), MAX_PATH);
      request->send(LittleFS, filename, utilshdl->getMimeType(filename));
    } else {
      request->send(HTTP_BAD_REQUEST, utilshdl->getMimeType(".txt"), "Parametro 'name' ausente");
    }
  });
}

void WebServerHandler::handleHome(){
  server->on("/", HTTP_GET, [this](AsyncWebServerRequest *request) {    
    String html = "";
    if(!strhdl->readFile("/home.html", html)) {
      html=String(MSG_ARQUIVO_NAO_ENCONTRADO);
    } else {
      // Dados fixos ou variáveis de cada caixa
      const char* img       = "get-file?name=fortlev.png";
      String nomes[1]       = {"Cisterna"};
      const char* ids[1]    = {"cx"};
      
      ArduinoSensorPort * s25 = searchListSensor(PIN_LV25);
      ArduinoSensorPort * s50 = searchListSensor(PIN_LV50);
      ArduinoSensorPort * s75 = searchListSensor(PIN_LV75);
      ArduinoSensorPort * s100= searchListSensor(PIN_LV100);

      // lê do hardware (evita usar cache antigo)
      bool estadosCx[4] = {
        readLevelStable(PIN_LV25),
        readLevelStable(PIN_LV50),
        readLevelStable(PIN_LV75),
        readLevelStable(PIN_LV100)
      };
      bool* estados[]   = { estadosCx };
      const int total   = 1;

      String json = "[";
      for (int i = 0; i < total; i++) {
        json += "{";
        json += "\"id\":\""   + String(ids[i])   + "\",";
        json += "\"name\":\"" + String(nomes[i]) + "\",";
        json += "\"img\":\""  + String(img)      + "\",";
        json += "\"state\":{";
        json += "\"s25\":"  + String(estados[i][0] ? "true" : "false") + ",";
        json += "\"s50\":"  + String(estados[i][1] ? "true" : "false") + ",";
        json += "\"s75\":"  + String(estados[i][2] ? "true" : "false") + ",";
        json += "\"s100\":" + String(estados[i][3] ? "true" : "false");
        json += "}";
        json += "}";
        if (i < total - 1) json += ",";
      }
      json += "]";

      html.replace("ARRAY_WATER_LEVEL", json);
      html.replace("0.0.0", apiVersion);
      html.replace("HOST_WATER_LEVEL", host + ".local");
    }
    request->send(HTTP_OK, utilshdl->getMimeType(".html"), html);
  });
}

void WebServerHandler::handleSwagger(){
  server->on("/swagger.json", HTTP_GET, [this](AsyncWebServerRequest *request) {
    String html = "";
    if(!strhdl->readFile("/swagger.json", html)) {  
      html=String(MSG_ARQUIVO_NAO_ENCONTRADO);  
    } else {
      html.replace("0.0.0",apiVersion);
      html.replace("HOST_WATER_LEVEL",host+".local");         
    }
    request->send(HTTP_OK, utilshdl->getMimeType(".json"), html);
  });
}

void WebServerHandler::handleSwaggerUI(){
  server->on("/swaggerUI", HTTP_GET, [this](AsyncWebServerRequest *request) {
    String html = "";
    if(!strhdl->readFile("/swaggerUI.html", html)) {
      html=String(MSG_ARQUIVO_NAO_ENCONTRADO);
    } else {
      html.replace("HOST_WATER_LEVEL",host+".local");  
    }
    request->send(HTTP_OK, utilshdl->getMimeType(".html"), html);
  });  
}

void WebServerHandler::handleHealth(){
  server->on("/health", HTTP_GET, [this](AsyncWebServerRequest *request) {
    String JSONmessage = "{\"greeting\": \"Bem vindo ao Nível de Caixa d'água ESP8266 REST Web Server\",\"date\": \""+utilshdl->getDataHora()+"\",\"url\": \"/health\",\"version\": \""+apiVersion+"\",\"ip\": \""+utilshdl->IpAddress2String(WiFi.localIP())+"\"}";
    request->send(HTTP_OK, utilshdl->getMimeType(".json"), JSONmessage);
  });
}

void WebServerHandler::handleMetrics(){
  server->on("/metrics", HTTP_GET, [this](AsyncWebServerRequest *request) {
    request->send(HTTP_OK, utilshdl->getMimeType(".txt"), obtemMetricas());
  });
}

void WebServerHandler::handlePorts(){
  server->on("/ports", HTTP_GET, [this](AsyncWebServerRequest *request) {
    if(check_authorization_header(request)) {
      String JSONmessage;
      ArduinoSensorPort *arduinoSensorPort;    
      for(int i = 0; i < sensorListaEncadeada.size(); i++){
        // Obtem a aplicação da lista
        arduinoSensorPort = sensorListaEncadeada.get(i);
        JSONmessage += "{\"id\": \""+String(arduinoSensorPort->id)+"\",\"gpio\": \""+String(arduinoSensorPort->gpio)+"\",\"name\": \""+String(arduinoSensorPort->name)+"\"},";
      }
      request->send(HTTP_OK, utilshdl->getMimeType(".json"), '['+JSONmessage.substring(0, JSONmessage.length()-1)+']');
    } else {
      request->send(HTTP_UNAUTHORIZED, utilshdl->getMimeType(".txt"), WRONG_AUTHORIZATION);
    }
  });  
}

void WebServerHandler::handleSensors() {
  server->on("/sensors", HTTP_GET, [this](AsyncWebServerRequest *request) {
    if (!check_authorization_header(request)) {
      request->send(HTTP_UNAUTHORIZED, utilshdl->getMimeType(".txt"), WRONG_AUTHORIZATION);
      return;
    }

    const AsyncWebParameter* pLevel = request->getParam("level");
    if (!pLevel) { request->send(HTTP_BAD_REQUEST, utilshdl->getMimeType(".txt"), "missing level"); return; }

    int level = pLevel->value().toInt();  // 1..4 (25/50/75/100)
    int pin = (level==1)?PIN_LV25 : (level==2)?PIN_LV50 : (level==3)?PIN_LV75 : (level==4)?PIN_LV100 : -1;
    bool on = readLevelStable(pin);
    if (auto s = searchListSensor(pin)) s->status = on;
    String resp = on ? "ativado" : "desativado";
    request->send(HTTP_OK, utilshdl->getMimeType(".txt"), resp);
  });  
}

void WebServerHandler::handleUpdateSensors() {
  server->on("/sensors", HTTP_PUT, [this](AsyncWebServerRequest *request) {}, NULL,
    [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {

    if (!check_authorization_header(request)) {
      request->send(HTTP_UNAUTHORIZED, utilshdl->getMimeType(".txt"), WRONG_AUTHORIZATION);
      return;
    }

    // Parâmetro sensor obrigatório na query
    const AsyncWebParameter* pSensor = request->getParam("sensor");
    if (!pSensor) { 
      request->send(HTTP_BAD_REQUEST, utilshdl->getMimeType(".txt"), "missing sensor"); 
      return; 
    }
   
    // Monta o corpo JSON (body)
    String body;
    for (size_t i = 0; i < len; i++) {
      body += (char)data[i];
    }

    // Faz parse do JSON
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, body);
    int newValue = 0;
    if (!err && doc["value"].is<int>()) {
      newValue = doc["value"].as<int>();
    }
    if (err) {
      request->send(HTTP_BAD_REQUEST, utilshdl->getMimeType(".txt"), "invalid json");
      return;
    }
    
    if (!(doc["value"].is<int>())) {
      request->send(HTTP_BAD_REQUEST, utilshdl->getMimeType(".txt"), "missing or invalid 'value'");
      return;
    }

    newValue = doc["value"].as<int>();
    if (newValue != 0 && newValue != 1) {
      request->send(HTTP_BAD_REQUEST, utilshdl->getMimeType(".txt"), "invalid value");
      return;
    }

    // Atualiza o pino
    pinMode(newValue, OUTPUT);
    int n = newValue==0?LOW:HIGH;
    digitalWrite(newValue, n);
    if (auto s = searchListSensor(newValue)) s->status = n;

    String resp = n == 0 ? "desativado":"ativado";
    request->send(HTTP_OK, utilshdl->getMimeType(".txt"), resp);
  });
}

void WebServerHandler::handleEventos(){
  server->on("/eventos", HTTP_GET, [this](AsyncWebServerRequest *request) {
    char filename[] = "/eventos.html";    
    String html = utilscds->lerArquivo(filename);
    if(html.length()==0) {
      html = HTML_MISSING_DATA_UPLOAD;
    } else {
      String mqttBroker = "";
      String mqttUser = "";
      String mqttPass = "";
      #ifdef USE_MQTT
        mqttBroker = utilscds->obtemMqttBroker();
        mqttUser   = utilscds->obtemMqttUser();
        mqttPass   = utilscds->obtemMqttPass();
      #endif
      html.replace("AIO_SERVER", mqttBroker);
      html.replace("AIO_USERNAME", mqttUser);
      html.replace("AIO_KEY", mqttPass);
    }
    request->send(HTTP_OK, utilshdl->getMimeType(filename), html);
  });
}

void WebServerHandler::handleLists(){
  server->on("/lists", HTTP_GET, [this](AsyncWebServerRequest *request) {
    if(check_authorization_header(request)) {
      String JSONmessage;
      Agenda *agd;
      for(int i = 0; i < agendaListaEncadeada.size(); i++){
        // Obtem a aplicação da lista
        agd = agendaListaEncadeada.get(i);
        JSONmessage += "{\"id\": "+String(i+1)+",\"data\": \""+String(agd->dataAgenda)+"\"}"+",";
      }
      request->send(HTTP_OK, utilshdl->getMimeType(".json"), "["+JSONmessage.substring(0, JSONmessage.length()-1)+"]");
    } else {
      request->send(HTTP_UNAUTHORIZED, utilshdl->getMimeType(".txt"), WRONG_AUTHORIZATION);
    }
  });
}

void WebServerHandler::handleInsertItemList(){
  server->on("/list", HTTP_POST, [this](AsyncWebServerRequest * request){}, NULL,
    [this](AsyncWebServerRequest * request, uint8_t *data, size_t len, size_t index, size_t total) {
    if(check_authorization_header(request)) {
      String JSONmessageBody;
      for (size_t i = 0; i < len; i++) {
        JSONmessageBody += (char)data[i];
      }
      JsonDocument doc;  // v7: alocação dinâmica
      DeserializationError error = deserializeJson(doc, JSONmessageBody);
      if(error) {
        request->send(HTTP_BAD_REQUEST, utilshdl->getMimeType(".json"), PARSER_ERROR);
      } else {
        //busco para checar se aplicacao já existe
        int index = searchList(doc["data"]);
        if(index == -1) {
          // não existe, então posso inserir
          // adiciona item na lista de agendamentos
          addAgenda(doc["data"]);
          // Grava no Storage
          saveAgendaList();
          doc.clear();
          request->send(HTTP_OK, utilshdl->getMimeType(".json"), INSERTED_ITEM);
        } else {
          request->send(HTTP_CONFLICT, utilshdl->getMimeType(".txt"), EXISTING_ITEM);
        }
      }
   } else {
    request->send(HTTP_UNAUTHORIZED, utilshdl->getMimeType(".txt"), WRONG_AUTHORIZATION);
   }
  });
}

void WebServerHandler::handleDeleteItemList(){
  server->on("/list", HTTP_DELETE, [this](AsyncWebServerRequest * request){}, NULL,
    [this](AsyncWebServerRequest * request, uint8_t *data, size_t len, size_t index, size_t total) {
    if(check_authorization_header(request)) {
      String JSONmessageBody;
      for (size_t i = 0; i < len; i++) {
        JSONmessageBody += (char)data[i];
      }
      JsonDocument doc;  // v7: alocação dinâmica
      DeserializationError error = deserializeJson(doc, JSONmessageBody);
      if(error) {
        request->send(HTTP_BAD_REQUEST, utilshdl->getMimeType(".json"), PARSER_ERROR);
      } else {
      //busco pela aplicacao a ser removida
      int index = searchList(doc["data"]);
      if(index != -1) {
        //removo
        removeAgenda(index);
        // Grava no Storage
        saveAgendaList();
        doc.clear();
        request->send(HTTP_OK, utilshdl->getMimeType(".txt"), REMOVED_ITEM);
      } else {
        request->send(HTTP_NOT_FOUND, utilshdl->getMimeType(".txt"), NOT_FOUND_ITEM);
      }
    }
   } else {
      request->send(HTTP_UNAUTHORIZED, utilshdl->getMimeType(".txt"), WRONG_AUTHORIZATION);
    }
  });
}

void WebServerHandler::handleOptions(){
  server->on("/", HTTP_OPTIONS, [this](AsyncWebServerRequest *request){
    request->send(HTTP_NO_CONTENT); // No Content
  });
}

void WebServerHandler::handleOnError(){
  server->onNotFound([this](AsyncWebServerRequest *request) {
    if (request->method() == HTTP_OPTIONS) {
      request->send(HTTP_NO_CONTENT); // responde ao preflight com 204
      return;
    }
    request->send(HTTP_NOT_FOUND, utilshdl->getMimeType(".txt"), "Rota não encontrada");
  });
}

AsyncWebServer * WebServerHandler::getWebServer() {
  return server;  
}

void WebServerHandler::onWsEvent(AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *data, size_t len){
  (void)server;  // evita -Wunused-parameter
  (void)arg;

  auto pushState = [this](){
    bool b25  = readLevelStable(PIN_LV25);
    bool b50  = readLevelStable(PIN_LV50);
    bool b75  = readLevelStable(PIN_LV75);
    bool b100 = readLevelStable(PIN_LV100);
  
    if (auto s = searchListSensor(PIN_LV25))  s->status  = b25;
    if (auto s = searchListSensor(PIN_LV50))  s->status  = b50;
    if (auto s = searchListSensor(PIN_LV75))  s->status  = b75;
    if (auto s = searchListSensor(PIN_LV100)) s->status  = b100;
  
    notifySensors("cx", b25, b50, b75, b100);
  };

  if (type == WS_EVT_CONNECT){
    pushState();                 // envia estado inicial ao conectar
  } else if (type == WS_EVT_DATA){
    Serial.println("WS_EVT_DATA -> atualizando estado");
    pushState();                 // reenvia quando chegar 'ping' do front
  }
}

void WebServerHandler::notifySensors(const String& id, bool s25, bool s50, bool s75, bool s100){
  String json;
  json.reserve(180);
  json = "{\"id\":\"" + id + "\",\"state\":{";
  json += "\"s25\":" + String(s25 ? "true" : "false") + ",";
  json += "\"s50\":" + String(s50 ? "true" : "false") + ",";
  json += "\"s75\":" + String(s75 ? "true" : "false") + ",";
  json += "\"s100\":" + String(s100 ? "true" : "false");
  json += "}}";
  ws->textAll(json);
}

void WebServerHandler::loop() {
  if (ws) {
    ws->cleanupClients();
  }
}

void WebServerHandler::startWebServer() {
  // carrega sensores  
  bool load = loadSensorList();
  if(!load) {
    #ifdef DEBUG
      Serial.println(F("Nao foi possivel carregar a lista de sensores!"));
    #endif
  }

  /* 
   *  Rotas sem bloqueios de token na API
   *  Configura as páginas de login e upload 
   *  de firmware OTA 
   */
  // Rotas das imagens a serem usadas na página (não tem basic auth)
  //não tem basic auth
  handleFileServing();  
  handleHealth();
  handleHome();
  handleSwagger();
  handleSwaggerUI();
  handleMetrics();
  //não tem basic auth
  
  /*
  * Rotas bloqueadas pelo token authorization
  */
  handlePorts();
  handleSensors();
  handleUpdateSensors();
  handleEventos();

  // Rotas bloqueadas pelo token authorization
  handleLists();
  handleInsertItemList();
  handleDeleteItemList();
  // ------------------------------------ //
      
  // se não se enquadrar em nenhuma das rotas
  handleOnError();
  
  // permitindo todas as origens. O ideal é trocar o '*' pela url do frontend poder utilizar a api com maior segurança
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Headers", "Access-Control-Allow-Headers, Origin, Accept, X-Requested-With, Content-Type, Access-Control-Request-Method, Access-Control-Request-Headers, Authorization");
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Credentials", "true");
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Methods", "GET,HEAD,OPTIONS,POST,PUT,DELETE");
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", callerOrigin);

  // startup web server
  ws->onEvent([this](AsyncWebSocket* server,
                   AsyncWebSocketClient* client,
                   AwsEventType type,
                   void* arg,
                   uint8_t* data,
                   size_t len){
    this->onWsEvent(server, client, type, arg, data, len);
  });
  server->addHandler(ws);
  server->begin();  
}

bool WebServerHandler::addSensor(int id, int gpio, int /*status*/) {
  ArduinoSensorPort *p = new ArduinoSensorPort();
  pinMode(gpio, ACTIVE_LOW ? INPUT_PULLUP : INPUT);
  delay(10); // estabiliza após configurar o pino

  p->id = id;
  p->gpio = gpio;
  p->status = readLevelStable(gpio); // estado inicial sem “fantasma”
  sensorListaEncadeada.add(p);
  return true;
}

bool WebServerHandler::loadSensorList(){
  // → RelayWater | → RelayLevel
  if(!addSensor(1, RelayWater, LOW)) return false;
  if(!addSensor(2, RelayLevel, LOW)) return false;
  return true;  
}

ArduinoSensorPort * WebServerHandler::searchListSensor(int gpio) {
  for(int i = 0; i < sensorListaEncadeada.size(); i++){
    ArduinoSensorPort *p = sensorListaEncadeada.get(i);
    if (gpio == p->gpio) return p;
  }
  return nullptr;
}

int WebServerHandler::searchList(String dataAgenda) {
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

bool WebServerHandler::validaHora(String hora) {
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

void WebServerHandler::addAgenda(String dataAgenda) {
  int ind = dataAgenda.indexOf(':');
  String hora = dataAgenda.substring(0, ind);
  String minutos = dataAgenda.substring(ind+1, dataAgenda.length());  
  Agenda *agdnew = new Agenda();  
  agdnew->dataAgenda = dataAgenda;
  agendaListaEncadeada.add(agdnew);
}

void WebServerHandler::removeAgenda(int index) {
  agendaListaEncadeada.remove(index);
}

void WebServerHandler::saveAgendaList() {
  Agenda *agd;
  String JSONmessage;
  for(int i = 0; i < agendaListaEncadeada.size(); i++){
    // Obtem a aplicação da lista
    agd = agendaListaEncadeada.get(i);
    JSONmessage += "{\"dataAgenda\": \""+agd->dataAgenda+"\"}"+",";
  }
  JSONmessage = "["+JSONmessage.substring(0, JSONmessage.length()-1)+"]";
  // Grava no storage
  utilscds->escreveArquivo("/lista.json", JSONmessage);
  #ifdef USE_MQTT
    // Grava no adafruit
    utilscds->atribuiFeed("list", JSONmessage.c_str());
  #endif
}

int WebServerHandler::loadAgendaList() {
  // Carrega do storage
  String JSONmessage = utilscds->lerArquivo("/lista.json");
  if(JSONmessage.length()==0) {
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

void WebServerHandler::nivelBaixo() {
  // se a bomba estiver ligada
  desligarBomba();
}

void WebServerHandler::nivelAlto() { 
  // ligo a bomba
  ligarBomba();
}

void WebServerHandler::ligarBomba(){
  Serial.println("Ligo a bomba");
  digitalWrite(RelayWater, HIGH);
  utilscds->atribuiFeed("water", "ON");
  
  // acendo a luz
  Serial.println("Acendo a luz");
  digitalWrite(RelayLight, HIGH);  
}
 
void WebServerHandler::desligarBomba(){
  // desligo a bomba
  Serial.println("Desligo a bomba");
  digitalWrite(RelayWater, LOW);
  utilscds->atribuiFeed("water", "OFF");

  //apago a luz
  Serial.println("Apago a luz");
  digitalWrite(RelayLight, LOW);
}

/**********************************************
 *  Rotas do portal (AP)
 **********************************************/
void WebServerHandler::registerPortalRoutes() {
  handleFileServing(); 
  // Captive endpoints comuns dos SOs → manda para "/"
  server->on("/generate_204", HTTP_ANY, [this](AsyncWebServerRequest* r){ r->redirect("/"); });
  server->on("/hotspot-detect.html", HTTP_ANY, [this](AsyncWebServerRequest* r){ r->redirect("/"); });
  server->on("/ncsi.txt", HTTP_ANY, [this](AsyncWebServerRequest* r){ r->redirect("/"); });

  // Página principal
  server->on("/", HTTP_GET, [this](AsyncWebServerRequest* request){
    Serial.println("[HTTP] GET /");
    String html = utilscds->lerArquivo("/wifimanager.html");
    if(html.length()==0) {
      Serial.println("readFile->registerPortalRoutes");
      request->send(HTTP_OK, utilshdl->getMimeType(".html"), MSG_ARQUIVO_NAO_ENCONTRADO);
    }
    else {
      request->send(HTTP_OK, utilshdl->getMimeType(".html"), html);
    }
  });

  // Salvar credenciais (usa PreferencesHandler do projeto)
  server->on("/save", HTTP_POST, [this](AsyncWebServerRequest* request){
    Serial.println("[HTTP] POST /save");
    String ssid = request->arg("ssid");
    String pass = request->arg("pass");
    if (ssid.isEmpty()) { request->send(HTTP_BAD_REQUEST, utilshdl->getMimeType(".txt"), "SSID vazio"); return; }

    utilscds->salvaDado("wifi", "ssid", ssid.c_str());
    utilscds->salvaDado("wifi", "pass", pass.c_str());
  
    request->send(HTTP_OK, utilshdl->getMimeType(".txt"), "Credenciais salvas. Reiniciando...");
    delay(3000);
    ESP.restart();
  });

  // Arquivos estáticos opcionais (CSS/JS/imagens) em /
  server->serveStatic("/", LittleFS, "/")
      .setDefaultFile("home.html")            // serve /home.html em "/"
      .setCacheControl("max-age=31536000");   // opcional: cache
}

/**********************************************
 *  AP + DNS cativo + portal
 **********************************************/
void WebServerHandler::startWebServerWifiManager(const String& apName) {
  
  Serial.println("==> Iniciando AP + DNS cativo + Portal");
  WiFi.mode(WIFI_AP);
  WiFi.softAP(apName.c_str());         // coloque senha se quiser: softAP(ssid, pass)
  IPAddress apIP = WiFi.softAPIP();
  Serial.printf("AP '%s' em %s\n", apName.c_str(), apIP.toString().c_str());

  const byte DNS_PORT = 53;
  dns.start(DNS_PORT, "*", apIP);            // captive DNS
  
  registerPortalRoutes();
  server->begin();
}

/**********************************************
 *  Conexão STA (Wi-Fi do roteador)
 **********************************************/
bool WebServerHandler::connectSTA(const String& hostForMDNS) {
  (void)hostForMDNS;

  savedSsid = utilscds->carregaDado("wifi", "ssid", savedSsid.c_str());
  savedPass = utilscds->carregaDado("wifi", "pass", savedPass.c_str());
  
  if (savedSsid.isEmpty()) {
    Serial.println(F("Sem credenciais salvas."));
    return false;
  }

  Serial.printf("Tentando STA: ssid='%s'\n", savedSsid.c_str());
  WiFi.mode(WIFI_STA);
  WiFi.persistent(false);
  WiFi.begin(savedSsid.c_str(), savedPass.c_str());

  for (int i = 0; i < 30 && WiFi.status() != WL_CONNECTED; i++) {
    delay(500);
    Serial.print('.');
  }
  Serial.println();

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println(F("Falha na conexão STA."));
    return false;
  }

  Serial.print(F("Conectado! IP: "));
  Serial.println(WiFi.localIP());
  return true;
}
