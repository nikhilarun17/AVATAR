"""Telemetry demo -- pairs with the Arduino Telemetry.ino example.

Prints the bool/int/float/string the board streams every 500 ms, and lets you
send an int back on ID 1.

    python telemetry.py
"""

from AVATAR import AVATARlink

LAYOUT = {
    "cmd_value": 1,   # host -> board (int)
    "flag": 20,       # board -> host (bool)
    "counter": 21,    # int
    "voltage": 22,    # float
    "label": 23,      # string
}

link = AVATARlink(port="/dev/ttyACM0", layout=LAYOUT)
link.packet_callback = lambda f: print(f"\r\033[K[board] {f}\ncmd > ", end="", flush=True)

print("Telemetry demo. Type an int to send on ID 1, or q to quit.")
while True:
    try:
        s = input("cmd > ").strip()
        if s.lower() == "q":
            break
        if s.lstrip("-").isdigit():
            link.send_value("cmd_value", int(s))
    except KeyboardInterrupt:
        break

link.close()
