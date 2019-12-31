#include <PubSubClient.h>

void element_setup();

void element_initial_state_send(PubSubClient* mqtt_client);

void process_command(PubSubClient* mqtt_client, const char* elem_addr, const char* data);

void element_loop(unsigned long current_millis, PubSubClient* mqtt_client);

void set_own_topic(const char* topic, size_t size);
