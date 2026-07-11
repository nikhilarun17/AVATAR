#include <AVATAR.h>

/*

Written by: Nikhil and Nharen
Date: 11 Jul, 2026

Description:
AVATAR based 6-motor drive controller for ESP32-S3 with indicator

Features:
- Receives (host -> board):
    IDs 2..7  -> motor PWM, int, -127..127 (negative = reverse)
    ID  8     -> relay state, int (currently unused)
- Echoes (board -> host):
    IDs 12..17 -> motor PWM echo
- Controls 6 motors using PWM + direction pins
- Watchdog safety stop if drive messages are not received
- RGB status indication:
    Green  -> Normal operation
    Red    -> Watchdog triggered (no drive messages)

Hardware Mapping:

Motor         PWM Pin     DIR Pin
---------------------------------
1             36          37
2             38          39
3             40          41
4             42          2
5             4           5
6             6           7

Notes:
- PWM input range: -127 to 127
- Negative values reverse motor direction
- Motors stop automatically on watchdog timeout; recovers to green
  automatically when messages resume

*/

#define NUM_MOTORS 6
#define MAX_PWM_VALUE 127
#define WATCHDOG_TRIGGER_TIME_IN_S 1
#define INDICATOR_RED_GPIO 48
#define RELAY_BLINK_MS 1000

static const int PWMpins[NUM_MOTORS] = {36,38,40,42,4,6};
static const int DIRpins[NUM_MOTORS] = {37,39,41,2,5,7};
static const int motor_directions[NUM_MOTORS] = {0,0,0,0,0,0};

int32_t drive_buffer[NUM_MOTORS] = {0};
int32_t relay_state_buffer = 0;
int32_t echo_drive_buffer[NUM_MOTORS] = {0};

AVATAR drive_link;

namespace Wire {
    const int32_t FR         = 2;
    const int32_t FL         = 3;
    const int32_t MR         = 4;
    const int32_t ML         = 5;
    const int32_t RR         = 6;
    const int32_t RL         = 7;
    const int32_t RELAY      = 8;

    const int32_t FR_echo    = 12;
    const int32_t FL_echo    = 13;
    const int32_t MR_echo    = 14;
    const int32_t ML_echo    = 15;
    const int32_t RR_echo    = 16;
    const int32_t RL_echo    = 17;
    const int32_t RELAY_echo = 18;
}

uint32_t last_msg_time = 0;

void transmit() {
    drive_link.clearTx();
    for (int i = 12; i < 18; i++) {
        drive_link.addInt(i, drive_buffer[i - 12]);
    }
    drive_link.sendTx();
}

void drive_callback() {
    last_msg_time = millis();
    if (drive_link.hasField(Wire::RELAY)) {
        relay_state_buffer = drive_link.getInt(Wire::RELAY);
    }
    for (int i = 2; i <= 7; i++) {
        if (drive_link.hasField(i)) {
            drive_buffer[i - 2] = drive_link.getInt(i);
        }
    }
    transmit();
}

void setup() {
    for (int i = 0; i < NUM_MOTORS; i++) {
        pinMode(PWMpins[i], OUTPUT);
        pinMode(DIRpins[i], OUTPUT);
        analogWrite(PWMpins[i], 0);
        digitalWrite(DIRpins[i], 0);
    }
    pinMode(INDICATOR_RED_GPIO, OUTPUT);
    digitalWrite(INDICATOR_RED_GPIO, 0);

    drive_link.begin(115200);
    drive_link.setCallback(drive_callback);
    last_msg_time = millis();
}

void loop() {
    drive_link.update();

    if (millis() - last_msg_time < (WATCHDOG_TRIGGER_TIME_IN_S * 1000)) {
        for (int i = 0; i < NUM_MOTORS; i++) {
            int pwm_input = constrain(abs(drive_buffer[i]), 0, MAX_PWM_VALUE);
            analogWrite(PWMpins[i], pwm_input);
            digitalWrite(DIRpins[i], drive_buffer[i] >= 0 ? motor_directions[i] : !motor_directions[i]);
        }
        neopixelWrite(RGB_BUILTIN, 0, 100, 0);
    }
    else {
        for (int i = 0; i < NUM_MOTORS; i++) {
            drive_buffer[i] = 0;
            analogWrite(PWMpins[i], 0);
            digitalWrite(DIRpins[i], motor_directions[i]);
        }
        neopixelWrite(RGB_BUILTIN, 150, 0, 0);
    }
}