# AVATAR — Python (host) side

Host-side counterpart to the Arduino library. Sends and receives the same
bool / int / float / string fields over serial, with a background reader thread.

## Install

### Install the Python host package

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


## Minimal use

```python
from AVATAR import AVATARlink

# Optional: map names <-> IDs so you don't juggle raw integers.
LAYOUT = {"pwm": 2, "flag": 20, "voltage": 22, "label": 23}

link = AVATARlink(port="/dev/ttyACM0", layout=LAYOUT) #Replace with own port name
link.packet_callback = lambda fields: print("got:", fields)

link.send_value("pwm", 128)              # int (inferred)
link.send_value("flag", True)            # bool
link.send_fields({"pwm": 0, "flag": False})  # both in one frame

link.close()
```

Type is inferred from the Python value: `bool → bool`, `int → int`,
`float → float`, `str → string`. (Note `bool` is checked before `int`, so
`True` is sent as a bool, not `1`.)

## API

| member | purpose |
|--------|---------|
| `AVATARlink(port, baud=115200, layout=None, auto_reset=False)` | open the link |
| `send_value(name_or_id, value)` | send one field |
| `send_fields({name_or_id: value, ...})` | send many in one frame |
| `packet_callback` | set to a function `fn(fields: dict)` |
| `close()` | stop the reader and close the port |

Received fields arrive as a dict keyed by name (if in `layout`) or by integer
ID otherwise. Works as a context manager (`with AVATAR(...) as link:`).

## `auto_reset`

Default `False` holds DTR/RTS low so boards with an auto-reset circuit don't
reboot when the port opens. Set `True` if you *want* the board to reset on
connect.

See [`../docs/PROTOCOL.md`](../docs/PROTOCOL.md) for the wire format.
