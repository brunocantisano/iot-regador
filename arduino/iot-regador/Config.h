//Config.h
#ifndef CONFIG_H
#define CONFIG_H

#ifndef HTTP_REST_PORT
  #define HTTP_REST_PORT  80
#endif


#define MAX_PATH                   256
#define MAX_STRING_LENGTH          2000
#define MAX_FLOAT                  5

#define DEFAULT_VOLUME             70
/* ports */
#define D0                         16
#define D1                         5
#define D2                         4
#define D3                         0
#define D4                         2
#define D5                         14
#define D6                         12
#define D7                         13
#define D8                         15
#define D9                         16

#define RelayWater                 D8
#define RelayLight                 D7
#define RelayLevel                 D6

#endif // CONFIG_H