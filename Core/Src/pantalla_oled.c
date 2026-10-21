#include "pantalla_oled.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

#define OLED_DIRECCION (0x3Cu << 1) /* HAL espera direccion desplazada */
#define OLED_ANCHO 128u
#define OLED_ALTO 64u
#define OLED_TIMEOUT_MS 20u

static I2C_HandleTypeDef *i2c_oled = NULL;
static uint8_t buffer[OLED_ANCHO * OLED_ALTO / 8u];

/* Fuente 5x7 reducida: espacio, '-', '.', numeros y letras de 'RPM'. */
static const uint8_t glifos[][5] = {
    {0,0,0,0,0},                  /* 0: espacio */
    {0,0x08,0x08,0x08,0},         /* 1: - */
    {0,0x60,0x60,0,0},            /* 2: . */
    {0x3e,0x51,0x49,0x45,0x3e},  /* 3: 0 */
    {0,0x42,0x7f,0x40,0},         /* 4: 1 */
    {0x42,0x61,0x51,0x49,0x46},  /* 5: 2 */
    {0x21,0x41,0x45,0x4b,0x31},  /* 6: 3 */
    {0x18,0x14,0x12,0x7f,0x10},  /* 7: 4 */
    {0x27,0x45,0x45,0x45,0x39},  /* 8: 5 */
    {0x3c,0x4a,0x49,0x49,0x30},  /* 9: 6 */
    {0x01,0x71,0x09,0x05,0x03},  /* 10: 7 */
    {0x36,0x49,0x49,0x49,0x36},  /* 11: 8 */
    {0x06,0x49,0x49,0x29,0x1e},  /* 12: 9 */
    {0x7f,0x09,0x19,0x29,0x46},  /* 13: R */
    {0x7f,0x09,0x09,0x09,0x06},  /* 14: P */
    {0x7f,0x02,0x0c,0x02,0x7f}   /* 15: M */
};

static const uint8_t *buscar_glifo(char c)
{
    if (c >= '0' && c <= '9') return glifos[3 + (c - '0')];
    if (c == '-') return glifos[1];
    if (c == '.') return glifos[2];
    if (c == 'R') return glifos[13];
    if (c == 'P') return glifos[14];
    if (c == 'M') return glifos[15];
    return glifos[0];
}

static bool comando(uint8_t cmd)
{
    uint8_t paquete[2] = {0x00, cmd};
    return HAL_I2C_Master_Transmit(i2c_oled, OLED_DIRECCION, paquete,
                                   sizeof(paquete), OLED_TIMEOUT_MS) == HAL_OK;
}

static void caracter(uint8_t x, uint8_t pagina, char c)
{
    if (pagina >= 8 || x > 122) return;
    const uint8_t *g = buscar_glifo(c);
    uint16_t base = (uint16_t)pagina * OLED_ANCHO + x;
    for (uint8_t i = 0; i < 5; ++i) buffer[base + i] = g[i];
    buffer[base + 5] = 0;
}

static void texto(uint8_t x, uint8_t pagina, const char *s)
{
    while (*s && x <= 122) {
        caracter(x, pagina, *s++);
        x = (uint8_t)(x + 6);
    }
}

static bool actualizar(void)
{
    /* Rango de columnas 0..127 y paginas 0..7. */
    if (!comando(0x21) || !comando(0) || !comando(127) ||
        !comando(0x22) || !comando(0) || !comando(7)) return false;

    uint8_t paquete[17];
    paquete[0] = 0x40; /* datos GDDRAM */
    for (uint16_t pos = 0; pos < sizeof(buffer); pos += 16) {
        memcpy(&paquete[1], &buffer[pos], 16);
        if (HAL_I2C_Master_Transmit(i2c_oled, OLED_DIRECCION, paquete,
                                     sizeof(paquete), OLED_TIMEOUT_MS) != HAL_OK) return false;
    }
    return true;
}

bool pantalla_oled_inicializar(I2C_HandleTypeDef *hi2c)
{
    if (hi2c == NULL) return false;
    i2c_oled = hi2c;
    HAL_Delay(100);
    const uint8_t inicio[] = {
        0xAE, 0xD5, 0x80, 0xA8, 0x3F, 0xD3, 0x00, 0x40,
        0x8D, 0x14, 0x20, 0x00, 0xA1, 0xC8, 0xDA, 0x12,
        0x81, 0x7F, 0xD9, 0xF1, 0xDB, 0x40, 0xA4, 0xA6, 0xAF
    };
    for (uint8_t i = 0; i < sizeof(inicio); ++i) {
        if (!comando(inicio[i])) return false;
    }
    memset(buffer, 0, sizeof(buffer));
    return actualizar();
}

bool pantalla_oled_mostrar_rpm(float rpm)
{
    if (i2c_oled == NULL || !isfinite(rpm)) return false;
    char numero[20];
    /* Redondeo a entero, evitando conversiones desbordadas. */
    if (rpm > 99999.0f) rpm = 99999.0f;
    if (rpm < -99999.0f) rpm = -99999.0f;
    (void)snprintf(numero, sizeof(numero), "%.0f", (double)rpm);
    memset(buffer, 0, sizeof(buffer));
    texto(20, 2, "RPM");
    texto(20, 4, numero);
    return actualizar();
}
