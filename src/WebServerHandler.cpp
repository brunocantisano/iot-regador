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
                      utilscds(cds),
                      apiToken(token),
                      apiVersion(version),
                      host(hostServer),
                      callerOrigin(caller)
{
  server = new AsyncWebServer(HTTP_REST_PORT);
  ws = new AsyncWebSocket("/ws");
  strhdl   = utilscds->obtemStorage();
  utilshdl = utilscds->obtemUtilitarios();
  prefshdl = utilscds->obtemPreferences();
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
    String html = utilscds->lerArquivo("/home.html");
    if(html.isEmpty()) {
      html=String(MSG_ARQUIVO_NAO_ENCONTRADO);
    } else {
      html.replace("{{API_VERSION}}", apiVersion);
      html.replace("{{HOST_WATER_LEVEL}}", host + ".local");
      // Slug do dashboard no Adafruit IO (io.adafruit.com/<user>/dashboards/<slug>) -
      // nao tem relacao com o hostname mDNS do dispositivo.
      html.replace("{{AIO_DASHBOARD}}", "minion");

      String mqttStatus = "";
      #ifdef USE_MQTT
        String mqttUser = utilscds->obtemMqttUser();
        html.replace("{{AIO_USERNAME}}", mqttUser);
        if (utilscds->obtemMqttCredenciaisInvalidas()) {
          mqttStatus = "<strong style=\"color:#b00020\">Configuração MQTT inválida: usuário ou senha incorretos. "
                       "Corrija em <a href=\"/wifimanager.html\">/wifimanager.html</a>.</strong>";
        }
      #endif
      html.replace("{{MQTT_STATUS}}", mqttStatus);
    }
    request->send(HTTP_OK, utilshdl->getMimeType(".html"), html);
  });
}

void WebServerHandler::handleSwagger(){
  server->on("/swagger.json", HTTP_GET, [this](AsyncWebServerRequest *request) {
    if (!LittleFS.exists("/swagger.json")) {
      request->send(HTTP_OK, utilscds->obtemTipoMime(".json"), MSG_ARQUIVO_NAO_ENCONTRADO);
      return;
    }
    // Streaming direto do LittleFS (sem carregar o arquivo inteiro num String)
    // - o swagger.json nao tem nenhum '%' fora dos placeholders, entao o
    // template engine do ESPAsyncWebServer e seguro aqui (ver home.html
    // pra saber por que isso NAO seria seguro num arquivo com '%' legitimo).
    String apiVersionCopy = apiVersion;
    String hostname = host + ".local";
    request->send(LittleFS, "/swagger.json", "application/json", false,
      [apiVersionCopy, hostname](const String& var) -> String {
        if (var == "API_VERSION") return apiVersionCopy;
        if (var == "HOST_WATER_LEVEL") return hostname;
        return String();
      });
  });
}

void WebServerHandler::handleSwaggerUI(){
  server->on("/swaggerUI", HTTP_GET, [this](AsyncWebServerRequest *request) {
    String html = utilscds->lerArquivo("/swaggerUI.html");
    if(html.isEmpty()) {
      html=String(MSG_ARQUIVO_NAO_ENCONTRADO);
    } else {
      html.replace("{{HOST_WATER_LEVEL}}",host+".local");  
    }
    request->send(HTTP_OK, utilscds->obtemTipoMime(".html"), html);
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
      const int total = sensorListaEncadeada.size();
      String JSONmessage = "[";
      for(int i = 0; i < total; i++){
        // Obtem a aplicação da lista
        ArduinoSensorPort *arduinoSensorPort = sensorListaEncadeada.get(i);
        if (!arduinoSensorPort) continue; // defensivo: nao deveria acontecer dentro de [0,size())
        if (JSONmessage.length() > 1) JSONmessage += ",";
        JSONmessage += "{\"id\": \""+String(arduinoSensorPort->id)+"\",\"gpio\": \""+String(arduinoSensorPort->gpio)+"\",\"name\": \""+String(arduinoSensorPort->name)+"\"}";
      }
      JSONmessage += "]";
      request->send(HTTP_OK, utilshdl->getMimeType(".json"), JSONmessage);
    } else {
      // WRONG_AUTHORIZATION e PROGMEM (WebMessages.h) - send() normal faz
      // String::operator=(const char*), que chama strlen() comum (nao
      // safe pra flash) antes de copiar; send_P() evita isso (mesma causa
      // do crash que já corrigimos no HTML_FALLBACK do portal AP).
      request->send_P(HTTP_UNAUTHORIZED, utilshdl->getMimeType(".txt"), WRONG_AUTHORIZATION);
    }
  });
}

void WebServerHandler::handleSensors() {
  server->on("/sensors", HTTP_GET, [this](AsyncWebServerRequest *request) {
    if (!check_authorization_header(request)) {
      request->send_P(HTTP_UNAUTHORIZED, utilshdl->getMimeType(".txt"), WRONG_AUTHORIZATION);
      return;
    }

    const AsyncWebParameter* pLevel = request->getParam("level");
    if (!pLevel) { request->send(HTTP_BAD_REQUEST, utilshdl->getMimeType(".txt"), "missing level"); return; }

    // Valida que "level" e um inteiro 1..4 antes de derivar o pino - sem isso
    // um valor invalido (ausente, texto, fora do range) cai no pin=-1 sem
    // avisar o cliente, respondendo "desativado" para um pino que nao existe.
    String levelStr = pLevel->value();
    bool numeric = levelStr.length() > 0;
    for (size_t i = 0; i < levelStr.length() && numeric; i++) {
      if (!isDigit(levelStr.charAt(i))) numeric = false;
    }
    int level = numeric ? levelStr.toInt() : -1;
    if (level < 1 || level > 4) {
      request->send(HTTP_BAD_REQUEST, utilscds->obtemTipoMime(".txt"), "level invalido (use 1..4)");
      return;
    }

    int pin = 25;
    bool on = readLevelStable(pin);
    if (auto s = searchListSensor(pin)) s->status = on;
    String resp = on ? "ativado" : "desativado";
    request->send(HTTP_OK, utilscds->obtemTipoMime(".txt"), resp);
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
      html.replace("{{AIO_SERVER}}", mqttBroker);
      html.replace("{{AIO_USERNAME}}", mqttUser);
      html.replace("{{AIO_KEY}}", mqttPass);
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

  if (type == WS_EVT_CONNECT){
    Serial.printf("[WS] EVT_CONNECT client=%u heap_livre=%u maior_bloco=%u\n",
                  client ? client->id() : 0, ESP.getFreeHeap(), ESP.getMaxFreeBlockSize());

  } else if (type == WS_EVT_DATA){
    Serial.printf("[WS] EVT_DATA client=%u heap_livre=%u maior_bloco=%u\n",
                  client ? client->id() : 0, ESP.getFreeHeap(), ESP.getMaxFreeBlockSize());

  } else if (type == WS_EVT_DISCONNECT){
    Serial.printf("[WS] EVT_DISCONNECT client=%u heap_livre=%u maior_bloco=%u\n",
                  client ? client->id() : 0, ESP.getFreeHeap(), ESP.getMaxFreeBlockSize());
  } else if (type == WS_EVT_ERROR){
    Serial.printf("[WS] EVT_ERROR client=%u heap_livre=%u\n",
                  client ? client->id() : 0, ESP.getFreeHeap());
  }
}

void WebServerHandler::loop() {
  if (ws) {
    ws->cleanupClients();
  }
  if (_apMode) dns.processNextRequest();  // mesmo método, compatível
  if (_pendingRestartAfterSave && (long)(millis() - _pendingRestartDeadline) >= 0) {
    _pendingRestartAfterSave = false;
    ESP.restart();
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
    if (p && gpio == p->gpio) return p;
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

/************ utilitário ************/
void WebServerHandler::sendPortalFallback(AsyncWebServerRequest* request) {
  request->send_P(HTTP_OK, "text/html", HTML_FALLBACK);
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
    if (html.isEmpty()) {
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
    if (ssid.isEmpty()) { request->send(HTTP_BAD_REQUEST, utilscds->obtemTipoMime(".txt"), "SSID vazio"); return; }

    prefshdl->saveDataPreferentials("wifi", "ssid", ssid.c_str());
    prefshdl->saveDataPreferentials("wifi", "pass", pass.c_str());

    // Campos opcionais: em branco mantém o valor já salvo anteriormente.
    String userFirmware = request->arg("user_firmware");
    String passFirmware = request->arg("pass_firmware");
    String apiUser       = request->arg("api_user");
    String apiPass       = request->arg("api_pass");

    String callerOrigin  = request->arg("caller_origin");
    String mqttBroker    = request->arg("mqtt_broker");
    String mqttPort      = request->arg("mqtt_port");
    String mqttUsername  = request->arg("mqtt_username");
    String mqttPassword  = request->arg("mqtt_password");

    if (!userFirmware.isEmpty()) {
      prefshdl->saveDataPreferentials("firmware", "user", utilscds->encrypta(userFirmware));
      prefshdl->saveDataPreferentials("firmware", "userLen", String(userFirmware.length()).c_str());
    }
    if (!passFirmware.isEmpty()) {
      prefshdl->saveDataPreferentials("firmware", "pass", utilscds->encrypta(passFirmware));
      prefshdl->saveDataPreferentials("firmware", "passLen", String(passFirmware.length()).c_str());
    }
    // Token de API no formato HTTP Basic: base64("usuario:senha"), depois criptografado.
    if (!apiUser.isEmpty() && !apiPass.isEmpty()) {
      String basicAuthPlain = apiUser + ":" + apiPass;
      String basicAuthB64 = base64::encode(basicAuthPlain);
      prefshdl->saveDataPreferentials("api", "token", utilscds->encrypta(basicAuthB64));
      prefshdl->saveDataPreferentials("api", "tokenLen", String(basicAuthB64.length()).c_str());
    }
	  if (!callerOrigin.isEmpty()) {
      prefshdl->saveDataPreferentials("api", "callerOrigin", callerOrigin.c_str());
    }
    if (!mqttBroker.isEmpty()) {
      prefshdl->saveDataPreferentials("mqtt", "broker", mqttBroker.c_str());
    }
    if (!mqttPort.isEmpty()) {
      prefshdl->saveDataPreferentials("mqtt", "port", mqttPort.c_str());
    }
    if (!mqttUsername.isEmpty()) {
      prefshdl->saveDataPreferentials("mqtt", "username", utilscds->encrypta(mqttUsername));
      prefshdl->saveDataPreferentials("mqtt", "usernameLen", String(mqttUsername.length()).c_str());
    }
    if (!mqttPassword.isEmpty()) {
      prefshdl->saveDataPreferentials("mqtt", "password", utilscds->encrypta(mqttPassword));
      prefshdl->saveDataPreferentials("mqtt", "passwordLen", String(mqttPassword.length()).c_str());
    }
    String mdnsHost = host.isEmpty() ? "regador" : host;
    String html = F(
      "<!doctype html><html lang=\"pt-BR\"><head><meta charset=\"utf-8\">"
      "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
      "<title>Configuração salva</title><style>:root{--bg:#0f172a;--card:#1e293b;"
      "--card-border:#334155;--accent:#38bdf8;--text:#e2e8f0;--muted:#94a3b8;--ok:#22c55e}"
      "*{box-sizing:border-box}body{font-family:system-ui,-apple-system,\"Segoe UI\",Roboto,Arial,sans-serif;"
      "margin:0;min-height:100vh;display:flex;align-items:center;justify-content:center;"
      "background:radial-gradient(1200px 600px at 50% -10%,#1e293b 0,var(--bg) 60%);color:var(--text);padding:24px}"
      ".card{width:100%;max-width:420px;background:var(--card);border:1px solid var(--card-border);"
      "border-radius:16px;padding:32px 28px;box-shadow:0 20px 50px rgba(0,0,0,.45);text-align:center}"
      "h1{font-size:20px;margin:0 0 8px}p{color:var(--muted);font-size:14px;line-height:1.6;margin:0 0 20px}"
      "a.url{display:inline-block;color:#06283d;background:var(--accent);padding:12px 18px;"
      "border-radius:10px;font-weight:600;text-decoration:none;font-size:15px}"
      ".foot{display:flex;align-items:center;gap:8px;margin-top:20px;color:var(--muted);"
      "font-size:12px;justify-content:center}.dot{width:8px;height:8px;border-radius:50%;"
      "background:var(--ok);box-shadow:0 0 8px var(--ok)}</style></head><body><div class=\"card\">"
      "<h1>Configuração salva!</h1><p>O dispositivo vai reiniciar e conectar na sua rede Wi-Fi. "
      "Depois de alguns segundos, acesse o endereço abaixo pelo navegador:</p>"
      "<a class=\"url\" href=\"http://HOST.local\">http://HOST.local</a>"
      "<div class=\"foot\"><span class=\"dot\"></span><span>Reiniciando...</span></div>"
      "</div></body></html>"
    );
    html.replace("HOST", mdnsHost);
    // request->send() e assincrono - so enfileira o envio, nao garante que os
    // bytes ja chegaram no navegador. Em vez de um delay() as cegas (que
    // tanto pode reiniciar cedo demais - pagina em branco - quanto demorar
    // mais que o necessario), reinicia so quando o cliente desconectar (a
    // resposta ja sai com "Connection: close", entao isso dispara assim que
    // o navegador terminar de receber a pagina). O prazo abaixo e so uma
    // rede de seguranca caso o onDisconnect nunca chegue a disparar.
    this->_pendingRestartAfterSave = true;
    this->_pendingRestartDeadline = millis() + 5000;
    request->onDisconnect([this]() {
      if (this->_pendingRestartAfterSave) {
        this->_pendingRestartAfterSave = false;
        ESP.restart();
      }
    });
    request->send(HTTP_OK, utilshdl->getMimeType(".html"), html);
  });

  // Arquivos estáticos opcionais (CSS/JS/imagens) em /
  server->serveStatic("/", LittleFS, "/")
      .setDefaultFile("home.html")            // serve /home.html em "/"
      .setCacheControl("max-age=31536000");   // opcional: cache

  // Última defesa: qualquer rota desconhecida → fallback do portal
  server->onNotFound([this](AsyncWebServerRequest* request){
    Serial.printf("[404] %s\n", request->url().c_str());
    sendPortalFallback(request);
  });
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
// Traduz wl_status_t em texto legível para diagnosticar falha de conexão
// (senha errada, SSID fora de alcance, etc.) — ver wl_definitions.h.
static const char* wifiStatusToString(wl_status_t status) {
  switch (status) {
    case WL_IDLE_STATUS:     return "IDLE_STATUS (ainda tentando/sem resultado)";
    case WL_NO_SSID_AVAIL:   return "NO_SSID_AVAIL (SSID nao encontrado no ar - fora de alcance, oculto ou banda errada)";
    case WL_SCAN_COMPLETED:  return "SCAN_COMPLETED";
    case WL_CONNECTED:       return "CONNECTED";
    case WL_CONNECT_FAILED:  return "CONNECT_FAILED (provavel senha incorreta ou modo de seguranca incompativel)";
    case WL_CONNECTION_LOST: return "CONNECTION_LOST";
    case WL_WRONG_PASSWORD:  return "WRONG_PASSWORD (senha incorreta)";
    case WL_DISCONNECTED:    return "DISCONNECTED";
    default:                 return "desconhecido";
  }
}

static const char* wifiEncTypeToString(uint8_t encType) {
  switch (encType) {
    case ENC_TYPE_WEP:  return "WEP";
    case ENC_TYPE_TKIP: return "WPA/TKIP";
    case ENC_TYPE_CCMP: return "WPA2/CCMP";
    case ENC_TYPE_NONE: return "aberta (sem senha)";
    case ENC_TYPE_AUTO: return "WPA/WPA2 misto";
    default:             return "desconhecido";
  }
}

bool WebServerHandler::connectSTA(const String& hostForMDNS) {
  (void)hostForMDNS;

  savedSsid = utilscds->carregaDado("wifi", "ssid", savedSsid.c_str());
  savedPass = utilscds->carregaDado("wifi", "pass", savedPass.c_str());
  
  if (savedSsid.isEmpty()) {
    Serial.println(F("Sem credenciais salvas."));
    return false;
  }

  Serial.printf("Tentando STA: ssid='%s' (senha com %d caracteres)\n", savedSsid.c_str(), savedPass.length());

  // Diagnóstico: procura a rede salva no ar antes de tentar conectar, para
  // distinguir "SSID nao existe/fora de alcance/so 5GHz" de "senha errada".
  WiFi.mode(WIFI_STA);
  int redesEncontradas = WiFi.scanNetworks();
  bool redeEncontrada = false;
  for (int i = 0; i < redesEncontradas; i++) {
    if (WiFi.SSID(i) == savedSsid) {
      redeEncontrada = true;
      Serial.printf("  Rede encontrada no scan: RSSI=%ddBm canal=%d seguranca=%s\n",
                     WiFi.RSSI(i), WiFi.channel(i), wifiEncTypeToString(WiFi.encryptionType(i)));
    }
  }
  if (!redeEncontrada) {
    Serial.println(F("  AVISO: SSID salvo NAO apareceu no scan (fora de alcance, oculto, ou so 5GHz - ESP8266 nao suporta 5GHz)."));
  }
  WiFi.scanDelete();

  WiFi.persistent(false);
  WiFi.begin(savedSsid.c_str(), savedPass.c_str());

  for (int i = 0; i < 30 && WiFi.status() != WL_CONNECTED; i++) {
    delay(500);
    Serial.print('.');
  }
  Serial.println();

  if (WiFi.status() != WL_CONNECTED) {
    Serial.printf("Falha na conexao STA. status=%d (%s)\n", WiFi.status(), wifiStatusToString(WiFi.status()));
    return false;
  }

  Serial.print(F("Conectado! IP: "));
  Serial.println(WiFi.localIP());
  return true;
}

void WebServerHandler::atualizaApiToken(const String& token) {
  apiToken = token;
}