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
#include <DNSServer.h>
#include <pgmspace.h>   // PROGMEM
#include <ArduinoJson.h>
#include "Tipos.h" 
#include "StorageHandler.h"
#include "UtilsHandler.h"
#include "PreferencesHandler.h"
#include "HttpStatusCodes.h"
#include <ArduinoUtilsCds.h>
#include "WebMessages.h"
#include <Regexp.h>
#include "Config.h"

#define MAX_PAYLOAD_SIZE           2000
#define HTTP_REST_PORT             80
#define RelayWater                 D8
#define RelayLight                 D7
#define RelayLevel                 D6

/**********************************************
 *  HTML fallback do portal (se /wifimanager.html não existir no LittleFS)
 **********************************************/
static const char HTML_FALLBACK[] PROGMEM = R"HTML(
<!doctype html><html lang="pt-BR"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>Arquivo não encontrado</title><style>:root{--bg:#0f172a;--card:#1e293b;--card-border:#334155;--accent:#f59e0b;--text:#e2e8f0;--muted:#94a3b8;--code-bg:#0f172a;--warn:#f59e0b}*{box-sizing:border-box}body{font-family:system-ui,-apple-system,"Segoe UI",Roboto,Arial,sans-serif;margin:0;min-height:100vh;display:flex;align-items:center;justify-content:center;background:radial-gradient(1200px 600px at 50% -10%,#1e293b 0,var(--bg) 60%);color:var(--text);padding:24px}.card{width:100%;max-width:460px;background:var(--card);border:1px solid var(--card-border);border-radius:16px;padding:32px 28px;box-shadow:0 20px 50px rgba(0,0,0,.45)}.brand{display:flex;align-items:center;gap:12px;margin-bottom:8px}.brand .icon{width:44px;height:44px;flex:0 0 44px;display:flex;align-items:center;justify-content:center;border-radius:12px;background:linear-gradient(135deg,var(--accent),#d97706)}.brand .icon svg{width:24px;height:24px}h1{font-size:19px;margin:0;font-weight:600}.sub{color:var(--muted);font-size:13px;margin:4px 0 0}.body-text{font-size:14px;line-height:1.6;color:var(--text);margin:22px 0 18px}.body-text strong{color:#fff}.steps{background:var(--code-bg);border:1px solid var(--card-border);border-radius:10px;padding:16px 18px;margin-bottom:20px}.steps p{margin:0 0 10px;font-size:12px;text-transform:uppercase;letter-spacing:.04em;color:var(--muted);font-weight:600}.steps ol{margin:0;padding-left:20px;font-size:13.5px;line-height:1.9;color:var(--text)}.steps code{background:#1e293b;border:1px solid var(--card-border);border-radius:4px;padding:2px 6px;font-size:12.5px;color:var(--accent);font-family:ui-monospace,SFMono-Regular,Menlo,Consolas,monospace}.foot{display:flex;align-items:center;gap:8px;margin-top:20px;color:var(--muted);font-size:12px;justify-content:center}.dot{width:8px;height:8px;border-radius:50%;background:var(--warn);box-shadow:0 0 8px var(--warn)}</style></head><body><div class="card"><div class="brand"><div class="icon"><svg viewBox="0 0 24 24" fill="none" stroke="#fff" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M12 9v4"/><path d="M12 17h.01"/><path d="M10.29 3.86 1.82 18a2 2 0 0 0 1.71 3h16.94a2 2 0 0 0 1.71-3L13.71 3.86a2 2 0 0 0-3.42 0Z"/></svg></div><div><h1>Página do portal não encontrada</h1><div class="sub">O arquivo <code style="background:#0f172a;border:1px solid #334155;border-radius:4px;padding:1px 5px;color:#f59e0b">wifimanager.html</code> não está no armazenamento</div></div></div><p class="body-text">Esta é uma <strong>página de emergência</strong> exibida automaticamente porque o firmware não encontrou o arquivo de interface do portal Wi-Fi no sistema de arquivos (LittleFS) do dispositivo. Isso geralmente acontece quando o firmware foi gravado sem o upload correspondente dos arquivos estáticos.</p><div class="steps"><p>Como resolver</p><ol><li>Confirme que o arquivo <code>wifimanager.html</code> existe na pasta de dados do projeto (<code>/data</code>).</li><li>Grave o sistema de arquivos no dispositivo, via PlatformIO: <code>pio run --target uploadfs</code></li><li>Reinicie o dispositivo após a gravação concluir.</li></ol></div><p class="body-text" style="margin:0;font-size:13px;color:var(--muted)">Se o problema persistir, verifique se o <code>board_build.filesystem</code> está configurado como <code>littlefs</code> no <code>platformio.ini</code> e se a partição de dados tem espaço suficiente.</p><div class="foot"><span class="dot"></span><span>Modo de configuração ativo — Ponto de acesso</span></div></div></body></html>
)HTML";

class WebServerHandler {
private:
    AsyncWebServer * server;
    AsyncWebSocket * ws;      // rota do websocket
    ArduinoUtilsCds * utilscds;
    UtilsHandler * utilshdl;
    StorageHandler * strhdl;
    PreferencesHandler * prefshdl;
    // Lista de sensores
    ListaEncadeada<ArduinoSensorPort*> sensorListaEncadeada = ListaEncadeada<ArduinoSensorPort*>();
    // Lista de aplicacoes do jenkins
    ListaEncadeada<Agenda*> agendaListaEncadeada = ListaEncadeada<Agenda*>();

    DNSServer dns;
    String apiToken;
    String apiVersion;
    String host;
    String callerOrigin;
    bool _apMode;
    // Restart adiado apos /save: so reinicia quando o cliente confirmar que
    // recebeu a pagina de confirmacao (onDisconnect), com um prazo maximo de
    // seguranca caso a desconexao nunca chegue a disparar.
    bool _pendingRestartAfterSave = false;
    unsigned long _pendingRestartDeadline = 0;    
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
    void handleUpdateSensors();
    void handleEventos();
    void handleLists();
    void handleInsertItemList();
    void handleDeleteItemList();
    void handleOptions();
    void handleOnError();
    void sendPortalFallback(AsyncWebServerRequest* request);
    void registerPortalRoutes();
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
    void loop();
    bool connectSTA(const String& hostForMDNS);
    void startWebServerWifiManager(const String& apName);
    bool loadSensorList();
    bool addSensor(int id, int gpio, int status);
    ArduinoSensorPort * searchListSensor(int gpio);
    
    int searchList(String dataAgenda);
    bool validaHora(String hora);
    void addAgenda(String dataAgenda);
    void removeAgenda(int index);
    void saveAgendaList();
    int loadAgendaList();
    void nivelBaixo();
    void nivelAlto();
    void ligarBomba();
    void desligarBomba();
    void atualizaApiToken(const String& token);
    AsyncWebServer * getWebServer();
};

#endif
