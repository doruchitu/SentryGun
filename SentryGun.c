
// CODUL PENTRU CEI 4 SENZORI PIR
/*
#include <stdio.h>
#include "pico/stdlib.h"

int main() {
    stdio_init_all();

    const uint LED_PIN = 25;
    
    // Definim un array cu pinii celor 4 senzori PIR
    const uint PIR_PINS[4] = {15, 14, 13, 12};

    // Configurăm LED-ul
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);

    // Configurăm toți pinii senzorilor ca intrări (INPUT)
    for (int i = 0; i < 4; i++) {
        gpio_init(PIR_PINS[i]);
        gpio_set_dir(PIR_PINS[i], GPIO_IN);
    }

    while (true) {
        bool miscare_detectata = false;

        // Verificăm fiecare senzor în parte
        for (int i = 0; i < 4; i++) {
            if (gpio_get(PIR_PINS[i])) {
                miscare_detectata = true;
                break; // Oprim verificarea, am găsit mișcare
            }
        }

        if (miscare_detectata) {
            gpio_put(LED_PIN, 1);
        } else {
            gpio_put(LED_PIN, 0);
        }
        
        sleep_ms(50);
    }
} */


//          CODUL PENTRU MOTORUL PAS CU PAS 28BYJ-48
/*
#include <stdio.h>
#include "pico/stdlib.h"
#include <stdint.h>

int main() {
    stdio_init_all();

    // Definim pinii conectați la IN1, IN2, IN3, IN4
    const uint motor_pins[4] = {16, 17, 18, 19};

    // Configurăm pinii ca ieșiri (OUTPUT)
    for (int i = 0; i < 4; i++) {
        gpio_init(motor_pins[i]);
        gpio_set_dir(motor_pins[i], GPIO_OUT);
    }

    // Secvența clasică de pași (Full-Step) pentru acest motor
    const uint8_t step_sequence[4][4] = {
        {1, 1, 0, 0},
        {0, 1, 1, 0},
        {0, 0, 1, 1},
        {1, 0, 0, 1}
    };

    while (true) {
        // Parcurgem cei 4 pași din secvență
        for (int step = 0; step < 4; step++) {
            // Trimitem semnalul către fiecare pin
            for (int pin = 0; pin < 4; pin++) {
                gpio_put(motor_pins[pin], step_sequence[step][pin]);
            }
            
            // O pauză foarte scurtă între pași. 
            // 2-3 milisecunde este ideal pentru 28BYJ-48.
            sleep_ms(3); 
        }
    }
} */


// COD PENTRU DIODA LASER   
/*
#include <stdio.h>
#include "pico/stdlib.h"

int main() {
    stdio_init_all();

    // Definim pinul la care am conectat laserul
    const uint LASER_PIN = 20;

    // Configurăm pinul ca ieșire (OUTPUT)
    gpio_init(LASER_PIN);
    gpio_set_dir(LASER_PIN, GPIO_OUT);

    while (true) {
        gpio_put(LASER_PIN, 1); // Aprinde laserul
        sleep_ms(2000);         // Ține-l aprins 2 secunde
        
        gpio_put(LASER_PIN, 0); // Stinge laserul
        sleep_ms(1000);         // Ține-l stins 1 secundă
    }
} */


// COD PENTRU BUZZER
/*
#include <stdio.h>
#include "pico/stdlib.h"

#define BUZZER_PIN 21

int main() {
    stdio_init_all();

    gpio_init(BUZZER_PIN);
    gpio_set_dir(BUZZER_PIN, GPIO_OUT);

    while (true) {
        // Buzzer PORNIT (bip)
        gpio_put(BUZZER_PIN, 1);
        sleep_ms(500);

        // Buzzer OPRIT (pauză)
        gpio_put(BUZZER_PIN, 0);
        sleep_ms(500);
    }
} */


#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"

#define SPI_PORT spi0
#define PIN_SCK  2
#define PIN_MOSI 3
#define PIN_MISO 4
#define PIN_CS   5
#define PIN_RST  6

// Registri MFRC522
#define CommandReg     0x01
#define ComIrqReg      0x04
#define ErrorReg       0x06
#define FIFODataReg    0x09
#define FIFOLevelReg   0x0A
#define ControlReg     0x0C
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

    // Pornire antenă
    uint8_t temp = rc522_read(TxControlReg);
    if (!(temp & 0x03)) {
        rc522_write(TxControlReg, temp | 0x03);
    }
}

bool rc522_transceive(uint8_t *send, uint8_t sendLen, uint8_t *back, uint8_t *backLen, uint8_t bitFraming) {
    rc522_write(CommandReg, PCD_IDLE);
    rc522_write(ComIrqReg, 0x7F);
    rc522_write(FIFOLevelReg, 0x80); // Golire FIFO

    for (uint8_t i = 0; i < sendLen; i++) {
        rc522_write(FIFODataReg, send[i]);
    }

    rc522_write(BitFramingReg, bitFraming);
    rc522_write(CommandReg, PCD_TRANSCEIVE);
    rc522_write(BitFramingReg, bitFraming | 0x80); // StartSend

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
    uint8_t req = 0x26; // REQA
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

int main() {
    stdio_init_all();

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

    sleep_ms(2000);
    rc522_init();
    printf("Apropie un card RFID de cititor...\n");

    uint8_t uid[4];
    while (true) {
        if (rc522_read_uid(uid)) {
            printf("Card detectat! UID: %02X %02X %02X %02X\n", uid[0], uid[1], uid[2], uid[3]);
            sleep_ms(1000);
        }
        sleep_ms(100);
    }
}