/*
  Rev 13   of An ESP8266 Cheap and Simple Wifi Shield for Arduino and other micros
  Load this sketch to the ESP8266-01 module or the Adafruit HAZZAH ESP8266 module.

 The ESP8266 can only handle sending one TCP packet (of 1460 bytes of data) at a time.
 This code and the associated pfodESP8266WiFi non-blocking library allows incoming Serial data to be read and buffered while the ESP is handling the
 previous packet, waiting for it to be ACKed.  This allows data to be streamed at higher baud rates.

 Window's OS takes about 200mS to ACK a single TCP packet. Since the ESP8266 blocks handling more outgoing data until the last packet is ACKed,
 so, even with this non-blocking code and library, for continual data transmission you should limit the Serial baud rate to <= 57600 baud to avoid loss of data.

 For linux and Android, the ACKs come back much faster, 10-40mS, so when connecting from these OS's you can send data continually at 115200 baud.

 If the WiFi connection is poor and there are re-transmissions of lost packets, it takes longer for the ESP to successfully send the packed and
 you will need to set a lower baud rate to avoid lost data.
 19200 is the maximum practical baud rate for communicating with Arduino UNO and Arduino Mega boards

 This code could be extended to have multiple input buffers to handle higher burst rates of data. This is left as an exercise for the reader.

 This code also has a connection idle timeout.  If there is no incoming data from the connected client, this code will close the connection after
 the connection timeout time set in webpage config (default 15 secs).  The prevents half-closed connections from locking out new connections.  If you set
 connection timeout to 0 the connection will never time out, not recommended.

 This code also batches outgoing data by delaying sending of the buffered data until either the buffer is full, a full TCP packet, or no data has
 been received from the Serial port for SEND_DELAY_TIME (10mS).  This improves through put and allows higher baud rates without data loss.

  Rev 13 fix compile failure introduced by Arduino 1.6.6
  Rev 12 minor changes to handling new clients
  Rev 11 handles and closes other connections if already connected, set timeout as set from config was always 0 in rev10
  Rev 10 adds non-blocking library, pfodESP8266WiFi, and added baud rate setting to web config page
  Rev 8 removes 200mS wait after client.write()  client.write() is a blocking call.
  Rev 7 copies one byte at a time to Serial tx. Avoids calling client.flush() and waits 200mS after calling client.write()
  Rev 6 checked tx available
  Rev 5 added WiFiClient.stopAll() to clean up memory on disconnect
  Rev 4 added option connection disconnection messsages use #define CONNECTION_MESSAGES to enable them
  Rev 3 added connection timeout
  Rev 4 change tcpNoDelay to true clients and only monitors incoming data to reset connection timer.

  Load the Arduino with an empty sketch,
  connect USB to Serial TX to Flash TX
  connect USB to Serial RX to Flash RX
  short out the Flash Link and apply power to get into flash mode

  To Config
  Short the config link and apply power
    and connect to the pfodWifiWebConfig access point
  using the password set in  #define pfodWifiWebConfigPASSWORD  (below)
  Scan this password from the QR code generated with http://www.forward.com.au/pfod/secureChallengeResponse/keyGenerator/index.html

  Once you connect to the Access Point open the webpage
  http://10.1.1.1 to setup your network parameters
  ssid, password and either DHCP or staticIP and portNo.
  Then when you reboot (with the config link removed) you can connect to the ip:portNo and pass data via the UART in both directions.

  The micro board that is connected to this module should have setup() code stating with

  void setup() {
   delay(1000);  // skip skip the ESP8266 debug output
   Serial.begin(19200); // or whatever baud rate you configured via the webpage

    .... rest of setup here
     ..


 see http://www.forward.com.au/pfod/pfodWifiConfig/ESP8266/pfodWifiConfig_ESP8266.html for details
 For an example QR code image look in the directory this file is in.
 */

/**
 *  Cheap and Simple Wifi Shield for Arduino and other micros
 * http://www.forward.com.au/pfod/CheapWifiSheild/index.html
 *
 * (c)2015 Forward Computing and Control Pty. Ltd.
 * This code may be freely used for both private and commerical use.
 * Provide this copyright is maintained.
 */

#include <EEPROM.h>
#include <pfodESP8266Utils.h>
#include <pfodESP8266WiFi.h>
#include <pfodWiFiClient.h>
#include <pfodESP8266WebServer.h>
#include <pfodESP8266BufferedClient.h>

//---------- Arduin0 1.6.6+ needs these declarations -------------
void handleNotFound();
void handleRoot();
void handleConfig();
void setupAP(const char* ssid_wifi, const char* password_wifi);
void closeConnection();


// normally DEBUG is commented out
//#define DEBUG
// Initial baud rate for DEBUG is 115200,
// If not starting in web config mode the baud rate is switched to the configured baud.

// uncomment this to connect / disconnect messages on Serial out.
//#define CONNECTION_MESSAGES

// the default to show in the config web page
const int DEFAULT_CONNECTION_TIMEOUT_SEC = 15;
WiFiServer server(80);
WiFiClient client; // just one client reused
pfodESP8266BufferedClient bufferedClient;

// =============== start of pfodWifiWebConfig settings ==============
// update this define with the password from your QR code
//  http://www.forward.com.au/pfod/secureChallengeResponse/keyGenerator/index.html
#define pfodWifiWebConfigPASSWORD "GoodPlantations"
#define pfodWifiWebConfigAP "MakingWaves"

// note pfodSecurity uses 19 bytes of eeprom usually starting from 0 so
// start the eeprom address from 20 for configureWifiConfig
const uint8_t webConfigEEPROMStartAddress = 20;

int wifiSetup_pin = 2; // name the input pin for setup mode detection GPIO2 on most ESP8266 boards
// =============== end of pfodWifiWebConfig settings ==============

// On ESP8266-01 and Adafruit HAZZAH ESP8266, connect LED + 270ohm resistor from D2 (GPIO2) to +3V3 to indicate when in config mode

// set the EEPROM structure
struct EEPROM_storage {
  uint32_t timeout;
  uint32_t baudRate;
  uint16_t portNo;
  char ssid[pfodESP8266Utils::MAX_SSID_LEN + 1]; // WIFI ssid + null
  char password[pfodESP8266Utils::MAX_PASSWORD_LEN + 1]; // WiFi password,  if empyt use OPEN, else use AUTO (WEP/WPA/WPA2) + null
  char staticIP[pfodESP8266Utils::MAX_STATICIP_LEN + 1]; // staticIP, if empty use DHCP + null
} storage;
const int EEPROM_storageSize = sizeof(EEPROM_storage);

char ssid_found[pfodESP8266Utils::MAX_SSID_LEN + 1]; // max 32 chars + null
ESP8266WebServer webserver(80);  // this just sets portNo nothing else happens until begin() is called

byte inConfigMode = 0; // false
uint32_t timeout = 0;
const char *aps;

void setup ( void ) {
  WiFi.mode(WIFI_STA);
  inConfigMode = 0; // non in config mode
  EEPROM.begin(512); // must be greater than (wifiConfigStartAddress + EEPROM_storageSize)
  delay(10);
  pinMode(0, OUTPUT);
  digitalWrite(0, LOW); // make GPIO0 low after ESP8266 has initialized

  for (int i = 4; i > 0; i--) {
    delay(500);
  }

#ifdef DEBUG
  // initial baud rate in DEBUG is 115200
  Serial.begin(115200);
  Serial.println();
  Serial.println(F("Starting Setup"));
  //  bufferedClient.setDebugStream(&Serial);  // add this line if using DEBUG in pfodESP8266BufferedClient library code
#endif

  //============ pfodWifiConfigV1 config in Access Point mode ====================
  // see if config button is pressed
  if (digitalRead(wifiSetup_pin) == LOW) {
    inConfigMode = 1; // in config mode
    WiFi.mode(WIFI_AP_STA);
#ifdef DEBUG
    Serial.println(F("Setting up Access Point for pfodWifiWebConfig"));
#endif
    // connect to temporary wifi network for setup
    // the features determine the format of the {set...} command
    setupAP(pfodWifiWebConfigAP, pfodWifiWebConfigPASSWORD);
    //   Need to reboot afterwards
    return; // skip rest of setup();
  }
  //============ end pfodWifiConfigV1 config ====================

  // else button was not pressed continue to load the stored network settings
  //else use configured setttings from EEPROM
  uint8_t * byteStorageRead = (uint8_t *)&storage;
  for (size_t i = 0; i < EEPROM_storageSize; i++) {
    byteStorageRead[i] = EEPROM.read(webConfigEEPROMStartAddress + i);
  }
  
  timeout = storage.timeout; // set the timeout
  
  // Initialise wifi module
#ifdef DEBUG
  Serial.println();
  Serial.println(F("Connecting to AP"));
  Serial.print("ssid '");
  Serial.print(storage.ssid);
  Serial.println("'");
  Serial.print("password '");
  Serial.print(storage.password);
  Serial.println("'");
#endif
  server = WiFiServer(storage.portNo);

  // do this before begin to set static IP and not enable dhcp
  if (*storage.staticIP != '\0') {
    // config static IP
    IPAddress ip(pfodESP8266Utils::ipStrToNum(storage.staticIP));
    IPAddress gateway(ip[0], ip[1], ip[2], 1); // set gatway to ... 1
#ifdef DEBUG
    Serial.print(F("Setting gateway to: "));
    Serial.println(gateway);
#endif
    IPAddress subnet(255, 255, 255, 0);
    WiFi.config(ip, gateway, subnet);
  } // else leave as DHCP

  WiFi.begin(storage.ssid, storage.password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
#ifdef DEBUG
    Serial.print(".");
#endif
  }
#ifdef DEBUG
  Serial.println();
  Serial.println(F("Connected!"));
#endif

  // Start listening for connections
#ifdef DEBUG
  Serial.println(F("Start Server"));
#endif
  server.begin();
  server.setNoDelay(true); // does not do much if anything
#ifdef DEBUG
  Serial.println(F("Server Started"));
  // Print the IP address
  Serial.print(WiFi.localIP());
  Serial.print(':');
  Serial.println(storage.portNo);
  Serial.println(F("Listening for connections..."));
#endif

#ifdef DEBUG
  Serial.println("End of Setup()"); // end of setup start listening now
  Serial.print(F("Restarting Serial with configured baud rate:"));
  Serial.println(storage.baudRate);
  delay(100);
#endif
  // restart Serial with configured baudrate
  Serial.end();
  delay(10);
  Serial.begin(storage.baudRate);
  delay(10);
}


unsigned long timeoutTimerStart = 0;
const unsigned long SEND_DELAY_TIME = 10; // 10mS delay before sending buffer
unsigned long sendTimerStart = 0;
static const size_t SEND_BUFFER_SIZE = 1460; // Max data size for standard TCP/IP packet
static uint8_t sendBuffer[SEND_BUFFER_SIZE]; //
size_t sendBufferIdx = 0;

bool alreadyConnected = false;
// the loop routine runs over and over again forever:
void loop() {
  if (inConfigMode) {
    webserver.handleClient();
    delay(0);
    return;
  }
  if (server.hasClient()) {   // avoid creating WiFiClient object if no connection,  ESP8266 specific
    WiFiClient anotherClient = server.available();
    if (!client) {
      client = anotherClient; // take this connection
    } else {
#ifdef DEBUG
  Serial.println("Stop extra connection");
#endif        
      anotherClient.stop(); // stop this extra connection
    }
  }  // anotherClient is disposed here at end of its block
  if (client) { 
    // have client
    if (!client.connected()) {
      if (alreadyConnected) {
        // client closed so clean up
        closeConnection();
      }
    } else {
      // have connected client
      if (!alreadyConnected) {
#ifdef CONNECTION_MESSAGES
        Serial.println("ConnectionOpened");
#endif
        client.setNoDelay(true); // does not do much if anything
        bufferedClient.connect(&client); // buffer this client
        alreadyConnected = true;
        // start timer
        timeoutTimerStart = millis();
      }
    }
  }

  //check UART for data
  if (Serial.available()) {
    size_t len = Serial.available();
    if (len > 0) { // size_t is an unsigned type so >0 is actually redundent
      for (size_t i = 0; i < len; i++) {
        bufferedClient.write(Serial.read());
      }
    }
  }

  // NOTE: MUST call some bufferedClient method each loop to push out buffered data on delayed send timeout
  if (bufferedClient.connected()) {
    while ((bufferedClient.available() > 0) &&  (Serial.availableForWrite() > 0)) {
      // use Serial.availableForWrite to prevent loosing incoming data
      timeoutTimerStart = millis();  // reset timer if we have incoming data
      Serial.write(bufferedClient.read());
    }
  }

  // see if we should drop the connection
  if (alreadyConnected && (timeout > 0) && ((millis() - timeoutTimerStart) > timeout)) {
    closeConnection();
  }
}


void closeConnection() {
#ifdef CONNECTION_MESSAGES
  Serial.println("ConnectionClosed");
#endif
  alreadyConnected = false;
  bufferedClient.stop(); // clears client reference
  client.stop(); // stop client and tcp buffer.
  if (server.hasClient()) {   // avoid creating WiFiClient object if no connection,  ESP8266 specific
    client = server.available();
  } // else just keep client that was just stopped will evaluate to false
}


void setupAP(const char* ssid_wifi, const char* password_wifi) {
  aps = pfodESP8266Utils::scanForStrongestAP((char*)&ssid_found, pfodESP8266Utils::MAX_SSID_LEN + 1);
  delay(0);
  IPAddress local_ip = IPAddress(10, 1, 1, 1);
  IPAddress gateway_ip = IPAddress(10, 1, 1, 1);
  IPAddress subnet_ip = IPAddress(255, 255, 255, 0);
#ifdef DEBUG
  Serial.println(F("configure pfodWifiWebConfig"));
#endif
  WiFi.softAP(ssid_wifi, password_wifi);

#ifdef DEBUG
  Serial.println(F("Access Point setup"));
#endif
  WiFi.softAPConfig(local_ip, gateway_ip, subnet_ip);

#ifdef DEBUG
  Serial.println("done");
  IPAddress myIP = WiFi.softAPIP();
  Serial.print(F("AP IP address: "));
  Serial.println(myIP);
#endif
  delay(10);
  webserver.on ( "/", handleRoot );
  webserver.on ( "/config", handleConfig );
  webserver.onNotFound ( handleNotFound );
  webserver.begin();
#ifdef DEBUG
  Serial.println ( "HTTP webserver started" );
#endif
}

void handleConfig() {
  // set defaults
  uint16_t portNo = 80;
  uint32_t timeout = DEFAULT_CONNECTION_TIMEOUT_SEC * 1000; // time out in 15 sec
  uint32_t baudRate = 19200;

  if (webserver.args() > 0) {
#ifdef DEBUG
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
    Serial.println(message);
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
    for (size_t i = 0; i < EEPROM_storageSize; i++) {
      EEPROM.write(webConfigEEPROMStartAddress + i, byteStorage[i]);
    }
    delay(0);
    EEPROM.commit();
  } // else if no args just return current settings

  delay(0);
  struct EEPROM_storage storageRead;
  uint8_t * byteStorageRead = (uint8_t *)&storageRead;
  for (size_t i = 0; i < EEPROM_storageSize; i++) {
    byteStorageRead[i] = EEPROM.read(webConfigEEPROMStartAddress + i);
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
  rtnMsg += ".<br> with connection timeout of ";
  rtnMsg += storageRead.timeout / 1000;
  rtnMsg += " seconds.";
  rtnMsg += "<br><br>Serial baud rate set to ";
  rtnMsg += storageRead.baudRate;
  "</body>"
  "</html>";

  webserver.send ( 200, "text/html", rtnMsg );
}


void handleRoot() {
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
         "</p>"
         "<p class=\"baud\">"
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
  msg +=   DEFAULT_CONNECTION_TIMEOUT_SEC;
  msg +=  "\""
          " pattern=\"\\b([0-9]{1,7})\\b\" />"
          "</p>"
          "<p class=\"submit\">"
          "<input type=\"submit\" value=\"Configure\"  />"
          "</p>"
          "</form>"
          "</body>"
          "</html>";

  webserver.send ( 200, "text/html", msg );
}


void handleNotFound() {
  handleRoot();
}






