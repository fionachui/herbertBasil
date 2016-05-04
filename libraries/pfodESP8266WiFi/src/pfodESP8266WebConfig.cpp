// pfodESP8266WebConfig.cpp
/**
 (c)2015 Forward Computing and Control Pty. Ltd.
 This code may be freely used for both private and commerical use.
 Provide this copyright is maintained.
*/

#include <stdint.h>
#include <EEPROM.h>
#include "pfodESP8266WebConfig.h"

// uncomment this next line and call setDebugStream(&Serial); to enable debug out
#define DEBUG

void pfodESP8266WebConfig::handleRoot(pfodESP8266WebConfig* ptr) {
  ptr->rootHandler();
}

void pfodESP8266WebConfig::handleConfig(pfodESP8266WebConfig* ptr) {
  ptr->configHandler();
}
void pfodESP8266WebConfig::handleNotFound(pfodESP8266WebConfig* ptr) {
  ptr->rootHandler();
}

void pfodESP8266WebConfig::init(uint8_t _startAddress,  uint8_t _mode) {
  startAddress = _startAddress;
  webserverPtr = &webserver;
  webserverPtr->on ( "/", std::bind(&pfodESP8266WebConfig::handleRoot, this));
  webserverPtr->on ( "/config", std::bind(&pfodESP8266WebConfig::handleConfig, this));
  webserverPtr->onNotFound( std::bind(&pfodESP8266WebConfig::handleNotFound, this));
  mode = _mode;
  inConfig = false;
  configHandlerCalled = false;
  rootHandlerCalled = false;
  needToConnect = false;
}

void pfodESP8266WebConfig::setDebugStream(Print* out) {
  debugOut = out;
}

// returns 2 if config processed, 1 if root called, 0 if no change
int pfodESP8266WebConfig::handleClient() {
  webserverPtr->handleClient();
  if (configHandlerCalled) {
    return 2;
  } else if (rootHandlerCalled) {
    return 1;
  } //else
  return 0;
}

bool pfodESP8266WebConfig::inConfigMode() {
  return inConfig;
}

void pfodESP8266WebConfig::exitConfigMode() {
  if (inConfig) {
    WiFiClient::stopAll(); // cleans up memory
    webserver.close();
    WiFi.softAPdisconnect();
    WiFi.mode(WIFI_STA);
    needToConnect = true;
  }
  inConfig = false;
#ifdef DEBUG
  if (debugOut) {
    debugOut->println(F("Exit pfodWifiWebConfig"));
  }
#endif
}

void pfodESP8266WebConfig::setupAP(const char* ssid_wifi, const char* password_wifi) {
  inConfig = true;
  rootHandlerCalled = false;
  configHandlerCalled = false;
  WiFi.mode(WIFI_AP);
  aps = pfodESP8266Utils::scanForStrongestAP((char*)&ssid_found, pfodESP8266Utils::MAX_SSID_LEN + 1);
  delay(0);
  IPAddress local_ip = IPAddress(10, 1, 1, 1);
  IPAddress gateway_ip = IPAddress(10, 1, 1, 1);
  IPAddress subnet_ip = IPAddress(255, 255, 255, 0);
#ifdef DEBUG
  if (debugOut) {
    debugOut->print(F("configure pfodWifiWebConfig mode:"));
    debugOut->println(mode);
  }
#endif
  WiFi.softAP(ssid_wifi, password_wifi);

#ifdef DEBUG
  if (debugOut) {
    debugOut->println(F("Access Point setup"));
  }
#endif
  WiFi.softAPConfig(local_ip, gateway_ip, subnet_ip);

#ifdef DEBUG
  if (debugOut) {
    debugOut->println("done");
    IPAddress myIP = WiFi.softAPIP();
    debugOut->print(F("AP IP address: "));
    debugOut->println(myIP);
  }
#endif
  delay(10);
  webserver.begin();
#ifdef DEBUG
  if (debugOut) {
    debugOut->println ( "HTTP webserver started" );
  }
#endif
}

pfodESP8266WebConfig::STORAGE* pfodESP8266WebConfig::getStorage() {
  return &storage;
}

pfodESP8266WebConfig::STORAGE* pfodESP8266WebConfig::loadConfig() {
  uint8_t * byteStorageRead = (uint8_t *) & (pfodESP8266WebConfig::storage);
  for (size_t i = 0; i < STORAGE_SIZE; i++) {
    byteStorageRead[i] = EEPROM.read(startAddress + i);
  }
  // clean up portNo
  if ((storage.portNo == 0xffff) || (storage.portNo == 0)) {
    storage.portNo = 4989; // default
  }
  return &storage;
}


// return wifi status
int pfodESP8266WebConfig::loadConfigAndAttemptToJoinNetwork() {
  loadConfig();
  // Initialise wifi module
  /**
  #ifdef DEBUG
    if (debugOut) {
      debugOut->println(F("Connecting to AP"));
    }
  #endif
  **/
  return  WiFi.begin(storage.ssid, storage.password);
}

void pfodESP8266WebConfig::joinWiFiIfNotConnected() {
  if (WiFi.status() != WL_CONNECTED) {
    if (!needToConnect) {
      needToConnect = true; // set this each time we loose connection
      WiFi.disconnect();
#ifdef DEBUG
      if (debugOut) {
        debugOut->print(F("NOT connected "));
      }
#endif
    }
  }

  if (needToConnect && (loadConfigAndAttemptToJoinNetwork() == WL_CONNECTED)) {
    // did not have connection and have one now
    needToConnect = false;
    // do this again as when switching from DHCP to static IP do not get static IP
    pfodESP8266WebConfig::STORAGE *storage = getStorage();
#ifdef DEBUG
    if (debugOut) {
      debugOut->print(F("Connected to "));    debugOut->println(storage->ssid);
    }
#endif
    if (*(storage->staticIP) != '\0') {
      // config static IP
      IPAddress ip(pfodESP8266Utils::ipStrToNum(storage->staticIP));
      IPAddress gateway(ip[0], ip[1], ip[2], 1); // set gatway to ... 1
#ifdef DEBUG
      if (debugOut) {
        debugOut->print(F("Setting IP to "));
        debugOut->println(storage->staticIP);
      }
#endif
      IPAddress subnet(255, 255, 255, 0);
      WiFi.config(ip, gateway, subnet);
    } else {
      //leave as DHCP
#ifdef DEBUG
      if (debugOut) {
        debugOut->print(F("DHCP IP is "));      Serial.println(WiFi.localIP());
      }
#endif
    }
  } else {
    delay(0);
  }
}

pfodESP8266WebConfig::STORAGE *pfodESP8266WebConfig::loadConfigAndJoinNetwork() {
  uint8_t * byteStorageRead = (uint8_t *) & (pfodESP8266WebConfig::storage);
  for (size_t i = 0; i < STORAGE_SIZE; i++) {
    byteStorageRead[i] = EEPROM.read(startAddress + i);
  }
  // Initialise wifi module
#ifdef DEBUG
  if (debugOut) {
    debugOut->println();
    debugOut->println(F("Connecting to AP"));
    debugOut->print("ssid '");
    debugOut->print(storage.ssid);
    debugOut->println("'");
    debugOut->print("password '");
    debugOut->print(storage.password);
    debugOut->println("'");
  }
#endif
  /**********
    // do this before begin to set static IP and not enable dhcp
    if (*(storage.staticIP) != '\0') {
      // config static IP
      IPAddress ip(pfodESP8266Utils::ipStrToNum(storage.staticIP));
      IPAddress gateway(ip[0], ip[1], ip[2], 1); // set gatway to ... 1
      IPAddress subnet(255, 255, 255, 0);
      WiFi.config(ip, gateway, subnet);
    } // else leave as DHCP
  **********/
  WiFi.begin(storage.ssid, storage.password);
  int counter = 0;
  while ((WiFi.status() != WL_CONNECTED) && (counter < 40)) {
    delay(500);
    counter++;
#ifdef DEBUG
    if (debugOut) {
      debugOut->print(".");
    }
#endif
  }
  if (WiFi.status() != WL_CONNECTED) {
#ifdef DEBUG
    if (debugOut) {
      debugOut->println();
      debugOut->println(F("Could not connect!"));
    }
#endif
    return NULL;
  }
#ifdef DEBUG
  if (debugOut) {
    debugOut->println();
    debugOut->println(F("Connected!"));
  }
#endif
  // do this again as when switching from DHCP to static IP do not get static IP
  if (*(storage.staticIP) != '\0') {
    // config static IP
    IPAddress ip(pfodESP8266Utils::ipStrToNum(storage.staticIP));
    IPAddress gateway(ip[0], ip[1], ip[2], 1); // set gatway to ... 1
#ifdef DEBUG
    if (debugOut) {
      debugOut->print(F("Setting gateway to: "));
      debugOut->println(gateway);
    }
#endif
    IPAddress subnet(255, 255, 255, 0);
    WiFi.config(ip, gateway, subnet);
  } // else leave as DHCP

  // Start listening for connections
#ifdef DEBUG
  if (debugOut) {
    // Print the IP address
    debugOut->print(WiFi.localIP());
    debugOut->print(':');
    debugOut->println(storage.portNo);
    debugOut->println(F("Start Server"));
  }
#endif

  return &storage;
}

void pfodESP8266WebConfig::configHandler() {
  if (!rootHandlerCalled) {
    rootHandler();  // always display config page first
    return;
  }

  // set defaults
  uint16_t portNo = 80;
  uint32_t timeout = DEFAULT_CONNECTION_TIMEOUT_SEC * 1000; // time out in 15 sec
  uint32_t baudRate = 19200;
#ifdef DEBUG
  if (debugOut) {
    debugOut->print("configHandler mode:");
    debugOut->println(mode);
  }
#endif

  if (webserver.args() > 0) {
#ifdef DEBUG
    if (debugOut) {
      String message = "Config results\n\n";
      message += "URI: ";
      message += webserver.uri();
      message += "\nMethod: ";
      message += ( webserver.method() == HTTP_GET ) ? "GET" : "POST";
      message += "\nArguments: ";
      message += webserver.args();
      message += "\n";
      for ( uint8_t i = 0; i < webserver.args(); i++ ) {
        message += " " + webserver.argName ( i ) + ": " + webserver.arg ( i ) + "\n";
      }
      debugOut->println(message);
    }
#endif

    uint8_t numOfArgs = webserver.args();
    const char *strPtr;
    uint8_t i = 0;
    for (; (i < numOfArgs); i++ ) {
      // check field numbers
      if (webserver.argName(i)[0] == '1') {
        pfodESP8266Utils::strncpy_safe(storage.ssid, (webserver.arg(i)).c_str(), pfodESP8266Utils::MAX_SSID_LEN);
        pfodESP8266Utils::urldecode2(storage.ssid, storage.ssid); // result is always <= source so just copy over
      } else if (webserver.argName(i)[0] == '2') {
        pfodESP8266Utils::strncpy_safe(storage.password, (webserver.arg(i)).c_str(), pfodESP8266Utils::MAX_PASSWORD_LEN);
        pfodESP8266Utils::urldecode2(storage.password, storage.password); // result is always <= source so just copy over
        // if password all blanks make it empty
        if (pfodESP8266Utils::isEmpty(storage.password)) {
          storage.password[0] = '\0';
        }
      } else if (webserver.argName(i)[0] == '3') {
        pfodESP8266Utils::strncpy_safe(storage.staticIP, (webserver.arg(i)).c_str(), pfodESP8266Utils::MAX_STATICIP_LEN);
        pfodESP8266Utils::urldecode2(storage.staticIP, storage.staticIP); // result is always <= source so just copy over
        if (pfodESP8266Utils::isEmpty(storage.staticIP)) {
          // use dhcp
          storage.staticIP[0] = '\0';
        }
      } else if (webserver.argName(i)[0] == '4') {
        // convert portNo to uint16_6
        const char *portNoStr = (( webserver.arg(i)).c_str());
        long longPort = 0;
        pfodESP8266Utils::parseLong((byte*)portNoStr, &longPort);
        storage.portNo = (uint16_t)longPort;
      } else if (webserver.argName(i)[0] == '5') {
        // convert baud rate to int32_t
        const char *baudStr = (( webserver.arg(i)).c_str());
        long baud = 0;
        pfodESP8266Utils::parseLong((byte*)baudStr, &baud);
        storage.baudRate = (uint32_t)(baud);
      } else if (webserver.argName(i)[0] == '6') {
        // convert timeout to int32_t
        // then *1000 to make mS and store as uint32_t
        const char *timeoutStr = (( webserver.arg(i)).c_str());
        long timeoutSec = 0;
        pfodESP8266Utils::parseLong((byte*)timeoutStr, &timeoutSec);
        if (timeoutSec > 4294967) {
          timeoutSec = 4294967;
        }
        if (timeoutSec < 0) {
          timeoutSec = 0;
        }
        storage.timeout = (uint32_t)(timeoutSec * 1000);
      }
    }

    uint8_t * byteStorage = (uint8_t *)&storage;
    for (size_t i = 0; i < STORAGE_SIZE; i++) {
      EEPROM.write(startAddress + i, byteStorage[i]);
    }
    delay(0);
    EEPROM.commit();
  } // else if no args just return current settings

  delay(0);
  STORAGE storageRead;
  uint8_t * byteStorageRead = (uint8_t *)&storageRead;
  for (size_t i = 0; i < STORAGE_SIZE; i++) {
    byteStorageRead[i] = EEPROM.read(startAddress + i);
  }

  String rtnMsg = "<html>"
                  "<head>"
                  "<title>pfodWifiWebConfig Server Setup</title>"
                  "<meta charset=\"utf-8\" />"
                  "<meta name=viewport content=\"width=device-width, initial-scale=1\">"
                  "</head>"
                  "<body>"
                  "<h2>pfodWifiWebConfig Server Settings saved.</h2><br>Power cycle to connect to ";
  if (storageRead.password[0] == '\0') {
    rtnMsg += "the open network ";
  }
  rtnMsg += "<b>";
  rtnMsg += storageRead.ssid;
  rtnMsg += "</b>";

  if (storageRead.staticIP[0] == '\0') {
    rtnMsg += "<br> using DCHP to get its IP address";
  } else { // staticIP
    rtnMsg += "<br> using IP addess ";
    rtnMsg += "<b>";
    rtnMsg += storageRead.staticIP;
    rtnMsg += "</b>";
  }
  rtnMsg += "<br><br>Will start a server listening on port ";
  rtnMsg += storageRead.portNo;
  if (mode == WIFI_SHIELD_MODE) {
    rtnMsg += ".<br> with connection timeout of ";
    rtnMsg += storageRead.timeout / 1000;
    rtnMsg += " seconds.";
    rtnMsg += "<br><br>Serial baud rate set to ";
    rtnMsg += storageRead.baudRate;
  }
  rtnMsg += "</body>"
            "</html>";

  webserver.send ( 200, "text/html", rtnMsg );
  delay(1000); // wait a moment to send response
  configHandlerCalled = true;
}


void pfodESP8266WebConfig::rootHandler() {
  // got request for root
  // stop flash and make light solid
  rootHandlerCalled = true;
#ifdef DEBUG
  if (debugOut) {
    debugOut->print("rootHandler mode:");
    debugOut->println(mode);
  }
#endif
  String msg;
  msg = "<html>"
        "<head>"
        "<title>pfodWifiWebConfig Server Setup</title>"
        "<meta charset=\"utf-8\" />"
        "<meta name=viewport content=\"width=device-width, initial-scale=1\">"
        "</head>"
        "<body>"
        "<h2>pfodWifiWebConfig Server Setup</h2>"
        "<p>Use this form to configure your device to connect to your Wifi network and start as a Server listening on the specified port.</p>"
        "<form class=\"form\" method=\"post\" action=\"/config\" >"
        "<p class=\"name\">"
        "<label for=\"name\">Network SSID</label><br>"
        "<input type=\"text\" name=\"1\" id=\"ssid\" placeholder=\"wifi network name\"  required "; // field 1

  if (*aps != '\0') {
    msg += " value=\"";
    msg += aps;
    msg += "\" ";
  }
  msg += " />"
         "<p class=\"password\">"
         "<label for=\"password\">Password for WEP/WPA/WPA2 (enter a space if there is no password, i.e. OPEN)</label><br>"
         "<input type=\"text\" name=\"2\" id=\"password\" placeholder=\"wifi network password\" autocomplete=\"off\" required " // field 2
         "</p>"
         "<p class=\"static_ip\">"
         "<label for=\"static_ip\">Set the Static IP for this device</label><br>"
         "(If this field is empty, DHCP will be used to get an IP address)<br>"
         "<input type=\"text\" name=\"3\" id=\"static_ip\" placeholder=\"192.168.4.99\" "  // field 3
         " pattern=\"\\b(?:(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\\.){3}(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\\b\"/>"
         "</p>"
         "<p class=\"portNo\">"
         "<label for=\"portNo\">Set the port number that the Server will listen on for connections.</label><br>"
         "<input type=\"text\" name=\"4\" id=\"portNo\" placeholder=\"80\" required"  // field 4
         " pattern=\"\\b([0-9]{1,4}|[1-5][0-9]{4}|6[0-4][0-9]{3}|65[0-4][0-9]{2}|655[0-2][0-9]|6553[0-5])\\b\" />"
         "</p>";
  if (mode == WIFI_SHIELD_MODE) {
    msg += "<p class=\"baud\">"
           "<label for=\"baud\">Serial Baud Rate (limit to 19200 for Uno and Mega)</label><br>"
           "<select name=\"5\" id=\"baud\" required>" // field 5
           "<option value=\"9600\">9600</option>"
           "<option value=\"14400\">14400</option>"
           "<option selected value=\"19200\">19200</option>"
           "<option value=\"28800\">28800</option>"
           "<option value=\"38400\">38400</option>"
           "<option value=\"57600\">57600</option>"
           "<option value=\"76800\">76800</option>"
           "<option value=\"115200\">115200</option>"
           "</select>"
           "</p>"
           "<p class=\"timeout\">"
           "<label for=\"timeout\">Set the server connection timeout in seconds, 0 for never timeout (not recommended).</label><br>"
           "<input type=\"text\" name=\"6\" id=\"timeout\" required"  // field 6
           " value=\"";
    msg += DEFAULT_CONNECTION_TIMEOUT_SEC;
    msg += "\""
           " pattern=\"\\b([0-9]{1,7})\\b\" />"
           "</p>";
  }
  msg +=   "<p class=\"submit\">"
           "<input type=\"submit\" value=\"Configure\"  />"
           "</p>"
           "</form>"
           "</body>"
           "</html>";

  webserver.send ( 200, "text/html", msg );
}





