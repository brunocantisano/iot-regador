String getMetrics() {
  String p = "";

  int sketch_size = ESP.getSketchSize();
  int flash_size =  ESP.getFreeSketchSpace();
  int available_size = flash_size - sketch_size;
  int heap_size = ESP.getFreeContStack();
  int free_heap = ESP.getFreeHeap();
  
  setMetric(&p, "esp8266_uptime", String(millis()));
  setMetric(&p, "esp8266_wifi_rssi", String(WiFi.RSSI()));
  setMetric(&p, "esp8266_sketch_size", String(sketch_size));
  setMetric(&p, "esp8266_flash_size", String(flash_size));
  setMetric(&p, "esp8266_available_size", String(available_size));
  setMetric(&p, "esp8266_heap_size", String(heap_size));
  setMetric(&p, "esp8266_free_heap", String(free_heap));
  setMetric(&p, "esp8266_boot_counter", String(getBootCounter()));  
  setMetric(&p, "esp8266_water1", String(readSensorStatus(RelayWater1)));
  setMetric(&p, "esp8266_water2", String(readSensorStatus(RelayWater2)));
  setMetric(&p, "esp8266_level", String(readSensorStatus(RelayLevel)));

  return p;
}

const char* uint64ToText(uint64_t input) {
  String result = "";
  uint8_t base = 10;

  do {
    char c = input % base;
    input /= base;

    if (c < 10)
      c +='0';
    else
      c += 'A' - 10;
    result = c + result;
  } while (input);
  return result.c_str();
}

/**
   Layout

   # esp8266_uptime
   # TYPE esp8266_uptime gauge
   esp8266_uptime 23899

*/
void setMetric(String *p, String metric, String value) {
  *p += "# " + metric + "\n";
  *p += "# TYPE " + metric + " gauge\n";
  *p += "" + metric + " ";
  *p += value;
  *p += "\n";
}

int getBootCounter() {
  return preferences.getInt("boot");
}

void incrementBootCounter() {
  int boot = getBootCounter();
  preferences.putInt("boot", (boot + 1));
}

void setupStorage() {
  preferences.begin("storage", false);
}

void closeStorage() {
  preferences.end();
}
