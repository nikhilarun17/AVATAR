// Telemetry -- minimal all-types demo.
//
// Sends one packet every 500 ms containing a bool, an int, a float, and a
// string, and prints any int received on ID 1. Use this to confirm every
// type survives the wire in both directions.

#include <AVATAR.h>

namespace Wire {
    const uint32_t CMD_VALUE  = 1;   // int, host -> board
    const uint32_t T_FLAG     = 20;  // bool
    const uint32_t T_COUNTER  = 21;  // int
    const uint32_t T_VOLTAGE  = 22;  // float
    const uint32_t T_LABEL    = 23;  // string
}

AVATAR link_name;
unsigned long last_tx = 0;
int64_t counter = 0;

void onPacket() {
    if (link_name.hasField(Wire::CMD_VALUE)) {
        int64_t v = link_name.getInt(Wire::CMD_VALUE);
        (void)v;  // do something with it in your own code
    }
}

void setup() {
    link_name.begin(115200);
    link_name.setCallback(onPacket);
}

void loop() {
    link_name.update();
    if (millis() - last_tx >= 500) {
        last_tx = millis();
        link_name.clearTx();
        link_name.addBool(Wire::T_FLAG, (counter % 2) == 0);
        link_name.addInt(Wire::T_COUNTER, counter++);
        link_name.addFloat(Wire::T_VOLTAGE, 3.3f + (counter % 10) * 0.01f);
        link_name.addString(Wire::T_LABEL, "hello from board");
        link_name.sendTx();
    }
}
