#include "pfodESP8266Utils.h"

// will always put '\0\ at dest[maxLen]
// return the number of char copied excluding the terminating null
size_t pfodESP8266Utils::strncpy_safe(char* dest, const char* src, size_t maxLen) {
  size_t rtn = 0;
  if (src == NULL) {
    dest[0] = '\0';
  } else {
    strncpy(dest, src, maxLen);
    rtn = strlen(src);
    if ( rtn > maxLen) {
      rtn = maxLen;
    }
  }
  dest[maxLen] = '\0';
  return rtn;
}

void pfodESP8266Utils::urldecode2(char *dst, const char *src) {
  char a, b, c;
  if (dst == NULL) return;
  while (*src) {
    if (*src == '%') {
      if (src[1] == '\0') {
        // don't have 2 more chars to handle
        *dst++ = *src++; // save this char and continue
        // next loop will stop
        continue;
      }
    }
    if ((*src == '%') &&
        ((a = src[1]) && (b = src[2])) &&
        (isxdigit(a) && isxdigit(b))) {
      // here have at least src[1] and src[2] (src[2] may be null)
      if (a >= 'a')
        a -= 'a' - 'A';
      if (a >= 'A')
        a -= ('A' - 10);
      else
        a -= '0';
      if (b >= 'a')
        b -= 'a' - 'A';
      if (b >= 'A')
        b -= ('A' - 10);
      else
        b -= '0';
      *dst++ = 16 * a + b;
      src += 3;
    }
    else {
      c = *src++;
      if (c == '+')c = ' ';
      *dst++ = c;
    }
  }
  *dst = '\0'; // terminate result
}

/**
 * parseLong
 * will parse between  -2,147,483,648 to 2,147,483,647
 * No error checking done.
 * will return 0 for empty string, i.e. first uint8_t == null
 *
 * Inputs:
 *  idxPtr - byte* pointer to start of bytes to parse
 *  result - long* pointer to where to store result
 * return
 *   byte* updated pointer to bytes after skipping terminator
 *
 */
uint8_t* pfodESP8266Utils::parseLong(uint8_t* idxPtr, long *result) {
  long rtn = 0;
  bool neg = false;
  while ( *idxPtr != 0) {
    if (*idxPtr == '-') {
      neg = true;
    } else {
      rtn = (rtn << 3) + (rtn << 1); // *10 = *8+*2
      rtn = rtn +  (*idxPtr - '0');
    }
    ++idxPtr;
  }
  if (neg) {
    rtn = -rtn;
  }
  *result = rtn;
  return ++idxPtr; // skip null
}

/**
 *  check for uint8_t !=32
 * return zero if one found
 */
uint8_t pfodESP8266Utils::isEmpty(const char* str) {
  for (; *str != 0; str++) {
    if ((*str != 32)) {
      return 0;
    }
  }
  return 1;
}

/**
 * ipStrToNum
 * works for IPV4 only
 * parses  "10.1.1.200" and "10,1,1,200" strings 
 * extra spaces ignored  eg "10, 1, 1, 200" is OK also 
 * return uint32_t for passing to ipAddress( );
 */
uint32_t pfodESP8266Utils::ipStrToNum(const char* ipStr) {
  const int SIZE_OF_NUMS = 4;
  union {
	uint8_t bytes[SIZE_OF_NUMS];  // IPv4 address
	uint32_t dword;
  } _address;
  _address.dword = 0; // clear return

  int i = 0;
  uint8_t num = 0; // start with 0
  while ((*ipStr) != '\0') {
    // while not end of string
    if ((*ipStr == '.') || (*ipStr == ',')) {
      // store num and move on to next position
      _address.bytes[i++] = num;
      num = 0;
      if (i>=SIZE_OF_NUMS) {
        break; // filled array
      }
    } else {  
      if (*ipStr != ' ') {     	// skip blanks
        num = (num << 3) + (num << 1); // *10 = *8+*2
        num = num +  (*ipStr - '0');
      }  
    }  
    ipStr++;
  }  
  if (i<SIZE_OF_NUMS) {
    // store last num
    _address.bytes[i++] = num;
  }
  return _address.dword; 
}  

   const char pfodESP8266Utils::WEP[] = "WEP";
   const char pfodESP8266Utils::TKIP[] = "TKIP";
   const char pfodESP8266Utils::CCMP[] = "CCMP";
   const char pfodESP8266Utils::NONE[] = "NONE";
   const char pfodESP8266Utils::AUTO[] = "WEP/WPA/WPA2";
   const char pfodESP8266Utils::UNKNOWN_ENCRY[] = "--UNKNOWN--";

const char* pfodESP8266Utils::encryptionTypeToStr(uint8_t type) {
  if (type == ENC_TYPE_WEP) {
    return  WEP;
  } else if (type == ENC_TYPE_TKIP) {
    return  TKIP;
  } else if (type == ENC_TYPE_CCMP) {
    return  CCMP;
  } else if (type == ENC_TYPE_NONE) {
    return  NONE;
  } else if (type == ENC_TYPE_AUTO) {
    return  AUTO;
  } //else {
  return UNKNOWN_ENCRY;
}


/**
 * will return name of AP with strongest signal found or return empty string if none found
 */
const char* pfodESP8266Utils::scanForStrongestAP(char* result, size_t resultLen) {
  // WiFi.scanNetworks will return the number of networks found
  int8_t n = WiFi.scanNetworks();
#ifdef DEBUG
  Serial.print ("Scan done\n");
#endif
  delay(0);
  int32_t maxRSSI = -1000;
  pfodESP8266Utils::strncpy_safe((char*)result, "", resultLen); // empty
  if (n <= 0) {
#ifdef DEBUG
    Serial.print("No networks found\n");
#endif
  } else {
#ifdef DEBUG
    Serial.print("Networks found:");
    Serial.println(n);
#endif
    for (int8_t i = 0; i < n; ++i) {
      const char * ssid_scan = WiFi.SSID_charPtr(i);
      int32_t rssi_scan = WiFi.RSSI(i);
      uint8_t sec_scan = WiFi.encryptionType(i);
      if (rssi_scan > maxRSSI) {
        maxRSSI = rssi_scan;
        pfodESP8266Utils::strncpy_safe((char*)result, ssid_scan, resultLen);
      }
#ifdef DEBUG
      Serial.print(ssid_scan);
      Serial.print(" ");
      Serial.print(encryptionTypeToStr(sec_scan));
      Serial.print(" ");
      Serial.println(rssi_scan);
#endif
      delay(0);
    }
  }
  return result;
}

