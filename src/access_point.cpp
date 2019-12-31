#include <ESP8266WebServer.h>
#include <FS.h>
#include <ArduinoJson.h>

#include "access_point.hpp"
#include "station_mode.hpp"

ESP8266WebServer server(80);

const char* ap_ssid = "hive_node";
const char* ap_default_pass = "forgot_to_save_creds";
const char* ap_config_path = "/ap_config.json";


void start_setup_server() {
    server.on("/", []() {
        Serial.println("Index get");
        server.send_P(200, "text/html", setup_webpage);
    });

    server.on("/save", HTTP_POST, [](){
        Serial.println("Data save");
        handle_setup_post();
    });

    server.onNotFound([]() { server.send(404, "text/plain", "404: Not found"); });

    server.begin();
    Serial.println("HTTP server started");
}

bool start_setup_ap() {
    // Set the mode to access point
    WiFi.mode(WIFI_AP);
    // RF TX power to the lowest setting
    WiFi.setOutputPower(0);
    // This is just for ease of use
    WiFi.softAPConfig(IPAddress(2, 2, 2, 2), IPAddress(2, 2, 2, 2), IPAddress(255, 255, 255, 0));

    String wifi_pass = "";
    if (SPIFFS.exists(ap_config_path)) {
        File configFile = SPIFFS.open(ap_config_path, "r");
        if (configFile) {
            size_t f_size = configFile.size();
            std::unique_ptr<char[]> f_buff (new char[f_size]);

            configFile.readBytes(f_buff.get(), f_size);
            configFile.close();

            DynamicJsonDocument json_doc(JSON_OBJECT_SIZE(1) + 50);
            auto result = deserializeJson(json_doc, f_buff.get());

            if (result == DeserializationError::Ok) {
                wifi_pass = json_doc["password"].as<const char*>();

                Serial.println("AP password loaded.");
            } else {
                Serial.println("Error: Could not deserialize AP JSON.");
                Serial.println(result.c_str());

                // If we fail parsing JSON, set the default password
                wifi_pass = ap_default_pass;

                Serial.println("Failed to parse AP configuration file, using default password.");
            }
        } else {
            // If we fail opening the file, set the default password
            wifi_pass = ap_default_pass;

            Serial.println("Could not open AP config file, using default password.");
        }
    } else {
        // If we cannot find the access AP file, use the default AP password
        wifi_pass = ap_default_pass;

        Serial.println("No AP config file found, using default password.");
    }

    // Start the AP
    if (!WiFi.softAP(ap_ssid, wifi_pass, 1, 0, 1)) {
        Serial.println("Wifi error.");
        //WiFi.printDiag(Serial);

        return false;
    }

    Serial.print("AP IP: ");
    Serial.println(WiFi.softAPIP());

    start_setup_server();

    return true;
}

void handle_setup_post() {
    bool error = false;

    String data = server.arg("plain");

    DynamicJsonDocument json_doc(JSON_OBJECT_SIZE(3) + 1490);
    auto result = deserializeJson(json_doc, data);

    if (result == DeserializationError::Ok) {
        IPAddress addr;

        if (!json_doc.containsKey("ssid") || !json_doc.containsKey("password") || !json_doc.containsKey("cert") || !json_doc.containsKey("ip"))
            error = true;
        else {
            if (addr.fromString((const char*)json_doc["ip"])) {

                // #define MQTT_IP_CONFIG_PATH "/mqtt_ip.conf"
                // Save the ip in a different file before removing it
                File ip_file = SPIFFS.open("/mqtt_ip.conf", "w");
                ip_file.print((const char*)json_doc["ip"]);
                ip_file.close();

                json_doc.remove("ip");

                File sta_file = SPIFFS.open(sta_config_path, "w");
                serializeJson(json_doc, sta_file);
                sta_file.close();

                server.send(200, "application/json", "{\"status\" : \"Saved\"}");

                Serial.println("\nConfiguration saved, calling determine_mode...");

                Serial.println("Restarting module...");
                delay(100);
                ESP.restart();
            } else {
                error = true;
            }
        }
    } else {
        Serial.println("Could not parse POST request json file.");
        error = true;
    }

    if (error)
        server.send(200, "application/json", "{\"status\" : \"Parsing failed.\"}");
}

void handle_client() {
    server.handleClient();
}
