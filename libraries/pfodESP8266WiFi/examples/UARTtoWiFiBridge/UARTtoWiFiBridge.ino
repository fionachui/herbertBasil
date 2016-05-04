/*
  UARTtoWiFiBridge
  nonBlocking modification to WiFiTelnetToSerial - Example Transparent UART to Telnet Server for esp8266
  Modified by Matthew Ford
  Mods Copyright (c) 2015 Forward Computing and Control Pty. Ltd. All rights reserved.
  License condition unchanged.

 The ESP8266 can only handle sending one TCP packet (of 1460 bytes of data) at a time.
 This code and the associated pfodESP8266WiFi non-blocking library allows incoming Serial data to be read and buffered while the ESP is handling the
 previous packet, waiting for it to be ACKed.  This allows data to be streamed at higher baud rates.
 
 Window's OS takes about 200mS to ACK a single TCP packet. Since the ESP8266 blocks handling more outgoing data until the last packet is ACKed,
 so, even with this non-blocking code and library, for continual data transmission you should limit the Serial baud rate to <= 57600 baud to avoid loss of data.

 For linux and Android, the ACKs come back much faster, 10-40mS, so when connecting from these OS's you can send data continually at 115200 baud.

 If the WiFi connection is poor and there are re-transmissions of lost packets, it takes longer for the ESP to successfully send the packed and
 you will need to set a lower baud rate to avoid lost data.
 This code could be extended to have multiple input buffers to handle higher burst rates of data. This is left as an exercise for the reader.

 This code also has a connection idle timeout.  If there is no incoming data from the connected client, this code will close the connection after
 CONNECTION_TIMEOUT mS (default 15 secs).  The prevents half-closed connections from locking out new connections.  If you set
 CONNECTION_TIMEOUT to 0 the connection will never time out, not recommended.

 This code also batches outgoing data by delaying sending of the buffered data until either the buffer is full, a full TCP packet, or no data has
 been received from the Serial port for SEND_DELAY_TIME (default 10mS).  This improves through put and allows higher baud rates without data loss.

  Copyright (c) 2015 Hristo Gochkov. All rights reserved.
  This file is part of the ESP8266WiFi library for Arduino environment.

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/
#include "pfodESP8266WiFi.h"

// uncomment next line to get connection/disconnection msgs output to Serial link
//#define CONNECTION_MESSAGES

// uncomment next line to get debug messages output to Serial link
//#define DEBUG

const int SERIAL_BAUD_RATE = 19200; // see notes at top regarding this board rate, use <=57600 if Window's clients are connecting.
const char* ssid = "**********";
const char* password = "**********";
const char* staticIP = ""; // set this to the static IP you want, if left empty DHCP will be used.

// connection timeout in mS default is 15 sec.  This is here to overcome "half closed" issues, set to 0 for never timeout, NOT recommended
// connection will close if no incoming data for this time.
const unsigned long CONNECTION_TIMEOUT = 15000;
const int portNo = 23;  // set the port no to accept connections on

WiFiServer server(portNo);
WiFiClient client;

void setup() {
  Serial.begin(SERIAL_BAUD_RATE);
#ifdef DEBUG
  Serial.println(F("Connecting to AP"));
  Serial.print("ssid '");
  Serial.print(ssid);
  Serial.println("'");
  Serial.print("password '");
  Serial.print(password);
  Serial.println("'");
#endif
  // do this before begin to set static IP and not enable dhcp
  if (*staticIP != '\0') {
    // config static IP
    IPAddress ip(ipStrToNum(staticIP));
    IPAddress gateway(ip[0], ip[1], ip[2], 1); // set gatway to ... 1
#ifdef DEBUG
    Serial.print(F("Setting gateway to: "));
    Serial.println(gateway);
#endif
    IPAddress subnet(255, 255, 255, 0);
    WiFi.config(ip, gateway, subnet);
  } // else leave as DHCP

  WiFi.begin(ssid, password);
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
  Serial.println(portNo);
  Serial.println(F("Listening for connections..."));
#endif

  client = server.available();
#ifdef DEBUG
  Serial.print("+++"); // end of setup start listening now
#endif
}


unsigned long timeoutTimerStart = 0;
const unsigned long SEND_DELAY_TIME = 10; // 10mS delay before sending buffer
static const size_t SEND_BUFFER_SIZE = 1460; // Max data size for standard TCP/IP packet
static uint8_t sendBuffer[SEND_BUFFER_SIZE]; //
size_t sendBufferIdx = 0;
unsigned long sendTimerStart = 0;

bool alreadyConnected = false;
// the loop routine runs over and over again forever:
void loop() {
  if (!client) { // see if a client is available
    client = server.available(); // evaluates to false if no connection
  } else {
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
        alreadyConnected = true;
        sendBufferIdx = 0;
        // start timer
        timeoutTimerStart = millis();
      }
    }
  }

  //check UART for data
  if (Serial.available()) {
    size_t len = Serial.available();
    if (len > 0) { // size_t is an unsigned type so >0 is actually redundent
      sendTimerStart = millis();
      size_t will_copy = (len < (SEND_BUFFER_SIZE - sendBufferIdx)) ? len : (SEND_BUFFER_SIZE - sendBufferIdx);
      Serial.readBytes(sendBuffer + sendBufferIdx, will_copy);
      sendBufferIdx += will_copy;
      //push UART data to connected client, if buffer full
      if (sendBufferIdx == SEND_BUFFER_SIZE) {
        // buffer full
        if (client && client.connected()) {
          client.write((const uint8_t *)sendBuffer, sendBufferIdx); // this will only block if ESP is still waiting for ACK for last packet
          // client.flush();  // don't flush does not affect write but does clear inbound buffer :-(
        } else {
          // just throw this data away
        }
        sendBufferIdx = 0;
      }
    }
    delay(0); // yield
  }



  if ((sendBufferIdx > 0) && ((millis() - sendTimerStart) > SEND_DELAY_TIME)) {
    if (client && client.connected()) {
      if (client.isSendWaiting()) {
        // skip this write and keep adding data to buffer
      } else {
        // ESP not busy and this call will not block
        client.write((const uint8_t *)sendBuffer, sendBufferIdx);
        // client.flush();  // don't flush does not affect write but does clear inbound buffer :-(
        sendBufferIdx = 0;
      }
    } else {
      // just throw this data away
      sendBufferIdx = 0;
    }
    delay(0); // yield
  }

  if (client && client.connected()) {
    while ((client.available() > 0) &&  (Serial.availableForWrite() > 0) && client.connected()) {
      // use Serial.availableForWrite to prevent loosing incoming data
      timeoutTimerStart = millis();  // reset timer if we have incoming data
      Serial.write(client.read());
      delay(0); // yield
    }
  }

  // see if we should drop the connection
  if (alreadyConnected && (CONNECTION_TIMEOUT > 0) && ((millis() - timeoutTimerStart) > CONNECTION_TIMEOUT)) {
    closeConnection();
  }
}



void closeConnection() {
#ifdef CONNECTION_MESSAGES
  Serial.println("ConnectionClosed");
#endif
  alreadyConnected = false;
  WiFiClient::stopAll(); // stop all clients and release memory.
  client = server.available(); // evaluates to false if no connection
}


/**
 * ipStrToNum
 * works for IPV4 only
 * return uint32_t for passing to ipAddress( );
 */
uint32_t ipStrToNum(const char* ipStr) {
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
    if (*ipStr == '.') {
      // store num and move on to next position
      _address.bytes[i++] = num;
      num = 0;
      if (i >= SIZE_OF_NUMS) {
        break; // filled array
      }
    } else {
      num = (num << 3) + (num << 1); // *10 = *8+*2
      num = num +  (*ipStr - '0');
    }
    ipStr++;
  }
  if (i < SIZE_OF_NUMS) {
    // store last num
    _address.bytes[i++] = num;
  }
  return _address.dword;
}
