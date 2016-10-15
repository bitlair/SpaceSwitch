/*
  ESP Spacestate switch for bitlair
*/
#include <limits.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <PubSubClient.h>

const char* projectName = "SpaceSwitch";

// WiFi settings
const char ssid[] = "DJOAMERSFOORT";                  //  your network SSID (name)
const char pass[] = "l4anp4r7y";                      // your network password
const char* mqtt_server = "mqtt.bitlair.nl";

#define numStates 2 // bitlair and DJO states
const uint8_t inputPins[numStates] = {D1, D2}; // add pullup resistors
boolean lastReading[numStates] = {HIGH, HIGH};
const uint8_t LEDPins[numStates] = {D5, D6};
const char *stateValues[numStates] = {"bitlair", "djo"};
const char *default_state = "closed";

const int BAUD_RATE   = 115200;                       // serial baud rate

// MQTT stuff
WiFiClient espClient;
PubSubClient client(espClient);
const char* mqttDebugTopic = "bitlair/debug";
const char* mqttTopic = "bitlair/switch/state"; // post closed/bitlair/djo
long lastMsg = 0;
char msg[50];
int value = 0;

void setup() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  for (uint8_t i = 0; i < numStates; i++) {
    pinMode(inputPins[i], INPUT);
    pinMode(LEDPins[i], OUTPUT);
    digitalWrite(LEDPins[i], LOW);
  }

  Serial.begin(BAUD_RATE);
  Serial.println();
  Serial.println(projectName);

  // We start by connecting to a WiFi network
  Serial.print("Connecting to ");

  Serial.println(ssid);
  WiFi.begin(ssid, pass);

  while (WiFi.status() != WL_CONNECTED) {
    delay(50);
    Serial.print(".");
  }
  Serial.println("");

  Serial.print("WiFi connected to: ");
  Serial.println(ssid);

  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.println();

  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect("ESP8266Client")) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      snprintf (msg, 75, "%s (re)connect #%ld", projectName, value);
      Serial.print("Publish message: ");
      Serial.println(msg);
      client.publish(mqttDebugTopic, msg);
      ++value;
      // ... and resubscribe
      client.subscribe(mqttTopic);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void handleSwitches() {
  boolean recentReading[numStates];
  boolean isChange = false;
  boolean isClosed = true;
  for (int i = 0; i < numStates; i++) {
    recentReading[i] = digitalRead(inputPins[i]);
    if (recentReading[i] != lastReading[i])
      isChange = true;
  }
  if (isChange) {
    char newState[50] = {0};
    for (int i = 0; i < numStates; i++) {
      digitalWrite(LEDPins[i], LOW);
      if (!recentReading[i])
        isClosed = false;
    }
    if (isClosed) {
      strcpy(newState, default_state);
    } else {
      for (int i = 0; i < numStates; i++) {
        if (!recentReading[i]) {
          digitalWrite(LEDPins[i], HIGH);
          strcpy(newState, stateValues[i]);
          break;
        }
      }
    }

    Serial.print("New state: ");
    Serial.println(newState);
    if (client.publish(mqttTopic, newState, true)) {
      Serial.println("MQTT publish succesful!");
      for (int i = 0; i < numStates; i++) {
        lastReading[i] = recentReading[i];
      }
    } else {
      Serial.println("MQTT publish unsuccesful! Retrying later.");
    }
    client.subscribe(mqttTopic);
  }


  for (int i = 0; i < numStates; i++) {
    if (recentReading[i] != lastReading[i]) {
      isChange = true;
    }
  }

  delay(1000);
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  handleSwitches();
  client.loop();
  yield();
}