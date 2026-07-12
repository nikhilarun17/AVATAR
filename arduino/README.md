# AVATAR — Arduino / ESP32 side

Header-only typed serial link. One class, `AVATAR`, handles framing, encoding,
and parsing of bool / int / float / string fields addressed by numeric IDs.

## Installation

### 1. Clone and build the Arduino library

```bash
git clone https://github.com/nikhilarun17/AVATAR.git ~/AVATAR
cd ~/AVATAR
chmod +x tools/build_arduino_zip.sh
./tools/build_arduino_zip.sh
mkdir -p ~/Arduino/libraries
cp dist/AVATAR.zip ~/Arduino/libraries/
cd ~/Arduino/libraries
unzip -o AVATAR.zip
```

Replace `~/Arduino/libraries` above with your actual Arduino sketchbook `libraries` folder if it's somewhere else (check Arduino IDE → *File → Preferences → Sketchbook location*).

Note: Restart Arduino IDE before using the library 

To run example : go to *File → Examples → AVATAR → LedBlink*.

### 2. Just want the library, not the whole repo?

```bash
mkdir -p ~/Arduino/libraries
cd ~/Arduino/libraries
curl -L -o AVATAR.zip https://github.com/nikhilarun17/AVATAR/raw/main/dist/AVATAR.zip
unzip -o AVATAR.zip
```

## Minimal use

```cpp
#include <AVATAR.h>

AVATAR link_name;

// Callback function (rename it if needed)
void Callback_function_name() {
    if (link_name.hasField(2)) {
        int64_t v = link_name.getInt(2);
        // use v
    }
}

void setup() {
    link_name.begin(115200);
    link_name.setCallback(Callback_function_name);
}

void loop() {
    link_name.update();             // parse inbound frames (non-blocking)

    link_name.clearTx();            // Clearing buffer before sending
    // link_name.add*(id, msg)
    // Note: match the IDs with the layout dict on the Python side.
    // Here, IDs 20,21,22,23 carry a Bool, Int, Float, String respectively.
    link_name.addBool(20, true);
    link_name.addInt(21, 1234);
    link_name.addFloat(22, 3.14f);
    link_name.addString(23, "hello");
    link_name.sendTx();            // one frame with four fields

}
```

## API

| method | purpose |
|--------|---------|
| `begin(baud=115200)` | start Serial |
| `setCallback(fn)` | called once per received frame |
| `update()` | poll serial, parse frames; call every loop |
| `hasField(id)` | was `id` present in the last frame? |
| `getBool/getInt/getFloat/getString(id)` | read a received field |
| `clearTx()` | begin a new outgoing frame |
| `addBool/addInt/addFloat/addString(id, v)` | queue a field |
| `sendTx()` | encode + send the queued fields |

## Limits (compile-time constants in `AVATAR.h`)

* `MAX_FIELDS` 60 — fields per frame
* `MAX_RAW_LEN` 512 — decoded frame bytes
* strings truncate at 95 bytes

Received data is only valid inside the callback (or right after `update()`),
since the next frame overwrites the buffer — copy anything you need to keep.

See [`../docs/PROTOCOL.md`](../docs/PROTOCOL.md) for the wire format.
