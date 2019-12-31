#include <FS.h>
#include <Ticker.h>

#include <ArduinoJson.h>

#include "station_mode.hpp"

WiFiEventHandler wifiConnectHandler;
WiFiEventHandler wifiDisconnectHandler;

Ticker wifiReconnectTimer;
BearSSL::X509List x509_cert;

bool start_sta_mode(BearSSL::WiFiClientSecure* client) {
    // Reset networking
    // WiFi.softAPdisconnect(true);
    // WiFi.disconnect(true);
    // delay(1000);

    File configFile = SPIFFS.open(sta_config_path, "r");
    if (configFile) {
        size_t f_size = configFile.size();
        std::unique_ptr<char[]> f_buff (new char[f_size]);

        configFile.readBytes(f_buff.get(), f_size);
        configFile.close();

        DynamicJsonDocument json_doc(JSON_OBJECT_SIZE(3) + 1490);
        auto result = deserializeJson(json_doc, f_buff.get());

        if (result == DeserializationError::Ok) {
            const char* ssid = json_doc["ssid"];
            const char* password = json_doc["password"];

            // Save MQTT server certificate to the store
            x509_cert.append(json_doc["cert"]);

            Serial.println("STA config loaded.");

            connect_to_ap(ssid, password);

            // Get and set the current time - used for cert verification
            configTime(3 * 3600, 0, "pool.ntp.org", "time.nist.gov");
            Serial.print("Waiting for NTP time sync...");
            time_t now = time(nullptr);
            while (now < 8 * 3600 * 2) {
                delay(500);
                now = time(nullptr);
            }

            client->setX509Time(now);
            client->setTrustAnchors(&x509_cert);

            Serial.println("\nSTA mode initialized.");

            return true;
        } else {
            Serial.println("Error: Could not deserialize AP JSON.");
            Serial.println(result.c_str());

            Serial.println("Error: Failed to parse STA configuration file.");
        }
    } else {
        Serial.println("Error: Could not open STA config file.");
    }

    return false;
}

void cleanup_config() {
    Serial.println("\nRemoving STA config file...");
    SPIFFS.remove(sta_config_path);

    // #define MQTT_IP_CONFIG_PATH "/mqtt_ip.conf"
    SPIFFS.remove("/mqtt_ip.conf");
}

// This is a far fetch, might not work
void _connect() {
    WiFi.reconnect();
}

void connect_to_ap(const char* ssid, const char* password) {
    Serial.print("Connecting to SSID: ");
    Serial.println(ssid);

    wifiDisconnectHandler = WiFi.onStationModeDisconnected(onWifiDisconnect);

    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);

    // Give the networking equipment time to establish a connection,
    // if we go into that wile loop too soon, we won't connect at all
    delay(1000);

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
    }

    // This is registered after connecting to the AP
    // So that we can connect to the MQTT server from main.cpp or after wifi disconnection
    wifiConnectHandler = WiFi.onStationModeConnected(onWifiConnect);
}

void onWifiConnect(const WiFiEventStationModeConnected& event) {
    Serial.println("\nWiFi connection established.");
}

void onWifiDisconnect(const WiFiEventStationModeDisconnected& event) {
    Serial.println("\nWiFi connection lost.");

    wifiReconnectTimer.once(2, _connect);
}
