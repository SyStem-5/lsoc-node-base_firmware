#include "access_point.hpp"
#include "station_mode.hpp"
#include "mqtt_connection.hpp"
#include "element_control.hpp"

#define BTN_CONFIG_RST_PIN 0
#define STATUS_LED_PIN 2

#define LED_AP_DURATION 200
#define LED_STA_DURATION 10000

BearSSL::WiFiClientSecure wifi;
PubSubClient mqtt_client(wifi);

bool ap_mode = false;

unsigned long previousMillis = 0;
unsigned long mqtt_previous_millis = 0;


void led_status(unsigned long current) {
    if (ap_mode) {
        if (current - previousMillis >= LED_AP_DURATION) {
            previousMillis = current;
            digitalWrite(STATUS_LED_PIN, !digitalRead(STATUS_LED_PIN));
        }
    } else {
        if (current - previousMillis >= LED_STA_DURATION) {
            previousMillis = current;
            digitalWrite(STATUS_LED_PIN, 0);
        } else if (current - previousMillis >= (LED_STA_DURATION / 100)) {
            digitalWrite(STATUS_LED_PIN, 1);
        }
    }
}

void determine_mode()  {
    if (SPIFFS.exists(sta_config_path)) {
        Serial.println("Booting into STA mode.");

        ap_mode = !start_sta_mode(&wifi);

        if (!ap_mode) {
            // Only if we're a registered node - setup the elements
            if (init_mqtt(&mqtt_client)) {
                element_setup();
            }
        }
    } else {
        Serial.println("Booting into AP mode.");
        ap_mode = start_setup_ap();
    }
}

void setup() {
    // system_update_cpu_freq(160);

    pinMode(BTN_CONFIG_RST_PIN, INPUT_PULLUP);

    pinMode(STATUS_LED_PIN, OUTPUT);
    digitalWrite(STATUS_LED_PIN, 1);

    Serial.begin(115200);
    SPIFFS.begin();

    delay(5000);

    // File ip_file = SPIFFS.open("/mqtt_ip.conf", "w");
    // ip_file.print("192.168.0.107");
    // ip_file.close();
    // SPIFFS.remove("/mqtt_ip.conf");

    if (digitalRead(BTN_CONFIG_RST_PIN) == LOW) {
        // Remove the wifi config file
        cleanup_config();

        // Call the mqtt config cleanup
        unregister_node(false);
    }

    Serial.println("Initializing...");

    determine_mode();
}

void loop() {
    unsigned long current_millis = millis();
    led_status(current_millis);

    if (ap_mode) {
        handle_client();
    } else {
        if (mqtt_client.loop()) {
            element_loop(current_millis, &mqtt_client);
        } else {
            if (WiFi.isConnected()) {
                if (current_millis - mqtt_previous_millis >= 5000) {
                    init_mqtt(&mqtt_client);
                    mqtt_previous_millis = current_millis;
                }
            }
        }
    }
}
