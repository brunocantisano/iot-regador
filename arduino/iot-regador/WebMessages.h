const char WRONG_AUTHORIZATION[] PROGMEM = "Authorization token errado";
const char NOT_AUTHORIZED_EXTENTIONS[] PROGMEM = "Extensão de arquivo inválida para upload";
const char SDCARD_PHOTO_WRITTEN[] PROGMEM = "Imagem salva no cartão SD com sucesso";
const char WRONG_STATUS[] PROGMEM = "Erro ao atualizar o status";
const char EXISTING_ITEM[] PROGMEM = "Item já existente na lista";
const char REMOVED_ITEM[] PROGMEM = "Item removido da lista";
const char REMOVED_FILE[] PROGMEM = "Arquivo removido";
const char UPLOADED_FILE[] PROGMEM = "Arquivo criado com sucesso";
const char NOT_FOUND_ITEM[] PROGMEM = "Item não encontrado na lista";
const char NOT_FOUND_ROUTE[] PROGMEM = "Rota nao encontrada";
const char PARSER_ERROR[] PROGMEM = "{\"message\": \"Erro ao fazer parser do json\"}";
const char HOUR_ERROR[] PROGMEM = "{\"message\": \"Erro ao validar hora de agendamento\"}";
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