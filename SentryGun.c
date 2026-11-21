//          CODUL PENTRU MOTORUL PAS CU PAS 28BYJ-48, cursa de +/- 200 grade, 
//          rfid controleaza parcarea la 0 grade inainte de oprire cu cardul alb
#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"

// Pini RFID (SPI0)
#define SPI_PORT spi0
#define PIN_SCK  2
#define PIN_MOSI 3
#define PIN_MISO 4
#define PIN_CS   5
#define PIN_RST  6

// Pini Motor pas cu pas (28BYJ-48)
#define IN1 16
#define IN2 17
#define IN3 18
#define IN4 19

#define MAX_STEPS     1138 // cam 200 de grade
#define STEP_DELAY_MS 4

// Registri MFRC522
#define CommandReg     0x01
#define ComIrqReg      0x04
#define ErrorReg       0x06
#define FIFODataReg    0x09
#define FIFOLevelReg   0x0A
#define BitFramingReg  0x0D
#define ModeReg        0x11
#define TxControlReg   0x14
#define TxASKReg       0x15
#define TModeReg       0x2A
#define TPrescalerReg  0x2B
#define TReloadRegH    0x2C
#define TReloadRegL    0x2D

#define PCD_IDLE       0x00
#define PCD_TRANSCEIVE 0x0C
#define PCD_RESETPHASE 0x0F

// Carduri RFID
const uint8_t AUTH_UID[4] = {0xD9, 0x3B, 0xA5, 0x29}; // Reluare patrulare - cardul albastru
const uint8_t PARK_UID[4] = {0x50, 0x29, 0x19, 0x58}; // parcare la 0 grade - cardul alb

// Variabile Motor
const int step_seq[4][4] = {
    {1, 1, 0, 0},
    {0, 1, 1, 0},
    {0, 0, 1, 1},
    {1, 0, 0, 1}
};

int step_index  = 0;
int current_pos = 0;
int direction   = 1;

// FUNCȚII MOTOR
void motor_step(int dir) {
    step_index = (step_index + dir + 4) % 4;
    gpio_put(IN1, step_seq[step_index][0]);
    gpio_put(IN2, step_seq[step_index][1]);
    gpio_put(IN3, step_seq[step_index][2]);
    gpio_put(IN4, step_seq[step_index][3]);
}

void motor_release() {
    gpio_put(IN1, 0);
    gpio_put(IN2, 0);
    gpio_put(IN3, 0);
    gpio_put(IN4, 0);
}

void step_patrol() {
    motor_step(direction);
    current_pos += direction;

    if (current_pos >= MAX_STEPS) {
        direction = -1;
    } else if (current_pos <= -MAX_STEPS) {
        direction = 1;
    }
}

void park_motor() {
    printf("\n parcare motor la 0 grade (de la pasul %d)...\n", current_pos);
    while (current_pos != 0) {
        int dir = (current_pos > 0) ? -1 : 1;
        motor_step(dir);
        current_pos += dir;
        sleep_ms(STEP_DELAY_MS);
    }
    direction = 1;
    motor_release();
    printf(" motorul este la 0 grade. Apropie cardul albastru pentru reluare.\n");
}

// FUNCȚII RFID
void rc522_write(uint8_t reg, uint8_t val) {
    uint8_t buf[2] = { (uint8_t)((reg << 1) & 0x7E), val };
    gpio_put(PIN_CS, 0);
    spi_write_blocking(SPI_PORT, buf, 2);
    gpio_put(PIN_CS, 1);
}

uint8_t rc522_read(uint8_t reg) {
    uint8_t addr = ((reg << 1) & 0x7E) | 0x80;
    uint8_t val = 0;
    gpio_put(PIN_CS, 0);
    spi_write_blocking(SPI_PORT, &addr, 1);
    spi_read_blocking(SPI_PORT, 0x00, &val, 1);
    gpio_put(PIN_CS, 1);
    return val;
}

void rc522_init() {
    rc522_write(CommandReg, PCD_RESETPHASE);
    sleep_ms(50);

    rc522_write(TModeReg, 0x8D);
    rc522_write(TPrescalerReg, 0x3E);
    rc522_write(TReloadRegL, 30);
    rc522_write(TReloadRegH, 0);
    rc522_write(TxASKReg, 0x40);
    rc522_write(ModeReg, 0x3D);

    uint8_t temp = rc522_read(TxControlReg);
    if (!(temp & 0x03)) {
        rc522_write(TxControlReg, temp | 0x03);
    }
}

bool rc522_transceive(uint8_t *send, uint8_t sendLen, uint8_t *back, uint8_t *backLen, uint8_t bitFraming) {
    rc522_write(CommandReg, PCD_IDLE);
    rc522_write(ComIrqReg, 0x7F);
    rc522_write(FIFOLevelReg, 0x80);

    for (uint8_t i = 0; i < sendLen; i++) {
        rc522_write(FIFODataReg, send[i]);
    }

    rc522_write(BitFramingReg, bitFraming);
    rc522_write(CommandReg, PCD_TRANSCEIVE);
    rc522_write(BitFramingReg, bitFraming | 0x80);

    int i = 2000;
    uint8_t n;
    do {
        n = rc522_read(ComIrqReg);
        i--;
    } while ((i != 0) && !(n & 0x30));

    rc522_write(BitFramingReg, bitFraming & ~0x80);

    if (i == 0 || (rc522_read(ErrorReg) & 0x1B)) {
        return false;
    }

    uint8_t len = rc522_read(FIFOLevelReg);
    if (len > *backLen) len = *backLen;
    *backLen = len;

    for (uint8_t j = 0; j < len; j++) {
        back[j] = rc522_read(FIFODataReg);
    }
    return true;
}

bool rc522_read_uid(uint8_t *uid) {
    uint8_t req = 0x26;
    uint8_t back[16];
    uint8_t backLen = sizeof(back);

    if (!rc522_transceive(&req, 1, back, &backLen, 0x07)) {
        return false;
    }

    uint8_t anticoll[2] = { 0x93, 0x20 };
    backLen = sizeof(back);
    if (!rc522_transceive(anticoll, 2, back, &backLen, 0x00) || backLen < 5) {
        return false;
    }

    for (int i = 0; i < 4; i++) {
        uid[i] = back[i];
    }
    return true;
}

// MAIN
int main() {
    stdio_init_all();

    int motor_pins[4] = {IN1, IN2, IN3, IN4};
    for (int i = 0; i < 4; i++) {
        gpio_init(motor_pins[i]);
        gpio_set_dir(motor_pins[i], GPIO_OUT);
        gpio_put(motor_pins[i], 0);
    }

    spi_init(SPI_PORT, 1000 * 1000);
    gpio_set_function(PIN_SCK, GPIO_FUNC_SPI);
    gpio_set_function(PIN_MOSI, GPIO_FUNC_SPI);
    gpio_set_function(PIN_MISO, GPIO_FUNC_SPI);

    gpio_init(PIN_CS);
    gpio_set_dir(PIN_CS, GPIO_OUT);
    gpio_put(PIN_CS, 1);

    gpio_init(PIN_RST);
    gpio_set_dir(PIN_RST, GPIO_OUT);
    gpio_put(PIN_RST, 1);

    sleep_ms(3000);
    rc522_init();
    printf("Test Motor + Card Parcare (50 29 19 58) pornit!\n");

    uint8_t uid[4];
    bool is_parked = false;
    uint32_t last_rfid_check = 0;

    while (true) {
        if (is_parked) {
            if (rc522_read_uid(uid) && memcmp(uid, AUTH_UID, 4) == 0) {
                printf("\n[RELUARE] Pornesc patrularea de la 0 grade...\n");
                is_parked = false;
                sleep_ms(1000);
            }
            sleep_ms(100);
            continue;
        }

        uint32_t now = to_ms_since_boot(get_absolute_time());
        if (now - last_rfid_check >= 200) {
            last_rfid_check = now;
            if (rc522_read_uid(uid) && memcmp(uid, PARK_UID, 4) == 0) {
                park_motor();
                is_parked = true;
                continue;
            }
        }

        step_patrol();
        sleep_ms(STEP_DELAY_MS);
    }
}



//    CODUL PENTRU SENZORII PIR + RFID + BUZZER
/*
#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"

// Pini RFID (SPI0)
#define SPI_PORT spi0
#define PIN_SCK  2
#define PIN_MOSI 3
#define PIN_MISO 4
#define PIN_CS   5
#define PIN_RST  6

// Pin Buzzer
#define BUZZER_PIN 21

// Pini Senzori PIR
const uint PIR_PINS[4] = {12, 13, 14, 15};

// Registri MFRC522
#define CommandReg     0x01
#define ComIrqReg      0x04
#define ErrorReg       0x06
#define FIFODataReg    0x09
#define FIFOLevelReg   0x0A
#define BitFramingReg  0x0D
#define ModeReg        0x11
#define TxControlReg   0x14
#define TxASKReg       0x15
#define TModeReg       0x2A
#define TPrescalerReg  0x2B
#define TReloadRegH    0x2C
#define TReloadRegL    0x2D

#define PCD_IDLE       0x00
#define PCD_TRANSCEIVE 0x0C
#define PCD_RESETPHASE 0x0F

// Cardul autorizat
const uint8_t AUTH_UID[4] = {0xD9, 0x3B, 0xA5, 0x29};

void rc522_write(uint8_t reg, uint8_t val) {
    uint8_t buf[2] = { (uint8_t)((reg << 1) & 0x7E), val };
    gpio_put(PIN_CS, 0);
    spi_write_blocking(SPI_PORT, buf, 2);
    gpio_put(PIN_CS, 1);
}

uint8_t rc522_read(uint8_t reg) {
    uint8_t addr = ((reg << 1) & 0x7E) | 0x80;
    uint8_t val = 0;
    gpio_put(PIN_CS, 0);
    spi_write_blocking(SPI_PORT, &addr, 1);
    spi_read_blocking(SPI_PORT, 0x00, &val, 1);
    gpio_put(PIN_CS, 1);
    return val;
}

void rc522_init() {
    rc522_write(CommandReg, PCD_RESETPHASE);
    sleep_ms(50);

    rc522_write(TModeReg, 0x8D);
    rc522_write(TPrescalerReg, 0x3E);
    rc522_write(TReloadRegL, 30);
    rc522_write(TReloadRegH, 0);
    rc522_write(TxASKReg, 0x40);
    rc522_write(ModeReg, 0x3D);

    uint8_t temp = rc522_read(TxControlReg);
    if (!(temp & 0x03)) {
        rc522_write(TxControlReg, temp | 0x03);
    }
}

bool rc522_transceive(uint8_t *send, uint8_t sendLen, uint8_t *back, uint8_t *backLen, uint8_t bitFraming) {
    rc522_write(CommandReg, PCD_IDLE);
    rc522_write(ComIrqReg, 0x7F);
    rc522_write(FIFOLevelReg, 0x80);

    for (uint8_t i = 0; i < sendLen; i++) {
        rc522_write(FIFODataReg, send[i]);
    }

    rc522_write(BitFramingReg, bitFraming);
    rc522_write(CommandReg, PCD_TRANSCEIVE);
    rc522_write(BitFramingReg, bitFraming | 0x80);

    int i = 2000;
    uint8_t n;
    do {
        n = rc522_read(ComIrqReg);
        i--;
    } while ((i != 0) && !(n & 0x30));

    rc522_write(BitFramingReg, bitFraming & ~0x80);

    if (i == 0 || (rc522_read(ErrorReg) & 0x1B)) {
        return false;
    }

    uint8_t len = rc522_read(FIFOLevelReg);
    if (len > *backLen) len = *backLen;
    *backLen = len;

    for (uint8_t j = 0; j < len; j++) {
        back[j] = rc522_read(FIFODataReg);
    }
    return true;
}

bool rc522_read_uid(uint8_t *uid) {
    uint8_t req = 0x26;
    uint8_t back[16];
    uint8_t backLen = sizeof(back);

    if (!rc522_transceive(&req, 1, back, &backLen, 0x07)) {
        return false;
    }

    uint8_t anticoll[2] = { 0x93, 0x20 };
    backLen = sizeof(back);
    if (!rc522_transceive(anticoll, 2, back, &backLen, 0x00) || backLen < 5) {
        return false;
    }

    for (int i = 0; i < 4; i++) {
        uid[i] = back[i];
    }
    return true;
}

// Verifică toți cei 4 senzori PIR și returnează pinul activat (sau -1 dacă nu e mișcare)
int check_pir_sensors() {
    for (int i = 0; i < 4; i++) {
        if (gpio_get(PIR_PINS[i]) == 1) {
            return PIR_PINS[i];
        }
    }
    return -1;
}

int main() {
    stdio_init_all();

    // Inițializare Buzzer
    gpio_init(BUZZER_PIN);
    gpio_set_dir(BUZZER_PIN, GPIO_OUT);
    gpio_put(BUZZER_PIN, 0);

    // Inițializare Senzori PIR (GP12 - GP15)
    for (int i = 0; i < 4; i++) {
        gpio_init(PIR_PINS[i]);
        gpio_set_dir(PIR_PINS[i], GPIO_IN);
        gpio_pull_down(PIR_PINS[i]);
    }

    // Inițializare SPI0 pentru RFID
    spi_init(SPI_PORT, 1000 * 1000);
    gpio_set_function(PIN_SCK, GPIO_FUNC_SPI);
    gpio_set_function(PIN_MOSI, GPIO_FUNC_SPI);
    gpio_set_function(PIN_MISO, GPIO_FUNC_SPI);

    gpio_init(PIN_CS);
    gpio_set_dir(PIN_CS, GPIO_OUT);
    gpio_put(PIN_CS, 1);

    gpio_init(PIN_RST);
    gpio_set_dir(PIN_RST, GPIO_OUT);
    gpio_put(PIN_RST, 1);

    sleep_ms(3000); // Pauză pentru conectarea Serial Monitor
    rc522_init();
    printf("Sistem gata! Astept miscare pe senzorii PIR (GP12-GP15)...\n");

    uint8_t uid[4];

    //while (true) {
    //    printf("GP12: %d | GP13: %d | GP14: %d | GP15: %d\n",
    //           gpio_get(12), gpio_get(13), gpio_get(14), gpio_get(15));
    //    sleep_ms(300);
    //}

    while (true) {
        int triggered_pin = check_pir_sensors();

        if (triggered_pin != -1) {
            printf("\n--- [MISCARE DETECTATA pe GP%d] Buzzer PORNIT! Ai 4 secunde pentru card ---\n", triggered_pin);
            gpio_put(BUZZER_PIN, 1);

            uint32_t start_time = to_ms_since_boot(get_absolute_time());
            bool authorized = false;

            // Fereastra non-blocking de 4 secunde
            while (to_ms_since_boot(get_absolute_time()) - start_time < 4000) {
                if (rc522_read_uid(uid)) {
                    if (memcmp(uid, AUTH_UID, 4) == 0) {
                        authorized = true;
                        break;
                    } else {
                        printf("Card NEAUTORIZAT (%02X %02X %02X %02X)! Buzzer-ul continua...\n",
                               uid[0], uid[1], uid[2], uid[3]);
                        sleep_ms(400);
                    }
                }
                sleep_ms(50);
            }

            if (authorized) {
                gpio_put(BUZZER_PIN, 0);
                printf(">> Card VALID! Buzzer oprit instant.\n");
            } else {
                printf(">> Timp expirat! Alarma continua inca 3 secunde...\n");
                sleep_ms(3000);
                gpio_put(BUZZER_PIN, 0);
                printf(">> Alarma terminata.\n");
            }

            // Pauză scurtă (cooldown) ca senzorul PIR să revină pe 0 înainte de următoarea detecție
            printf("Cooldown 3 secunde...\n");
            sleep_ms(3000);
            printf("Astept miscare...\n");
        }

        sleep_ms(50);
    }
}*/





