#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_rom_sys.h"


#define FILAS 8
#define COLS_VERDES 8
#define COLS_ROJAS 3

#define BOTON_IZQ GPIO_NUM_34
#define BOTON_DER GPIO_NUM_35

// Filas con transistor PNP
// LOW = fila encendida
// HIGH = fila apagada
static const gpio_num_t pines_filas[FILAS] = {
    GPIO_NUM_16,
    GPIO_NUM_17,
    GPIO_NUM_18,
    GPIO_NUM_19,
    GPIO_NUM_21,
    GPIO_NUM_22,
    GPIO_NUM_23,
    GPIO_NUM_25
};

// Columnas verdes
static const gpio_num_t pines_verdes[COLS_VERDES] = {
    GPIO_NUM_26,
    GPIO_NUM_27,
    GPIO_NUM_32,
    GPIO_NUM_33,
    GPIO_NUM_13,
    GPIO_NUM_14,
    GPIO_NUM_4,
    GPIO_NUM_5
};

// Columnas rojas conectadas
static const gpio_num_t pines_rojos[COLS_ROJAS] = {
    GPIO_NUM_0,
    GPIO_NUM_2,
    GPIO_NUM_15
};

// Estas son las columnas verdes que coinciden con los 3 carriles rojos
static const int carriles_verdes[COLS_ROJAS] = {0, 2, 4};



static volatile bool matriz_verde[FILAS][COLS_VERDES];
static volatile bool matriz_roja[FILAS][COLS_ROJAS];



// Jugador en la fila 8
static volatile int pos_jugador = 3;

// Guarda en qué fila va cada obstáculo rojo
// -1 significa que no hay obstáculo en ese carril
static volatile int pos_obstaculos[COLS_ROJAS] = {-1, -1, -1};

// Contador de choques
static volatile int choques = 0;

static uint32_t semilla = 0xA5A5A5A5;



static const uint8_t numero_1[8] = {
    0b00011000,
    0b00111000,
    0b00011000,
    0b00011000,
    0b00011000,
    0b00011000,
    0b00111100,
    0b00000000
};

static const uint8_t numero_2[8] = {
    0b00111100,
    0b01100110,
    0b00000110,
    0b00001100,
    0b00110000,
    0b01100000,
    0b01111110,
    0b00000000
};


static uint32_t aleatorio_simple(void)
{
    semilla ^= (semilla << 13);
    semilla ^= (semilla >> 17);
    semilla ^= (semilla << 5);
    return semilla;
}

static void limpiar_todo(void)
{
    memset((void *)matriz_verde, 0, sizeof(matriz_verde));
    memset((void *)matriz_roja, 0, sizeof(matriz_roja));
}

static void apagar_filas(void)
{
    for (int i = 0; i < FILAS; i++) {
        gpio_set_level(pines_filas[i], 1);
    }
}

static void apagar_verdes(void)
{
    for (int i = 0; i < COLS_VERDES; i++) {
        gpio_set_level(pines_verdes[i], 1);
    }
}

static void apagar_rojos(void)
{
    for (int i = 0; i < COLS_ROJAS; i++) {
        gpio_set_level(pines_rojos[i], 1);
    }
}

static void apagar_columnas(void)
{
    apagar_verdes();
    apagar_rojos();
}

static void iniciar_pines(void)
{
    for (int i = 0; i < FILAS; i++) {
        gpio_reset_pin(pines_filas[i]);
        gpio_set_direction(pines_filas[i], GPIO_MODE_OUTPUT);
        gpio_set_level(pines_filas[i], 1);
    }

    for (int i = 0; i < COLS_VERDES; i++) {
        gpio_reset_pin(pines_verdes[i]);
        gpio_set_direction(pines_verdes[i], GPIO_MODE_OUTPUT);
        gpio_set_level(pines_verdes[i], 1);
    }

    for (int i = 0; i < COLS_ROJAS; i++) {
        gpio_reset_pin(pines_rojos[i]);
        gpio_set_direction(pines_rojos[i], GPIO_MODE_OUTPUT);
        gpio_set_level(pines_rojos[i], 1);
    }

    gpio_reset_pin(BOTON_IZQ);
    gpio_set_direction(BOTON_IZQ, GPIO_MODE_INPUT);

    gpio_reset_pin(BOTON_DER);
    gpio_set_direction(BOTON_DER, GPIO_MODE_INPUT);
}

static void mostrar_dibujo_verde(const uint8_t dibujo[8])
{
    limpiar_todo();

    for (int f = 0; f < 8; f++) {
        for (int c = 0; c < 8; c++) {
            if (dibujo[f] & (1 << (7 - c))) {
                matriz_verde[f][c] = true;
            }
        }
    }
}

static void prender_todo_verde(void)
{
    limpiar_todo();

    for (int f = 0; f < FILAS; f++) {
        for (int c = 0; c < COLS_VERDES; c++) {
            matriz_verde[f][c] = true;
        }
    }
}

static void actualizar_pantalla(void)
{
    limpiar_todo();

    // jugador verde en la fila 8
    if (pos_jugador >= 0 && pos_jugador < COLS_VERDES) {
        matriz_verde[7][pos_jugador] = true;
    }

    // obstaculos rojos
    for (int i = 0; i < COLS_ROJAS; i++) {
        if (pos_obstaculos[i] >= 0 && pos_obstaculos[i] < FILAS) {
            matriz_roja[pos_obstaculos[i]][i] = true;
        }
    }
}

static void reiniciar_ronda(void)
{
    pos_jugador = 3;

    for (int i = 0; i < COLS_ROJAS; i++) {
        pos_obstaculos[i] = -1;
    }

    actualizar_pantalla();
}

static void reiniciar_todo(void)
{
    choques = 0;
    reiniciar_ronda();
}


static void bajar_obstaculos(void)
{
    for (int i = 0; i < COLS_ROJAS; i++) {
        if (pos_obstaculos[i] != -1) {
            pos_obstaculos[i]++;

            if (pos_obstaculos[i] >= FILAS) {
                pos_obstaculos[i] = -1;
            }
        }
    }
}

static void crear_obstaculo(void)
{
    if ((aleatorio_simple() % 2) == 0) {
        int carril = aleatorio_simple() % COLS_ROJAS;

        if (pos_obstaculos[carril] == -1) {
            pos_obstaculos[carril] = 0;
        }
    }
}

static bool hubo_choque(void)
{
    for (int i = 0; i < COLS_ROJAS; i++) {
        if (pos_jugador == carriles_verdes[i] && pos_obstaculos[i] == 7) {
            return true;
        }
    }
    return false;
}

static void mostrar_choque(void)
{
    choques++;

    if (choques == 1) {
        mostrar_dibujo_verde(numero_1);
        vTaskDelay(pdMS_TO_TICKS(1000));
        reiniciar_ronda();
    }
    else if (choques == 2) {
        mostrar_dibujo_verde(numero_2);
        vTaskDelay(pdMS_TO_TICKS(1000));

        prender_todo_verde();
        vTaskDelay(pdMS_TO_TICKS(1000));

        reiniciar_todo();
    }
    else {
        reiniciar_todo();
    }
}

static void leer_botones(void)
{
    static int antes_izq = 1;
    static int antes_der = 1;

    int ahora_izq = gpio_get_level(BOTON_IZQ);
    int ahora_der = gpio_get_level(BOTON_DER);

    if ((antes_izq == 1) && (ahora_izq == 0)) {
        if (pos_jugador > 0) {
            pos_jugador--;
        }
    }

    if ((antes_der == 1) && (ahora_der == 0)) {
        if (pos_jugador < (COLS_VERDES - 1)) {
            pos_jugador++;
        }
    }

    antes_izq = ahora_izq;
    antes_der = ahora_der;
}


static void tarea_refresco(void *arg)
{
    while (1) {
        for (int f = 0; f < FILAS; f++) {

            apagar_filas();
            apagar_columnas();

            for (int c = 0; c < COLS_VERDES; c++) {
                gpio_set_level(pines_verdes[c], matriz_verde[f][c] ? 0 : 1);
            }

            for (int c = 0; c < COLS_ROJAS; c++) {
                gpio_set_level(pines_rojos[c], matriz_roja[f][c] ? 0 : 1);
            }

            gpio_set_level(pines_filas[f], 0);

            esp_rom_delay_us(1200);
        }

        vTaskDelay(pdMS_TO_TICKS(1));
    }
}



static void tarea_juego(void *arg)
{
    reiniciar_todo();

    while (1) {
        leer_botones();

        bajar_obstaculos();

        if (hubo_choque()) {
            mostrar_choque();
            actualizar_pantalla();
            vTaskDelay(pdMS_TO_TICKS(150));
            continue;
        }

        crear_obstaculo();

        actualizar_pantalla();

        vTaskDelay(pdMS_TO_TICKS(220));
    }
}

// -------------------------------
// MAIN
// -------------------------------

void app_main(void)
{
    iniciar_pines();
    reiniciar_todo();

    xTaskCreate(tarea_refresco, "tarea_refresco", 4096, NULL, 1, NULL);
    xTaskCreate(tarea_juego, "tarea_juego", 4096, NULL, 2, NULL);
}