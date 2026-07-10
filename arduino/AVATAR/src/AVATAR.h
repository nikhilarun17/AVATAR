// AVATAR.h -- lightweight typed serial link for Arduino / ESP32
//
// Full-duplex, self-describing field messaging over serial.
// Each field carries a numeric ID and one of four types:
//   bool, int (64-bit, zigzag varint), float (32-bit), string.
// Framing is COBS with a 0x00 delimiter; the payload is a
// protobuf-style (tag, length, value) stream.
//
// Header-only: all methods are defined in-class (implicitly inline),
// so including this in multiple translation units is safe.
//
// SPDX-License-Identifier: MIT

#ifndef AVATAR_H
#define AVATAR_H

#include <Arduino.h>

enum AVATARType { AVATAR_BOOL = 1, AVATAR_INT = 2, AVATAR_FLOAT = 3, AVATAR_STRING = 4 };

struct AVATARField {
    uint32_t id;
    AVATARType  type;
    int64_t  int_val;
    float    float_val;
    char     str_val[96];
};

class AVATAR {
public:
    static const size_t MAX_FIELDS   = 60;
    static const size_t MAX_RAW_LEN  = 512;
    static const size_t MAX_COBS_LEN = (MAX_RAW_LEN + (MAX_RAW_LEN / 254) + 2);

    AVATARField rx_fields[MAX_FIELDS];
    size_t   rx_count = 0;
    AVATARField tx_fields[MAX_FIELDS];
    size_t   tx_count = 0;

    void (*packetCallback)() = nullptr;

    void begin(unsigned long baud = 115200) { Serial.begin(baud); }
    void setCallback(void (*callback)()) { packetCallback = callback; }

    // ---- Receive-side accessors -------------------------------------------
    bool hasField(uint32_t id) {
        for (size_t i = 0; i < rx_count; i++) if (rx_fields[i].id == id) return true;
        return false;
    }
    bool getBool(uint32_t id) {
        for (size_t i = 0; i < rx_count; i++)
            if (rx_fields[i].id == id && rx_fields[i].type == AVATAR_BOOL) return rx_fields[i].int_val != 0;
        return false;
    }
    int64_t getInt(uint32_t id) {
        for (size_t i = 0; i < rx_count; i++)
            if (rx_fields[i].id == id && rx_fields[i].type == AVATAR_INT) return rx_fields[i].int_val;
        return 0;
    }
    float getFloat(uint32_t id) {
        for (size_t i = 0; i < rx_count; i++)
            if (rx_fields[i].id == id && rx_fields[i].type == AVATAR_FLOAT) return rx_fields[i].float_val;
        return 0.0f;
    }
    const char* getString(uint32_t id) {
        for (size_t i = 0; i < rx_count; i++)
            if (rx_fields[i].id == id && rx_fields[i].type == AVATAR_STRING) return rx_fields[i].str_val;
        return "";
    }

    // ---- Transmit-side builders -------------------------------------------
    void clearTx() { tx_count = 0; }
    void addBool(uint32_t id, bool v)    { if (tx_count < MAX_FIELDS) tx_fields[tx_count++] = {id, AVATAR_BOOL,  v ? 1 : 0, 0.0f, ""}; }
    void addInt(uint32_t id, int64_t v)  { if (tx_count < MAX_FIELDS) tx_fields[tx_count++] = {id, AVATAR_INT,   v,         0.0f, ""}; }
    void addFloat(uint32_t id, float v)  { if (tx_count < MAX_FIELDS) tx_fields[tx_count++] = {id, AVATAR_FLOAT, 0,         v,    ""}; }
    void addString(uint32_t id, const char* t) {
        if (tx_count < MAX_FIELDS) {
            AVATARField fd = {id, AVATAR_STRING, 0, 0.0f, ""};
            strncpy(fd.str_val, t, sizeof(fd.str_val) - 1);
            fd.str_val[sizeof(fd.str_val) - 1] = '\0';
            tx_fields[tx_count++] = fd;
        }
    }

    void sendTx() {
        uint8_t raw[MAX_RAW_LEN];
        size_t pos = 0;

        for (size_t i = 0; i < tx_count; i++) {
            uint8_t payload[104];
            size_t p_pos = 0;
            payload[p_pos++] = (uint8_t)tx_fields[i].type;

            if (tx_fields[i].type == AVATAR_BOOL || tx_fields[i].type == AVATAR_INT) {
                p_pos += pb_encode_varint(payload + p_pos, zigzag_from_int(tx_fields[i].int_val));
            } else if (tx_fields[i].type == AVATAR_FLOAT) {
                memcpy(payload + p_pos, &tx_fields[i].float_val, 4);
                p_pos += 4;
            } else if (tx_fields[i].type == AVATAR_STRING) {
                size_t slen = strlen(tx_fields[i].str_val);
                memcpy(payload + p_pos, tx_fields[i].str_val, slen);
                p_pos += slen;
            }

            uint8_t tag_buf[5];
            size_t tag_len = pb_encode_varint(tag_buf, ((uint64_t)tx_fields[i].id << 3) | 2);
            uint8_t len_buf[5];
            size_t len_len = pb_encode_varint(len_buf, (uint64_t)p_pos);

            if (pos + tag_len + len_len + p_pos > MAX_RAW_LEN) break;

            memcpy(raw + pos, tag_buf, tag_len); pos += tag_len;
            memcpy(raw + pos, len_buf, len_len); pos += len_len;
            memcpy(raw + pos, payload, p_pos);   pos += p_pos;
        }

        uint8_t enc[MAX_COBS_LEN];
        size_t enc_len = encode_cobs(raw, pos, enc);
        Serial.write(enc, enc_len);
        Serial.write((uint8_t)0x00);
        Serial.flush();
    }

    void update() {
        while (Serial.available() > 0) {
            uint8_t b = (uint8_t)Serial.read();
            if (b == 0x00) {
                if (rx_raw_len > 0) {
                    uint8_t dec[MAX_RAW_LEN];
                    size_t dec_len = decode_cobs(rx_raw_buffer, rx_raw_len, dec);
                    if (dec_len > 0) parse_packet(dec, dec_len);
                }
                rx_raw_len = 0;
                continue;
            }
            if (rx_raw_len < MAX_COBS_LEN) rx_raw_buffer[rx_raw_len++] = b;
            else rx_raw_len = 0;   // overflow: drop, resync on next delimiter
        }
    }

private:
    uint8_t rx_raw_buffer[MAX_COBS_LEN];
    size_t  rx_raw_len = 0;

    int64_t  zigzag_to_int(uint64_t n)  { return (int64_t)(n >> 1) ^ -(int64_t)(n & 1); }
    uint64_t zigzag_from_int(int64_t n) { return (uint64_t)((n << 1) ^ (n >> 63)); }

    size_t pb_encode_varint(uint8_t *buf, uint64_t value) {
        size_t i = 0;
        while (value >= 0x80) { buf[i++] = (uint8_t)((value & 0x7F) | 0x80); value >>= 7; }
        buf[i++] = (uint8_t)(value & 0x7F);
        return i;
    }

    bool pb_decode_varint(const uint8_t *buf, size_t max_len, uint64_t *value, size_t *bytes_read) {
        uint64_t result = 0; size_t shift = 0, i = 0;
        while (true) {
            if (i >= max_len || i >= 10) return false;
            uint8_t byte = buf[i];
            result |= (uint64_t)(byte & 0x7F) << shift; i++;
            if ((byte & 0x80) == 0) break;
            shift += 7;
        }
        *value = result; *bytes_read = i;
        return true;
    }

    size_t encode_cobs(const uint8_t *input, size_t len, uint8_t *output) {
        size_t r = 0, w = 1, code_idx = 0;
        uint8_t code = 1;
        while (r < len) {
            if (input[r] == 0x00) {
                output[code_idx] = code; code = 1; code_idx = w++; r++;
            } else {
                output[w++] = input[r++]; code++;
                if (code == 0xFF) { output[code_idx] = code; code = 1; code_idx = w++; }
            }
        }
        output[code_idx] = code;
        return w;
    }

    size_t decode_cobs(const uint8_t *input, size_t len, uint8_t *output) {
        size_t r = 0, w = 0;
        while (r < len) {
            uint8_t code = input[r];
            if (code == 0) return 0;
            r++;
            for (uint8_t i = 1; i < code; i++) {
                if (r >= len) return 0;
                output[w++] = input[r++];
            }
            if (code != 0xFF && r < len) output[w++] = 0x00;
        }
        return w;
    }

    void parse_packet(uint8_t *buf, size_t size) {
        size_t offset = 0;
        rx_count = 0;
        while (offset < size && rx_count < MAX_FIELDS) {
            uint64_t tag_val; size_t bytes_read;
            if (!pb_decode_varint(buf + offset, size - offset, &tag_val, &bytes_read)) return;
            offset += bytes_read;

            uint32_t f_num = (uint32_t)(tag_val >> 3);
            uint32_t f_wt  = (uint32_t)(tag_val & 0x07);
            if (f_wt != 2) return;

            uint64_t chunk_len;
            if (!pb_decode_varint(buf + offset, size - offset, &chunk_len, &bytes_read)) return;
            offset += bytes_read;

            if (chunk_len < 1 || offset + chunk_len > size) return;

            AVATARType d_type = (AVATARType)buf[offset];
            AVATARField fd = {f_num, d_type, 0, 0.0f, ""};

            size_t data_offset = offset + 1;
            size_t data_len = (size_t)chunk_len - 1;

            if (d_type == AVATAR_BOOL || d_type == AVATAR_INT) {
                uint64_t raw_int = 0;
                if (pb_decode_varint(buf + data_offset, data_len, &raw_int, &bytes_read))
                    fd.int_val = zigzag_to_int(raw_int);
            } else if (d_type == AVATAR_FLOAT) {
                if (data_len >= 4) memcpy(&fd.float_val, buf + data_offset, 4);
            } else if (d_type == AVATAR_STRING) {
                size_t c_len = (data_len > sizeof(fd.str_val) - 1) ? sizeof(fd.str_val) - 1 : data_len;
                memcpy(fd.str_val, buf + data_offset, c_len);
                fd.str_val[c_len] = '\0';
            }

            rx_fields[rx_count++] = fd;
            offset += chunk_len;
        }
        if (rx_count > 0 && packetCallback != nullptr) packetCallback();
    }
};

#endif // AVATAR_H
