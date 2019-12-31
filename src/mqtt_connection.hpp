#pragma once

#include <WiFiClientSecure.h>
#include <PubSubClient.h>

void connect_to_mqtt(PubSubClient*);
bool init_mqtt(PubSubClient*);

void get_mqtt_ip(char* ip_dest);
bool node_registered();
void random_clientID(char *s);
void register_node(const char* data);
void unregister_node(bool);
