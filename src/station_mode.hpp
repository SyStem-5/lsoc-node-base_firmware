#pragma once

#include <ESP8266WiFi.h>

#define sta_config_path "/sta_config.json"
// static const char* sta_config_path = "/sta_config.json";

void onWifiConnect(const WiFiEventStationModeConnected& event);
void onWifiDisconnect(const WiFiEventStationModeDisconnected& event);

void connect_to_ap(const char* ssid, const char* password);
bool start_sta_mode(BearSSL::WiFiClientSecure*);
void cleanup_config();
