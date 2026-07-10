"""AVATAR -- host-side typed serial link.

Full-duplex counterpart to the Arduino/ESP32 AVATAR library. Encodes and decodes
bool / int / float / string fields addressed by numeric IDs, over COBS-framed
protobuf-style packets. A background thread reads incoming frames and invokes
an optional callback.

SPDX-License-Identifier: MIT
"""

import struct
import threading
import time

import serial

TYPE_BOOL, TYPE_INT, TYPE_FLOAT, TYPE_STRING = 1, 2, 3, 4


class AVATARlink:
    def __init__(self, port="/dev/ttyACM0", baud=115200, layout=None, auto_reset=False):
        """Open the serial link.

        port       : serial device (e.g. '/dev/ttyACM0' or 'COM3')
        baud       : must match the board's link.begin(baud)
        layout     : optional {name: id} dict. When set, received fields are
                     keyed by name and send_value/send_fields accept names.
                     Leave None to work with raw integer IDs.
        auto_reset : if False (default), DTR/RTS are held low so boards with an
                     auto-reset circuit (many ESP32/Arduino dev kits) don't
                     reboot when the port opens.
        """
        self.ser = serial.Serial(port, baud, timeout=0.1, rtscts=False, dsrdtr=False)
        if not auto_reset:
            self.ser.dtr = False
            self.ser.rts = False

        time.sleep(2.0)  # let the board's serial come up

        self.LAYOUT = dict(layout) if layout else {}
        self.ID_TO_NAME = {v: k for k, v in self.LAYOUT.items()}

        self.packet_callback = None  # called with dict {name_or_id: value}
        self._rx_buf = bytearray()
        self._running = True
        self._reader = threading.Thread(target=self._read_loop, daemon=True)
        self._reader.start()

    # -- key resolution -----------------------------------------------------
    def _resolve_id(self, name_or_id):
        if isinstance(name_or_id, int):
            return name_or_id
        return self.LAYOUT.get(name_or_id)

    # -- transmit -----------------------------------------------------------
    def send_value(self, name_or_id, value):
        """Send a single field. Type is inferred from the Python value."""
        wire_id = self._resolve_id(name_or_id)
        if wire_id is not None:
            self._send_raw_packet({wire_id: value})

    def send_fields(self, dataset):
        """Send several fields in one frame. Keys may be names or ints."""
        by_id = {}
        for k, v in dataset.items():
            wid = self._resolve_id(k)
            if wid is not None:
                by_id[wid] = v
        if by_id:
            self._send_raw_packet(by_id)

    def _encode_varint(self, value):
        out = bytearray()
        while value >= 0x80:
            out.append((value & 0x7F) | 0x80)
            value >>= 7
        out.append(value & 0x7F)
        return out

    def _zigzag(self, val):
        return ((val << 1) ^ (val >> 63)) & 0xFFFF_FFFF_FFFF_FFFF

    def _encode_payload(self, val):
        # bool must be checked before int (bool subclasses int in Python)
        if isinstance(val, bool):
            return bytearray([TYPE_BOOL]) + self._encode_varint(self._zigzag(1 if val else 0))
        if isinstance(val, int):
            return bytearray([TYPE_INT]) + self._encode_varint(self._zigzag(val))
        if isinstance(val, float):
            return bytearray([TYPE_FLOAT]) + struct.pack("<f", val)
        if isinstance(val, str):
            return bytearray([TYPE_STRING]) + val.encode("utf-8")[:95]
        raise TypeError(f"Unsupported type: {type(val)}")

    def _send_raw_packet(self, dataset):
        raw = bytearray()
        for f_id, val in dataset.items():
            raw.extend(self._encode_varint((f_id << 3) | 2))
            payload = self._encode_payload(val)
            raw.extend(self._encode_varint(len(payload)))
            raw.extend(payload)
        encoded = self._cobs_encode(raw)
        encoded.append(0x00)
        self.ser.write(encoded)
        self.ser.flush()

    def _cobs_encode(self, data):
        res = bytearray([1])
        code, code_idx = 1, 0
        for b in data:
            if b == 0x00:
                res[code_idx] = code
                code, code_idx = 1, len(res)
                res.append(1)
            else:
                res.append(b)
                code += 1
                if code == 0xFF:
                    res[code_idx] = code
                    code, code_idx = 1, len(res)
                    res.append(1)
        res[code_idx] = code
        return res

    # -- receive ------------------------------------------------------------
    def _read_loop(self):
        while self._running:
            try:
                chunk = self.ser.read(256)
            except (serial.SerialException, OSError):
                break
            for b in chunk:
                if b == 0x00:
                    if self._rx_buf:
                        decoded = self._cobs_decode(self._rx_buf)
                        if decoded is not None:
                            fields = self._parse_packet(decoded)
                            if fields and self.packet_callback:
                                self.packet_callback(fields)
                    self._rx_buf.clear()
                else:
                    self._rx_buf.append(b)

    def _cobs_decode(self, data):
        out = bytearray()
        r = 0
        while r < len(data):
            code = data[r]
            if code == 0 or r + code > len(data) + 1:
                return None
            r += 1
            for _ in range(code - 1):
                if r >= len(data):
                    return None
                out.append(data[r])
                r += 1
            if code != 0xFF and r < len(data):
                out.append(0x00)
        return out

    def _decode_varint(self, buf, offset):
        result, shift = 0, 0
        for i in range(10):
            if offset + i >= len(buf):
                return None, offset
            b = buf[offset + i]
            result |= (b & 0x7F) << shift
            if not (b & 0x80):
                return result, offset + i + 1
            shift += 7
        return None, offset

    def _unzigzag(self, n):
        return (n >> 1) ^ -(n & 1)

    def _parse_packet(self, buf):
        fields = {}
        offset = 0
        while offset < len(buf):
            tag, offset = self._decode_varint(buf, offset)
            if tag is None:
                return fields
            f_id, f_wt = tag >> 3, tag & 0x07
            if f_wt != 2:
                return fields

            chunk_len, offset = self._decode_varint(buf, offset)
            if chunk_len is None or chunk_len < 1 or offset + chunk_len > len(buf):
                return fields

            d_type = buf[offset]
            data = buf[offset + 1: offset + chunk_len]
            offset += chunk_len

            value = None
            if d_type in (TYPE_BOOL, TYPE_INT):
                raw, _ = self._decode_varint(data, 0)
                if raw is not None:
                    value = self._unzigzag(raw)
                    if d_type == TYPE_BOOL:
                        value = bool(value)
            elif d_type == TYPE_FLOAT and len(data) >= 4:
                value = struct.unpack("<f", data[:4])[0]
            elif d_type == TYPE_STRING:
                value = data.decode("utf-8", errors="replace")

            if value is not None:
                fields[self.ID_TO_NAME.get(f_id, f_id)] = value
        return fields

    def close(self):
        self._running = False
        self._reader.join(timeout=1.0)
        self.ser.close()

    # context-manager sugar
    def __enter__(self):
        return self

    def __exit__(self, *exc):
        self.close()
