# AVATAR Wire Protocol

A frame is a set of fields. Each field is a `(tag, length, payload)` triple.
The whole frame is COBS-encoded and terminated by a single `0x00` byte.

```
  raw frame  =  field*                         (concatenated)
  field      =  tag  len  payload
  on the wire =  COBS(raw frame)  0x00
```

## Field encoding

| part    | encoding                                             |
|---------|------------------------------------------------------|
| tag     | varint of `(id << 3) | 2`  (wire type 2 = length-delimited) |
| len     | varint: number of bytes in `payload`                 |
| payload | `type_byte` followed by the value bytes              |

`type_byte`:

| value | type   | value bytes                                   |
|-------|--------|-----------------------------------------------|
| 1     | bool   | zigzag varint (0 or 1)                         |
| 2     | int    | zigzag varint (up to 64-bit)                   |
| 3     | float  | 4 bytes, IEEE-754 single, little-endian        |
| 4     | string | UTF-8 bytes (no terminator; length from `len`) |

## Varint

Little-endian base-128. Each byte holds 7 bits; the high bit (0x80) means
"more bytes follow".

## Zigzag

Maps signed to unsigned so small-magnitude negatives stay small:
`zigzag(n) = (n << 1) ^ (n >> 63)`.

## COBS

Consistent Overhead Byte Stuffing removes all `0x00` bytes from the raw frame
so a single `0x00` can delimit frames. Blocks are split at `0xFF` (254 data
bytes), the standard COBS maximum.

## Notes for porting

* IDs are arbitrary `uint32`. Both ends must agree on what each ID means; the
  Python side calls this a `layout` dict, the C++ side uses named constants.
* Only wire type 2 is ever used. A decoder that sees any other wire type
  should treat the frame as corrupt and stop.
* The tag is a **varint**, not a single byte — IDs >= 16 need multiple bytes.
