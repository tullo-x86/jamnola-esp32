/*
 * ESP32-POE controller for JamNola
 * Board type: [OLIMEX ESP32-POE]
 * Dependencies: 
 *   - "OSC" by Adrian Freed (Open Sound Control). **Search for "open sound control". This library will not be found when searching "OSC" in the Arduino IDE**
 */

#ifndef ETH_PHY_TYPE
#define ETH_PHY_TYPE  ETH_PHY_LAN8720
#define ETH_PHY_ADDR  0
#define ETH_PHY_MDC   23
#define ETH_PHY_MDIO  18
#define ETH_PHY_POWER -1
#define ETH_CLK_MODE  ETH_CLOCK_GPIO0_IN
#endif

#include <ETH.h>
#include <WiFiUdp.h>
#include <OSCMessage.h>

static bool eth_connected = false;

WiFiUDP udp;                                // A UDP instance to let us send and receive packets over UDP
const IPAddress remoteIp(192,168,1,165);    // remote IP of your computer
const unsigned int remotePort = 9999;       // remote port to receive OSC
const unsigned int localPort = 8888;        // local port to listen for OSC packets (actually not used for sending)

//Setup 7 Inputs for 8 Interactions - Rotary Switch

// WARNING: onEvent is called from a separate FreeRTOS task (thread)!
void onEvent(arduino_event_id_t event) {
  switch (event) {
    case ARDUINO_EVENT_ETH_START:
      Serial.println("ETH Started");
      // The hostname must be set after the interface is started, but needs
      // to be set before DHCP, so set it from the event handler thread.
      ETH.setHostname("esp32-ethernet");
      break;
    case ARDUINO_EVENT_ETH_CONNECTED: Serial.println("ETH Connected"); break;
    case ARDUINO_EVENT_ETH_GOT_IP:
      Serial.println("ETH Got IP");
      Serial.println(ETH);
      eth_connected = true;
      break;
    case ARDUINO_EVENT_ETH_LOST_IP:
      Serial.println("ETH Lost IP");
      eth_connected = false;
      break;
    case ARDUINO_EVENT_ETH_DISCONNECTED:
      Serial.println("ETH Disconnected");
      eth_connected = false;
      break;
    case ARDUINO_EVENT_ETH_STOP:
      Serial.println("ETH Stopped");
      eth_connected = false;
      break;
    default: break;
  }
}

void sendMsg(int32_t rotaryPosition) {
  OSCMessage message("/rotary");
  message.add(rotaryPosition);
  udp.beginPacket(remoteIp, remotePort);
  message.send(udp);
  udp.endPacket();
  message.empty();
}

// The rotary switch we are using is an EIGHT position switch, with the eighth position having all 7 lines open-circuit.
// The lines correspond to what we will logically call positions 0, 1, 2, 3, 4, 5, and 6. Open-circuit corresponds to position 7.
const int32_t positionLineCount = 7;
uint8_t positionLinePins[positionLineCount] = { 13, 14, 15, 16, 32, 33, 34 };
bool lastLineValues[positionLineCount] = { false, false, false, false, false, false, false };
bool currLineValues[positionLineCount] = { false, false, false, false, false, false, false };

void setup() {
  Serial.begin(115200);
  Network.onEvent(onEvent);
  ETH.begin();

  pinMode(13, INPUT); // external 2k2 pullup
  pinMode(14, INPUT_PULLUP);
  pinMode(15, INPUT_PULLUP);
  pinMode(16, INPUT); // external 2k2 pullup
  pinMode(32, INPUT); // external 10k pullup
  pinMode(33, INPUT_PULLUP);
  pinMode(34, INPUT_PULLUP);  
}

void loop() {
  // Fetch all the pins
  bool isAnyChange = false;
  int32_t numPinsActive = 0;
  int32_t knownSwitchPosition = 7;
  for (int32_t i = 0; i < positionLineCount; i++) {
    bool thisPin = (digitalRead(positionLinePins[i]) == LOW);
    lastLineValues[i] = currLineValues;
    currLineValues[i] = thisPin;
    
    if (thisPin) {
      numPinsActive += 1;
      knownSwitchPosition = i;
    }

    if (lastLineValues[i] != currLineValues[i]) {
      isAnyChange = true;
    }
  }

  // If there's been a change and any valid number of pins is being pulled low, send a message
  if (isAnyChange && numPinsActive < 2) {
    sendMsg(knownSwitchPosition);
  }

  delay(10);
}

