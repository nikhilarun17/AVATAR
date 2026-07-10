"""LED blink demo -- pairs with the Arduino LedBlink.ino example.

Run after `pip install ../` (or `pip install AVATAR`):
    python led_blink.py

Commands:
    on / off       enable/disable blinking (bool)
    1..500         set divisor (int): interval = 500 / value ms
    both <n>       send enable=True AND divisor=n in one frame
    mute / unmute  hide/show board telemetry
    q              quit

Telemetry prints only when the board's state changes, so a steady LED
won't flood the prompt.
"""

from AVATAR import AVATARlink

LAYOUT = {
    # host -> board
    "led_enable": 1,
    "blink_div": 2,
    # board -> host
    "state_enabled": 10,
    "state_interval": 11,
    "state_freq_hz": 12,
    "state_txt": 13,
}

link = AVATARlink(port="/dev/ttyACM0", layout=LAYOUT)

_last = None
_muted = False


def on_packet(fields):
    global _last
    if _muted:
        return
    snap = tuple(sorted(fields.items()))
    if snap == _last:
        return
    _last = snap
    print(f"\r\033[K[board] {fields}")
    print("CMD > ", end="", flush=True)


link.packet_callback = on_packet

print("LED blink demo. interval = 500 / value ms")
print("  on/off  1..500  both <n>  mute/unmute  q")

while True:
    try:
        cmd = input("CMD > ").strip().lower()
        if cmd == "q":
            break
        elif cmd == "mute":
            _muted = True
            print("muted")
        elif cmd == "unmute":
            _muted = False
            _last = None
            print("unmuted")
        elif cmd == "on":
            link.send_value("led_enable", True)
        elif cmd == "off":
            link.send_value("led_enable", False)
        elif cmd.startswith("both"):
            parts = cmd.split()
            if len(parts) == 2 and parts[1].isdigit():
                link.send_fields({"led_enable": True, "blink_div": int(parts[1])})
            else:
                print("usage: both <divisor>")
        elif cmd.lstrip("-").isdigit():
            v = int(cmd)
            if 1 <= v <= 500:
                link.send_value("blink_div", v)
            else:
                print("divisor 1..500")
        elif cmd == "":
            continue
        else:
            print("unknown command")
    except KeyboardInterrupt:
        break

link.close()
