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
const char *open_state = "open";
const char *closed_state = "closed";

const int BAUD_RATE   = 115200;                       // serial baud rate

// MQTT stuff
WiFiClient espClient;
PubSubClient client(espClient);
const char* mqttDebugTopic = "bitlair/debug";
const char* mqttTopic = "bitlair/switch/state"; // post /bitlair, /djo (open or closed as value)
long lastMsg = 0;
char msg[50];
int value = 0;

void setup() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  for (uint8_t i = 0; i < numStates; i++) {
    pinMode(inputPins[i], INPUT);
    pinMode(LEDPins[i], OUTPUT);
    digitalWrite(LEDPins[i], !digitalRead(inputPins[i]));
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
  for (int i = 0; i < numStates; i++) {
    recentReading[i] = digitalRead(inputPins[i]);
    if (recentReading[i] != lastReading[i]) {
      // A switch is toggled
      char newState[50] = {0}; // MQTT topic to push to
      char newStateTopic[50] = {0}; // MQTT topic to push to
      sprintf(newStateTopic, "%s/%s", mqttTopic, stateValues[i]);

      if (!recentReading[i]) {
        // OPEN
        strcpy(newState, open_state);
      } else {
        // CLOSED
        strcpy(newState, closed_state);
      }

      Serial.print(newStateTopic);
      Serial.print(": ");
      Serial.println(newState);
    
      if (client.publish(newStateTopic, newState, true)) {
        Serial.println("MQTT publish succesful!");
        lastReading[i] = recentReading[i];
        digitalWrite(LEDPins[i], !recentReading[i]);
        delay(1000);
      } else {
        Serial.println("MQTT publish unsuccesful! Retrying later.");
        delay(5000);
      }
      client.subscribe(mqttTopic);
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
