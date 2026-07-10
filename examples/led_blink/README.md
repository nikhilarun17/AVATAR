# Example: LED Blink

An end-to-end demo of both directions and all four field types.

The host sends an **int** (blink divisor) and a **bool** (enable). The board
blinks an LED at `500 / divisor` ms and echoes its state back once a second as
a **bool**, **int**, **float**, and **string**.

## Wiring

An LED (with a resistor) on the pin defined by `LED_PIN` in the sketch
(default `26`). No external LED? Change it to `LED_BUILTIN`.

## Run

1. Flash `arduino/LedBlink/LedBlink.ino` (needs the `AVATAR` library installed).
2. Install the host package: `pip install ../../python`
3. Run the host: `python python/led_blink.py`
   (edit the `port=` argument for your system, e.g. `COM3` on Windows).

## Try

```
CMD > on          # LED starts blinking at 500 ms
CMD > 5           # speeds up to 100 ms
CMD > both 25     # enable + 20 ms in one packet
CMD > off         # dark
```

The `[board] {...}` lines are the echoed telemetry — they confirm every type
round-trips correctly.
