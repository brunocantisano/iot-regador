// WebServerHandler.h
#ifndef WEBSERVERHANDLER_H
#define WEBSERVERHANDLER_H

#undef HTTP_GET
#undef HTTP_POST
#undef HTTP_PUT
#undef HTTP_DELETE

#if defined(ESP8266)
/*******************************
 *  Build para ESP8266
 *******************************/
	#include <ESP8266WiFi.h>
#else // ESP32
/*******************************
 *  Build para ESP32
 *******************************/
	#include <WiFi.h>
#endif

#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ESPAsyncDNSServer.h>

#include <pgmspace.h>   // PROGMEM
#include <ArduinoJson.h>
#include <Regexp.h>
#include "Config.h"
#include "WebMessages.h"
#include "Tipos.h"
#include "StorageHandler.h"
#include "UtilsHandler.h"
#include "MqttHandler.h"
#include "PreferencesHandler.h"
#include "HttpStatusCodes.h"

#include <ArduinoUtilsCds.h>

#define HTTP_REST_PORT             80
#define MAX_PAYLOAD_SIZE           2000

#define RelayWater                 D8
#define RelayLight                 D7
#define RelayLevel                 D6

class WebServerHandler {
private:
    AsyncWebServer * server;
    AsyncWebSocket * ws;      // rota do websocket
    // Lista de sensores
    ListaEncadeada<ArduinoSensorPort*> sensorListaEncadeada = ListaEncadeada<ArduinoSensorPort*>();
    // Lista de aplicacoes do jenkins
    ListaEncadeada<Agenda*> agendaListaEncadeada = ListaEncadeada<Agenda*>();

    AsyncDNSServer dns;
    String apiToken;
    String apiVersion;
    String host;
    ArduinoUtilsCds * utilscds;
    String callerOrigin;
    String savedSsid;
    String savedPass;
    String obtemEstadoSensor(int sensor);
    String obtemMetricas();
    void atribuiMetrica(String *p, String metric, String value);
    int obtemContagemBoots();
    void incrementaContagemBoots();
    bool check_authorization_header(AsyncWebServerRequest * request);
    char payloadBuffer[MAX_PAYLOAD_SIZE];
    void handleFileServing();
    void handleHome();
    void handleSwagger();
    void handleSwaggerUI();
    void handleHealth();
    void handleMetrics();
    void handlePorts();
    void handleSensors();
    void handleWaterList();
    void handleRegadorRobo();
    void handleEventos();
    void handleLists();
    void handleOptions();
    void handleOnError();    
    void registerPortalRoutes();
    void notifySensors(const String& id, bool sWater, bool sLevel);
    void onWsEvent(AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *data, size_t len);
public:
    WebServerHandler(
        const String& token, 
        const String& version, 
        const String& hostServer, 
        const String& caller,
        ArduinoUtilsCds * cds);
    ~WebServerHandler();

    void startWebServer(void);
    bool connectSTA(const String& hostForMDNS);
    void startWebServerWifiManager(const String& apName);
    bool loadSensorList();
    bool addSensor(int id, int gpio, int status);
    ArduinoSensorPort * searchListSensor(int gpio);
    
    int searchList(String dataAgenda);
    bool validaHora(String hora);
    void addAgenda(String dataAgenda);
    void saveAgendaList();
    int loadAgendaList();
    void nivelBaixo();
    void nivelAlto();
    void ligarBomba();
    void desligarBomba();
    AsyncWebServer * getWebServer();
};

#endif
