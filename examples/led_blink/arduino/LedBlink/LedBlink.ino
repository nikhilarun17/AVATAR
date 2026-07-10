// LedBlink -- receive an int + bool, echo state back with all four types.
//
// Host sends:
//   ID 1 (bool)  led_enable : master on/off
//   ID 2 (int)   blink_div  : blink interval = 500 / blink_div  (ms)
// Board echoes every second:
//   ID 10 (bool) enabled
//   ID 11 (int)  interval_ms
//   ID 12 (float) freq_hz
//   ID 13 (string) status
//
// Pair this with python/examples/led_blink.py (or examples/led_blink/).

#include <AVATAR.h>

namespace Wire {
    const uint32_t LED_ENABLE     = 1;   // bool  (host -> board)
    const uint32_t BLINK_DIV      = 2;   // int   (host -> board)
    const uint32_t STATE_ENABLED  = 10;  // bool  (board -> host)
    const uint32_t STATE_INTERVAL = 11;  // int
    const uint32_t STATE_FREQ_HZ  = 12;  // float
    const uint32_t STATE_TXT      = 13;  // string
}

#define LED_PIN 26   // change to LED_BUILTIN if you have no external LED

AVATAR link_name;

bool          led_enabled    = false;
int64_t       blink_divisor  = 1;
uint32_t      blink_interval = 500;
bool          led_state      = false;
unsigned long last_toggle    = 0;
unsigned long last_echo      = 0;

void recomputeInterval() {
    if (blink_divisor < 1)   blink_divisor = 1;
    if (blink_divisor > 500) blink_divisor = 500;
    blink_interval = (uint32_t)(500 / blink_divisor);
    if (blink_interval < 1) blink_interval = 1;
}

void onPacket() {
    if (link_name.hasField(Wire::LED_ENABLE)) {
        led_enabled = link_name.getBool(Wire::LED_ENABLE);
        if (!led_enabled) { led_state = false; digitalWrite(LED_PIN, LOW); }
    }
    if (link_name.hasField(Wire::BLINK_DIV)) {
        blink_divisor = link_name.getInt(Wire::BLINK_DIV);
        recomputeInterval();
        last_toggle = millis();
    }
}

void echoState() {
    link_name.clearTx();
    link_name.addBool(Wire::STATE_ENABLED, led_enabled);
    link_name.addInt(Wire::STATE_INTERVAL, (int64_t)blink_interval);
    link_name.addFloat(Wire::STATE_FREQ_HZ, led_enabled ? (1000.0f / (2.0f * blink_interval)) : 0.0f);
    link_name.addString(Wire::STATE_TXT, led_enabled ? "BLINKING" : "IDLE");
    link_name.sendTx();
}

void setup() {
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);
    link_name.begin(115200);
    link_name.setCallback(onPacket);
    recomputeInterval();
}

void loop() {
    link_name.update();

    if (led_enabled && (millis() - last_toggle >= blink_interval)) {
        last_toggle = millis();
        led_state = !led_state;
        digitalWrite(LED_PIN, led_state ? HIGH : LOW);
    }
    if (millis() - last_echo >= 1000) {
        last_echo = millis();
        echoState();
    }
}
