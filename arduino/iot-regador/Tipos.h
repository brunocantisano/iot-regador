#ifndef Tipos_h
#define Tipos_h

/************* lista de aplicacoes jenkins *************/
class Agenda {
  public:
    String dataAgenda;
};
/******************************************************/
/******************* lista de sensores ****************/
class ArduinoSensorPort {
  public:
    const char* name;
    byte id;
    byte gpio;
    byte status; // 1-TRUE / 0-FALSE
};
/******************************************************/
#endif
