#include <FS.h>
#include <ArduinoJson.h>
#include "mqtt_connection.hpp"
#include "element_control.hpp"

#define NODE_CONFIG_PATH "/node_config.json"
#define MQTT_IP_CONFIG_PATH "/mqtt_ip.conf"
#define ELEMENT_SUMMARY_PATH "/element_summary.json"

#define UNREGED_TOPIC "unregistered"
#define REGED_TOPIC "registered"

#define LWT_PAYLOAD R"({"command": "AnnounceState", "data": "false"})"

#define REGED_ANNOUNCE_ONLINE_PAYLOAD R"({"command": "AnnounceState", "data": "true"})"

// TODO
// This would usually be located in a json file saved in the flash mem (location: ELEMENT_SUMMARY_PATH)
// #define ELEMENT_SUMMARY R"({"command": "ElementSummary", "data": "[{\"address\":\"0xtest_address\", \"element_type\":\"BasicSwitch\"},{\"address\":\"0xtest_address2\", \"element_type\":\"Thermostat\"},{\"address\":\"0xtest_address3\", \"element_type\":\"DHT11\"}]"})"
#define ELEMENT_SUMMARY R"({"command": "ElementSummary", "data": "[{\"address\":\"0xtest_address\", \"element_type\":\"BasicSwitch\"},{\"address\":\"0xtest_address3\", \"element_type\":\"DHT11\"}]"})"

#define UNREGED_USERNAME "unregistered_node"
#define UNREGED_PASSWORD "unregistered"

const int clientID_len = 10;
char unreged_clientID[clientID_len + 1] = "";
char unreged_topic_self[sizeof(UNREGED_TOPIC) + 1 + sizeof(unreged_clientID) + 1] = "";

// 3 ('reg' prefix) 10 (random chars) 1 (termination char)
char reged_identifier[3 + 10 + 1] = "";
char reged_password[16 + 1] = "";
char reged_topic_self[sizeof(REGED_TOPIC) + 1 + sizeof(reged_identifier) + 1] = "";

bool isNodeRegistered = false;

void onMQTTConnect(PubSubClient* mqtt_client, boolean successfull, int state) {
    if (successfull) {
        Serial.println("MQTT Connection established.");

        if (isNodeRegistered) {
            Serial.println("Subscribing to registered...");

            mqtt_client->publish(reged_topic_self, REGED_ANNOUNCE_ONLINE_PAYLOAD);
            element_initial_state_send(mqtt_client);

            mqtt_client->subscribe(reged_topic_self, 1);
            mqtt_client->subscribe(REGED_TOPIC, 1);
        } else {
            Serial.println("Subscribing to unregistered...");

            mqtt_client->publish(unreged_topic_self, ELEMENT_SUMMARY, false);

            mqtt_client->subscribe(unreged_topic_self, 1);
            mqtt_client->subscribe(UNREGED_TOPIC, 1);
        }
    } else {
        Serial.println("MQTT connection failed.");

        Serial.print("MQTT state: ");

        switch (state) {
            case -4:
            Serial.println("MQTT_CONNECTION_TIMEOUT");
                break;
            case -3:
            Serial.println("MQTT_CONNECTION_LOST");
                break;
            case -2:
            Serial.println("MQTT_CONNECT_FAILED");
                break;
            case -1:
            Serial.println("MQTT_DISCONNECTED");
                break;
            case 0:
            Serial.println("MQTT_CONNECTED");
                break;
            case 1:
            Serial.println("MQTT_CONNECT_BAD_PROTOCOL");
                break;
            case 2:
            Serial.println("MQTT_CONNECT_BAD_CLIENT_ID");
                break;
            case 3:
            Serial.println("MQTT_CONNECT_UNAVAILABLE");
                break;
            case 4:
            Serial.println("MQTT_CONNECT_BAD_CREDENTIALS");
                break;
            case 5:
            Serial.println("MQTT_CONNECT_UNAUTHORIZED");
                break;
            default:
            // https://pubsubclient.knolleary.net/api.html#state
            Serial.println(state);
                break;
        }
    }
}

void onMQTTMessage(PubSubClient* mqtt_client, char* topic, byte* payload, unsigned int len) {
    // Serial.println("\n_____TOPIC______");
    // Serial.println(topic);
    // Serial.println("_____PAYLOAD____");

    DynamicJsonDocument cmdData(JSON_OBJECT_SIZE(2) + 100);
    auto result = deserializeJson(cmdData, payload);

    // serializeJsonPretty(cmdData, Serial);

    if (result == DeserializationError::Ok) {
        if (strcmp(topic, UNREGED_TOPIC) == 0) {
            if (cmdData["command"] == "Announce") {
                mqtt_client->publish(unreged_topic_self, ELEMENT_SUMMARY);
            }
        } else if (strcmp(topic, unreged_topic_self) == 0) {
            if (cmdData["command"] == "ImplementCreds") {
               register_node(cmdData["data"]);
            }
        } else if (strcmp(topic, REGED_TOPIC) == 0) {
            if (cmdData["command"] == "Announce") {
                mqtt_client->publish(unreged_topic_self, REGED_ANNOUNCE_ONLINE_PAYLOAD);
                element_initial_state_send(mqtt_client);
            }
        } else if (strcmp(topic, reged_topic_self) == 0) {
            if (cmdData["command"] == "SetElementState") {
                DynamicJsonDocument set_elem_state(JSON_OBJECT_SIZE(2) + 100);
                auto result_ses = deserializeJson(set_elem_state, (const char*)cmdData["data"]);
                if (result_ses == DeserializationError::Ok) {
                    // serializeJsonPretty(set_elem_state, Serial);
                    process_command(mqtt_client, set_elem_state["id"], set_elem_state["data"]);
                } else {
                    Serial.println("SetElementState -> Could not parse received data.");
                }
            } else if (cmdData["command"] == "UnregisterNotify") {
                unregister_node(true);
            } else if (cmdData["command"] == "RestartDevice") {
                mqtt_client->publish(reged_topic_self, LWT_PAYLOAD);
                delay(500);
                ESP.restart();
            }
            // Sent to the network > else if (cmdData["command"] == "UpdateElementState") {}
        }
    } else {
        Serial.print("\nCould not parse received payload. ");
        Serial.println(result.c_str());
        Serial.println();
    }
}

bool init_mqtt(PubSubClient* mqtt_client) {
    char ip[16];

    get_mqtt_ip(ip);

    IPAddress mqtt_server_ip;
    mqtt_server_ip.fromString(ip);

    if (strcmp(ip, "0.0.0.0") != 0){
        mqtt_client->setCallback(onMQTTMessage);
        mqtt_client->setServer(mqtt_server_ip, 8883);

        if (node_registered()) {
            isNodeRegistered = true;
            sprintf(reged_topic_self, "%s/%s", REGED_TOPIC, reged_identifier);

            set_own_topic(reged_topic_self, sizeof(reged_topic_self));
        } else {
            isNodeRegistered = false;

            random_clientID(unreged_clientID);
            sprintf(unreged_topic_self, "%s/%s", UNREGED_TOPIC, unreged_clientID);

            char clID[11 + clientID_len + 1] = "";
            sprintf(clID, "Client ID: %s", unreged_clientID);
            Serial.println(clID);
        }

        connect_to_mqtt(mqtt_client);

        // mqtt_client->connect(unreged_clientID, UNREGED_USERNAME, UNREGED_PASSWORD, unreged_topic_self, 1, false, LWT_PAYLOAD);
        // Serial.printf("RES: %d\n", mqtt_client->state());
    }

    return isNodeRegistered;
}

void get_mqtt_ip(char* ip_dest) {
    File mqtt_ip_file = SPIFFS.open(MQTT_IP_CONFIG_PATH, "r");
    if (!mqtt_ip_file) {
        Serial.println("Failed to open the ip config file.");
        strncpy(ip_dest, "0.0.0.0", 8);
    } else {
        // This is safe here because we verified the data beforehand
        strcpy(ip_dest, mqtt_ip_file.readString().c_str());
    }
    mqtt_ip_file.close();
}

void connect_to_mqtt(PubSubClient* mqtt_client) {
    Serial.println("MQTT Connecting...");

    int connect_result = 0;

    // Serial.println(isNodeRegistered);

    if (isNodeRegistered) {
        // Serial.println(reged_identifier);
        // Serial.println(reged_password);
        // Serial.println(reged_topic_self);
        connect_result = mqtt_client->connect(reged_identifier, reged_identifier, reged_password, reged_topic_self, 1, false, LWT_PAYLOAD);
    } else {
        // Serial.println(unreged_clientID);
        // Serial.println(UNREGED_USERNAME);
        // Serial.println(UNREGED_PASSWORD);
        // Serial.println(unreged_topic_self);
        connect_result = mqtt_client->connect(unreged_clientID, UNREGED_USERNAME, UNREGED_PASSWORD, unreged_topic_self, 1, false, LWT_PAYLOAD);
    }

    onMQTTConnect(mqtt_client, connect_result, mqtt_client->state());
}

bool node_registered() {
    if (SPIFFS.exists(NODE_CONFIG_PATH)) {
        File configFile = SPIFFS.open(NODE_CONFIG_PATH, "r");
        if (configFile) {
            size_t f_size = configFile.size();
            std::unique_ptr<char[]> f_buff (new char[f_size]);

            configFile.readBytes(f_buff.get(), f_size);
            configFile.close();

            DynamicJsonDocument json_doc(JSON_OBJECT_SIZE(2) + 70);
            auto result = deserializeJson(json_doc, f_buff.get());

            if (result == DeserializationError::Ok) {
                const char* _reged_identifier = json_doc["identifier"];
                const char* _reged_password = json_doc["password"];

                strncpy(reged_identifier, _reged_identifier, sizeof(reged_identifier));
                strncpy(reged_password, _reged_password, sizeof(reged_password));

                Serial.println("Node configuration loaded. Node status: Registered");
                return true;
            } else {
                Serial.println("Error: Could not deserialize Node configuration JSON.");
                Serial.println(result.c_str());
            }
        } else {
            Serial.println("Could not open Node configuration file.");
        }
    } else {
        Serial.println("No Node configuration file found. Node status: Not Registered.");
    }

    return false;
}

void register_node(const char* data) {
    char* data_c = strdup(data);

    DynamicJsonDocument json_doc(JSON_OBJECT_SIZE(2) + 500);

    const char* id = strtok(data_c, ":");
    const char* pass = strtok(NULL, ":");

    json_doc["identifier"] = id;
    json_doc["password"] = pass;

    File config_file = SPIFFS.open(NODE_CONFIG_PATH, "w");
    serializeJson(json_doc, config_file);
    config_file.close();

    delay(500);

    Serial.println("Node registered.");

    ESP.restart();
}

void unregister_node(bool restart) {
    Serial.println("\nUnregistering the node - Removing the config file...");
    SPIFFS.remove(NODE_CONFIG_PATH);

    if (restart) {
        Serial.println("Restarting...");
        delay(500);
        ESP.restart();
    }
}

void random_clientID(char *s) {
    const char alphanum[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

    randomSeed(millis());

    for (int i = 0; i < clientID_len; ++i) {
        s[i] = alphanum[random(0, sizeof(alphanum)-1)];
    }

    s[clientID_len] = 0;
}
