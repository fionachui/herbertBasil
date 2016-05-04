#ifndef pfodESP8266Utils_h
#define pfodESP8266Utils_h

#include <stdint.h>
#include <ctype.h>
#include <string.h>
#include <pfodESP8266WiFi.h>

class pfodESP8266Utils {
  public:
  	// some constants for field sizes
  static const int MAX_SSID_LEN = 32;
  static const int MAX_PASSWORD_LEN = 64;
  static const int MAX_STATICIP_LEN = 40;
  static size_t strncpy_safe(char* dest, const char* src, size_t maxLen);
  static void urldecode2(char *dst, const char *src);
  static uint8_t* parseLong(uint8_t* idxPtr, long *result);
  static uint8_t isEmpty(const char* str);
  static uint32_t ipStrToNum(const char* ipStr);
  static const char* encryptionTypeToStr(uint8_t type);
  static const char* scanForStrongestAP(char* result, size_t resultLen);
  

  static const char WEP[];
  static const char TKIP[];
  static const char CCMP[];
  static const char NONE[];
  static const char AUTO[];
  static const char UNKNOWN_ENCRY[];

};

#endif // pfodESP8266Utils_h