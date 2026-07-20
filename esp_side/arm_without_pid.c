#include <AVATAR.h>

#define NUM_MOT 6
#define MAX_PWM 255
#define WATCHDOG_TRIGGER_TIME_IN_S 1

int pwm_pins[NUM_MOT] = {2, 41, 6, 4, 15, 17};
int dir_pins[NUM_MOT] = {1, 42, 7, 5, 16, 18};
static const int dir_dir[NUM_MOT] = {1, 1, 1, 1, 1, 1};

float target_data[NUM_MOT] = {0};   // raw PWM per motor, -255..255

uint32_t last_msg_time = 0;
uint32_t last_tel_time = 0;

AVATAR arm_link;

namespace Wire {
    const int32_t TARGET_START = 2;   // 2..7  = per-motor PWM command (float)
    const int32_t ECHO_START   = 12;  // 12..17 = applied PWM echo (float)
}

inline void drive_pwm(int i, float raw) {
    int val = (int)raw;
    int pwm = constrain(abs(val), 0, MAX_PWM);
    int dir = (val >= 0) ? dir_dir[i] : !dir_dir[i];
    digitalWrite(dir_pins[i], dir);
    analogWrite(pwm_pins[i], pwm);
}

void transmit_telemetry() {
    arm_link.clearTx();
    for (int i = 0; i < NUM_MOT; i++)
        arm_link.addFloat(Wire::ECHO_START + i, target_data[i]);
    arm_link.sendTx();
}

void arm_callback() {
    last_msg_time = millis();
    for (int i = 0; i < NUM_MOT; i++) {
        int id = Wire::TARGET_START + i;
        if (arm_link.hasField(id))
            target_data[i] = arm_link.getFloat(id);
    }
    transmit_telemetry();
}

void setup() {
    for (int i = 0; i < NUM_MOT; i++) {
        pinMode(pwm_pins[i], OUTPUT);
        pinMode(dir_pins[i], OUTPUT);
        analogWrite(pwm_pins[i], 0);
        digitalWrite(dir_pins[i], dir_dir[i]);
    }
    arm_link.begin(115200);
    arm_link.setCallback(arm_callback);
    last_msg_time = millis();
    last_tel_time = millis();
}

void loop() {
    arm_link.update();
    uint32_t now = millis();

    if (now - last_tel_time >= 20) {
        last_tel_time = now;

        if (now - last_msg_time < (WATCHDOG_TRIGGER_TIME_IN_S * 1000)) {
            for (int i = 0; i < NUM_MOT; i++)
                drive_pwm(i, target_data[i]);      // always direct PWM, no gate
            neopixelWrite(RGB_BUILTIN, 0, 100, 0);
        } else {
            for (int i = 0; i < NUM_MOT; i++) {
                target_data[i] = 0;
                drive_pwm(i, 0);
            }
            neopixelWrite(RGB_BUILTIN, 150, 0, 0);
        }
    }
}