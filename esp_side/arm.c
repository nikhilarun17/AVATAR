#include <AVATAR.h>

#define NUM_MOT 6
#define MAX_PWM 255
#define WATCHDOG_TRIGGER_TIME_IN_S 1

int encA_pins[NUM_MOT] = {39, 48, 21, 14, 12, 10};
int encB_pins[NUM_MOT] = {40, 38, 47, 13, 11, 9};
int pwm_pins[NUM_MOT] = {2, 41, 6, 4, 15, 17};     
int dir_pins[NUM_MOT] = {1, 42, 7, 5, 16, 18};
static const int dir_dir[NUM_MOT] = {1, 1, 1, 1, 1, 1};

volatile int last_a[NUM_MOT] = {0};
volatile int last_b[NUM_MOT] = {0};
volatile long enc_count[NUM_MOT] = {0};

float current_angle[NUM_MOT] = {0};
float target_data[NUM_MOT] = {0};

static const float enc_to_angle_factor[NUM_MOT] = {
    180.0f / 143082.5f,
    90.0f / 390668.0f,
    90.0f / 64735.3f,
    90.0f / 55157.0f,
    90.0f / 55157.0f,
    90.0f / 55157.0f
};

float Kp[NUM_MOT] = {2.0f, 2.0f, 2.0f, 2.0f, 2.0f, 2.0f};
float Ki[NUM_MOT] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
float Kd[NUM_MOT] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};

float integral[NUM_MOT]   = {0};
float prev_error[NUM_MOT] = {0};

volatile bool input_space = false;
volatile bool arm_state   = false;

uint32_t last_msg_time = 0;
uint32_t last_pid_time = 0;

AVATAR arm_link;

namespace Wire {
    const int32_t TARGET_START = 2;
    const int32_t INPUT_SPACE  = 8;
    const int32_t ARM_STATE    = 9;
    const int32_t ECHO_START   = 12;
    const int32_t ENC_START    = 22;
    const int32_t ANGLE_START  = 32;
}

inline bool joint_is_pwm(int i) {
    if (arm_state) return false;
    if (!input_space) return true;
    return (i >= 3);
}

inline void drive_pwm(int i, float raw) {
    int val = (int)raw;
    int pwm = constrain(abs(val), 0, MAX_PWM);
    int dir = (val >= 0) ? HIGH : LOW;
    digitalWrite(dir_pins[i], dir);
    analogWrite(pwm_pins[i], pwm);
}

inline void updateEncoder(int i) {
    int a = digitalRead(encA_pins[i]);
    int b = digitalRead(encB_pins[i]);
    int last_a_state = last_a[i];
    if (a != last_a_state || b != last_b[i]) {
        if (last_a_state == a) {
            enc_count[i] += (a ^ b) ? -1 : +1;
        } else {
            enc_count[i] += (a ^ b) ? +1 : -1;
        }
        last_a[i] = a;
        last_b[i] = b;
    }
}

void IRAM_ATTR isr0A() {updateEncoder(0);} void IRAM_ATTR isr0B() {updateEncoder(0);}
void IRAM_ATTR isr1A() {updateEncoder(1);} void IRAM_ATTR isr1B() {updateEncoder(1);}
void IRAM_ATTR isr2A() {updateEncoder(2);} void IRAM_ATTR isr2B() {updateEncoder(2);}
void IRAM_ATTR isr3A() {updateEncoder(3);} void IRAM_ATTR isr3B() {updateEncoder(3);}
void IRAM_ATTR isr4A() {updateEncoder(4);} void IRAM_ATTR isr4B() {updateEncoder(4);}
void IRAM_ATTR isr5A() {updateEncoder(5);} void IRAM_ATTR isr5B() {updateEncoder(5);}

void transmit_telemetry() {
    arm_link.clearTx();
    for (int i = 0; i < NUM_MOT; i++) {
        arm_link.addFloat(Wire::ECHO_START + i, target_data[i]);
    }
    for (int i = 0; i < NUM_MOT; i++) {
        noInterrupts();
        long count = enc_count[i];
        interrupts();
        arm_link.addInt(Wire::ENC_START + i, (int32_t)count);
    }
    for (int i = 0; i < NUM_MOT; i++) {
        arm_link.addFloat(Wire::ANGLE_START + i, current_angle[i]);
    }
    arm_link.sendTx();
}

void arm_callback() {
    last_msg_time = millis();
    if (arm_link.hasField(Wire::INPUT_SPACE)) {
        input_space = (arm_link.getInt(Wire::INPUT_SPACE) == 1);
    }
    if (arm_link.hasField(Wire::ARM_STATE)) {
        arm_state = (arm_link.getInt(Wire::ARM_STATE) == 1);
    }
    for (int i = 0; i < NUM_MOT; i++) {
        int id = Wire::TARGET_START + i;
        if (arm_link.hasField(id)) {
            target_data[i] = arm_link.getFloat(id);
        }
    }
    for (int i = 0; i < NUM_MOT; i++) {
        if (!joint_is_pwm(i)) {
            integral[i]   = 0;
            prev_error[i] = 0;
        }
    }
    transmit_telemetry();
}

void run_pid_and_drive() {
    for (int i = 0; i < NUM_MOT; i++) {
        noInterrupts();
        long count = enc_count[i];
        interrupts();
        current_angle[i] = count * enc_to_angle_factor[i];
        if (joint_is_pwm(i)) {
            drive_pwm(i, target_data[i]);
        } else {
            float error = target_data[i] - current_angle[i];
            integral[i] += error * 0.02f;
            integral[i]  = constrain(integral[i], -1000, 1000);
            float derivative = (error - prev_error[i]) / 0.02f;
            float output     = Kp[i] * error + Ki[i] * integral[i] + Kd[i] * derivative;
            prev_error[i] = error;
            drive_pwm(i, output);
        }
    }
}

void setupEncoders() {
    for (int i = 0; i < NUM_MOT; i++) {
        pinMode(encA_pins[i], INPUT_PULLUP);
        pinMode(encB_pins[i], INPUT_PULLUP);
        last_a[i] = digitalRead(encA_pins[i]);
        last_b[i] = digitalRead(encB_pins[i]);
    }
    attachInterrupt(digitalPinToInterrupt(encA_pins[0]), isr0A, CHANGE); attachInterrupt(digitalPinToInterrupt(encB_pins[0]), isr0B, CHANGE);
    attachInterrupt(digitalPinToInterrupt(encA_pins[1]), isr1A, CHANGE); attachInterrupt(digitalPinToInterrupt(encB_pins[1]), isr1B, CHANGE);
    attachInterrupt(digitalPinToInterrupt(encA_pins[2]), isr2A, CHANGE); attachInterrupt(digitalPinToInterrupt(encB_pins[2]), isr2B, CHANGE);
    attachInterrupt(digitalPinToInterrupt(encA_pins[3]), isr3A, CHANGE); attachInterrupt(digitalPinToInterrupt(encB_pins[3]), isr3B, CHANGE);
    attachInterrupt(digitalPinToInterrupt(encA_pins[4]), isr4A, CHANGE); attachInterrupt(digitalPinToInterrupt(encB_pins[4]), isr4B, CHANGE);
    attachInterrupt(digitalPinToInterrupt(encA_pins[5]), isr5A, CHANGE); attachInterrupt(digitalPinToInterrupt(encB_pins[5]), isr5B, CHANGE);
}

void setup() {
    for (int i = 0; i < NUM_MOT; i++) {
        pinMode(pwm_pins[i], OUTPUT);
        pinMode(dir_pins[i], OUTPUT);
        analogWrite(pwm_pins[i], 0);
        digitalWrite(dir_pins[i], dir_dir[i]);
    }
    setupEncoders();
    arm_link.begin(115200);
    arm_link.setCallback(arm_callback);
    last_msg_time = millis();
    last_pid_time = millis();
}

void loop() {
    arm_link.update();
    uint32_t now = millis();
    if (now - last_pid_time >= 20) {
        last_pid_time = now;
        if (now - last_msg_time < (WATCHDOG_TRIGGER_TIME_IN_S * 1000)) {
            run_pid_and_drive();
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
