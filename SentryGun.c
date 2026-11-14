
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
}