#ifndef pfodESP8266WebConfig_h
#define pfodESP8266WebConfig_h
/**
 (c)2015 Forward Computing and Control Pty. Ltd.
 This code may be freely used for both private and commerical use.
 Provide this copyright is maintained.
*/

#include <Arduino.h>
#include "Stream.h"
#include "pfodESP8266WebServer.h"
#include "pfodESP8266Utils.h"

// use PFOD_MODE for config of ESP8266 as a pfodDevice talking to pfodApp
#define PFOD_MODE 0
// use WIFI_SHIELD_MODE for config general purpose WiFi shield
#define WIFI_SHIELD_MODE 1

class pfodESP8266WebConfig {
	
public:
	pfodESP8266WebConfig(uint8_t _startAddress, uint8_t _mode=0) : webserver(80) {
		init( _startAddress, _mode);
	};
	void setupAP(const char* ssid_wifi, const char* password_wifi);
	void joinWiFiIfNotConnected();

	bool inConfigMode();
	void exitConfigMode();
  void setDebugStream(Print* out);
  static void handleConfig(pfodESP8266WebConfig*);
	static void handleNotFound(pfodESP8266WebConfig*);
	static void handleRoot(pfodESP8266WebConfig*);

// set the EEPROM structure
typedef struct  {
  uint32_t timeout;
  uint32_t baudRate;
  uint16_t portNo;
  char ssid[pfodESP8266Utils::MAX_SSID_LEN + 1]; // WIFI ssid + null
  char password[pfodESP8266Utils::MAX_PASSWORD_LEN + 1]; // WiFi password,  if empyt use OPEN, else use AUTO (WEP/WPA/WPA2) + null
  char staticIP[pfodESP8266Utils::MAX_STATICIP_LEN + 1]; // staticIP, if empty use DHCP + null
} STORAGE;
  STORAGE* loadConfigAndJoinNetwork();
  STORAGE* loadConfig();
  int loadConfigAndAttemptToJoinNetwork(); // returns status
  ESP8266WebServer* webserverPtr;
  STORAGE* getStorage();
	int handleClient(); // enum either 0 if no change, 1 if root served, 2 if config processed

private:
	boolean needToConnect;
	bool rootHandlerCalled;
	bool configHandlerCalled;
  void rootHandler();
  void configHandler();
  uint8_t mode;
	void init(uint8_t _startAddress,  uint8_t _mode);
  STORAGE storage;
	Print* debugOut;
	uint8_t startAddress;
  char ssid_found[pfodESP8266Utils::MAX_SSID_LEN + 1]; // max 32 chars + null
  const int STORAGE_SIZE = sizeof(storage);
  ESP8266WebServer webserver;
  const int DEFAULT_CONNECTION_TIMEOUT_SEC = 15;
  const char* aps;
  bool inConfig;
};


#endif // pfodESP8266WebConfig_h