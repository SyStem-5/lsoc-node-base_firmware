/// INCLUDES
#include <DHTesp.h>
/// END

#include "element_control.hpp"

/*
{
  "command": "SetElementState",
  "data": "{\"id\":\"0xtest_address2\",\"data\":\"+\"}"
}
*/

/// PINS
// note: the name starts with an '_' and ends with '_PIN'

#define _TEST_ADDRESS_PIN 16
#define _TEST_ADDRESS3_PIN 4

/// END


/// ELEMENT STATES
// note: the variable must start with an '_'

bool _test_address = false;

DHTesp *_test_address3;
unsigned long _test_address_3_previous_millis = 0;
/// END

/// ELEMENT ADDRESSES (CONTROLLABLE ONLY)
const int elem_num = 1;
const char* element_addresses[1] = {"0xtest_address"};
/// END

char own_topic[30] = "";


void element_setup() {
    /// ELEMENT SETUP

    // note: default state for elements is off
    pinMode(_TEST_ADDRESS_PIN, OUTPUT);
    digitalWrite(_TEST_ADDRESS_PIN, HIGH);

    _test_address3 = new DHTesp;
    _test_address3->setup(_TEST_ADDRESS3_PIN, DHTesp::DHT11);
    /// END
}

void element_initial_state_send(PubSubClient* mqtt_client) {
    // POSSIBLE STATE SIZE
    // note: increase if needed
    char new_state[10] = "";

    for (int i = 0; i < elem_num; i++) {
        if (strcmp(element_addresses[i], "0xtest_address") == 0) {
            // ::BasicSwitch
            if (_test_address) {
                strncpy(new_state, "true", 5);
            } else {
                strncpy(new_state, "false", 6);
            }
            char payload[70 + sizeof(new_state)] = "";
            sprintf(payload, R"({"command": "UpdateElementState", "data": "0xtest_address::%s"})", new_state);

            mqtt_client->publish(own_topic, payload);
        }
    }
}

void process_command(PubSubClient* mqtt_client, const char* elem_addr, const char* data) {
    // POSSIBLE STATE SIZE
    // note: increase if needed
    char new_state[10] = "";

    /// ELEMENT ADDRESS TABLE
    // note: only elements that accept commands are here

    if (strcmp(elem_addr, "0xtest_address") == 0) {
        // ::BasicSwitch
        if (_test_address) {
            digitalWrite(_TEST_ADDRESS_PIN, HIGH);
            strncpy(new_state, "false", 6);
        } else {
            digitalWrite(_TEST_ADDRESS_PIN, LOW);
            strncpy(new_state, "true", 5);
        }
        _test_address = !_test_address;
    } else {
        return;
    }/* else if (strcmp(elem_addr, "0xtest_address2") == 0) {
        // ::Thermostat
        return;
    } */
    /// END

    char payload[70 + sizeof(new_state)] = "";
    sprintf(payload, R"({"command": "UpdateElementState", "data": "%s::%s"})", elem_addr, new_state);

    mqtt_client->publish(own_topic, payload);
}

void element_loop(unsigned long current_millis, PubSubClient* mqtt_client) {
    if (_test_address3 != nullptr && current_millis - _test_address_3_previous_millis >= 10000) {
        float humidity = _test_address3->getHumidity();
        float temperature = _test_address3->getTemperature();

        char payload[70+20] = "";
        sprintf(payload, R"({"command": "UpdateElementState", "data": "0xtest_address3::{\"temp\": \"%.0f\", \"hum\": \"%.0f\"}"})", temperature, humidity);

        mqtt_client->publish(own_topic, payload, false);

        _test_address_3_previous_millis = current_millis;
    }
}

void set_own_topic(const char* topic, size_t size) {
    strncpy(own_topic, topic, size);
}
