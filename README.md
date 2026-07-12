# **A**nveshak **V**ehicle **A**rchitecture for **T**elemetry and **A**sync **R**eading (**AVATAR**)
## micro-ROS replacement for Arduino

This is an library which serves to help for communication between ROS2 nodes and microcontrollers using a duplex serial link ( Google Protobuf encoding and COBS framing ) between the host which runs a python script and an Arduino/ESP32.

It is a very light-weight alternative to microros and can be run on ESP32 Boards which do not support microros.

Send named fields — **bool, int, float, string** — in either
direction over a single serial connection, with automatic framing and parsing. No syncing exists byte-for-byte (instead uses a self-describing field with ID+type), allowing for adding or removing of fields on either side without breaking the wire format.

**Important Note**: Close any other program using the same serial port before running the code (python side) including Arduino IDE or PlatformIO. 

Check [Information on Ports](#information-on-ports)


```
  Python host  <───serial───>  Arduino / ESP32
  link.send_value("pwm", 128)        link.getInt(2)
  print(fields)  <──────────────     link.addFloat(22, 3.3f); link.sendTx()
```

## Repository layout

```
AVATAR/
├── README.md              you are here
├── docs/PROTOCOL.md       the wire format 
├── arduino/                the C++ library
│   ├── README.md          C++ usage
│   └── AVATAR/               <- self-contained Arduino library 
│       ├── library.properties
│       ├── src/AVATAR.h
│       └── examples/
├── python/                 the Python package
│   ├── README.md          Python usage
│   ├── pyproject.toml     pip-installable
│   ├── AVATAR/                the package
│   └── examples/
├── examples/                paired end-to-end demos
│   └── led_blink/
├── dist/AVATAR.zip            prebuilt Arduino library (download this alone if you don't want to clone the repo)
└── tools/
    └── build_arduino_zip.sh
```

## Quick start (installation)

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

### 3. Install the Python host package

```bash
# replace ~/venv with the path to your own virtual environment
source ~/venv/bin/activate
```

```bash
cd ~/AVATAR/python
pip install .
```

If you're not using a virtual environment, plain `pip install .` may be
blocked on newer Debian/Ubuntu and macOS installs (`externally-managed-environment`
error). Either use a venv as above, or run:

```bash
cd ~/AVATAR/python
pip install . --break-system-packages
```

### 4. Run an example

```bash
cd ~/AVATAR/python/examples
python led_blink.py
```

See [`examples/led_blink/`](examples/led_blink/) for the two sides side by side.

## Information on Ports

### Finding your serial port

The board shows up as a different device name depending on your OS. You need
this to fill in `port=` in the Python scripts, and it's also the port that
must **not** be open anywhere else (Arduino Serial Monitor, `screen`, another
Python script) when you run your own script — only one program can hold a
serial port at a time.

**Linux:**
```bash
ls /dev/ttyUSB* /dev/ttyACM* 2>/dev/null
```
Boards usually show up as `/dev/ttyUSB0` (CP2102/CH340-based boards) or
`/dev/ttyACM0` (native USB, e.g. many ESP32-S2/S3, Leonardo, Due).

**macOS:**
```bash
ls /dev/cu.*
```
Look for something like `/dev/cu.usbserial-0001` or `/dev/cu.usbmodem14101`.
Use the `cu.*` device, not `tty.*`.

**Windows:**
Open Device Manager → *Ports (COM & LPT)* and note the `COMx` number, or run
in PowerShell:
```powershell
[System.IO.Ports.SerialPort]::GetPortNames()
```

Once you have it, change the `port=` argument in whichever script you're
running, e.g.:
```python
link = AVATARlink(port="/dev/ttyUSB0", layout=LAYOUT)   # Linux
link = AVATARlink(port="COM3", layout=LAYOUT)            # Windows
```

### Warnings

- **Close the Arduino Serial Monitor before running a Python script** (and
  vice versa) — a port can only be opened by one process at a time; the second
  one will fail with a "port busy" / "resource unavailable" error.
- **Linux permission errors** (`Permission denied: '/dev/ttyUSB0'`) usually
  mean your user isn't in the `dialout` group:
```bash
  sudo usermod -a -G dialout $USER
```
  Log out and back in for it to take effect.
- **Board reboots when the port opens** on some ESP32/Arduino boards — this
  library holds DTR/RTS low by default to avoid that (`auto_reset=False`,
  see the [Python README](python/README.md)), but on boards where the reset
  circuit ignores those lines you may still see a brief reset; this is
  expected and harmless, just wait ~2s (the library already does) before
  sending data.
- **Port names can change** between reboots or replugs, especially with
  multiple USB-serial boards connected — always re-check with the commands
  above if the script suddenly can't find the board.

## License

MIT — see [LICENSE](LICENSE).