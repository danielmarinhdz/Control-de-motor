#include "pantalla_oled.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

/* I2C1: PB8=SCL, PB9=SDA. Pines configurados en CubeMX, no aqui. */
#define OLED_DIRECCION (0x3Cu << 1)
#define OLED_ANCHO 128u
#define OLED_PAGINAS 8u
#define OLED_TIMEOUT_MS 20u
#define OLED_TEXTO_X 20u
#define OLED_TEXTO_ANCHO 48u /* Hasta 8 caracteres de 6 columnas */

static I2C_HandleTypeDef *i2c_oled = NULL;
static bool lista = false;
static uint8_t buffer[OLED_ANCHO * OLED_PAGINAS];

/* Glifos de cinco columnas, mas una columna vacia por caracter. */
static const uint8_t glifos[][5] = {
    {0,0,0,0,0}, {0,0x08,0x08,0x08,0}, {0,0x60,0x60,0,0},
    {0x3e,0x51,0x49,0x45,0x3e}, {0,0x42,0x7f,0x40,0},
    {0x42,0x61,0x51,0x49,0x46}, {0x21,0x41,0x45,0x4b,0x31},
    {0x18,0x14,0x12,0x7f,0x10}, {0x27,0x45,0x45,0x45,0x39},
    {0x3c,0x4a,0x49,0x49,0x30}, {0x01,0x71,0x09,0x05,0x03},
    {0x36,0x49,0x49,0x49,0x36}, {0x06,0x49,0x49,0x29,0x1e},
    {0x7f,0x09,0x19,0x29,0x46}, {0x7f,0x09,0x09,0x09,0x06},
    {0x7f,0x02,0x0c,0x02,0x7f}
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

/* Un solo envio I2C para varios comandos; 0x00 indica comandos. */
static bool comandos(const uint8_t *datos, uint16_t cantidad)
{
    uint8_t paquete[32];
    if (i2c_oled == NULL || datos == NULL || cantidad == 0u || cantidad > 31u) return false;
    paquete[0] = 0x00u;
    memcpy(&paquete[1], datos, cantidad);
    return HAL_I2C_Master_Transmit(i2c_oled, OLED_DIRECCION, paquete,
                                   (uint16_t)(cantidad + 1u), OLED_TIMEOUT_MS) == HAL_OK;
}

static void caracter(uint8_t x, uint8_t pagina, char c)
{
    if (pagina >= OLED_PAGINAS || x > (OLED_ANCHO - 6u)) return;
    const uint8_t *g = buscar_glifo(c);
    uint16_t base = (uint16_t)((uint16_t)pagina * OLED_ANCHO + x);
    for (uint8_t i = 0u; i < 5u; ++i) buffer[base + i] = g[i];
    buffer[base + 5u] = 0u;
}

static void texto(uint8_t x, uint8_t pagina, const char *s)
{
    while (*s != '\0' && x <= (OLED_ANCHO - 6u)) {
        caracter(x, pagina, *s);
        ++s;
        x = (uint8_t)(x + 6u);
    }
}

/* SSD1306 en modo horizontal: rango de columnas y una sola pagina. */
static bool enviar_zona(uint8_t pagina, uint8_t x, uint8_t ancho)
{
    if (pagina >= OLED_PAGINAS || ancho == 0u ||
        (uint16_t)x + ancho > OLED_ANCHO) return false;

    const uint8_t rango[] = {
        0x21u, x, (uint8_t)(x + ancho - 1u),
        0x22u, pagina, pagina
    };
    if (!comandos(rango, (uint16_t)sizeof(rango))) return false;

    uint8_t paquete[1u + OLED_ANCHO];
    paquete[0] = 0x40u;
    uint16_t base = (uint16_t)((uint16_t)pagina * OLED_ANCHO + x);
    memcpy(&paquete[1], &buffer[base], ancho);
    return HAL_I2C_Master_Transmit(i2c_oled, OLED_DIRECCION, paquete,
                                   (uint16_t)(ancho + 1u), OLED_TIMEOUT_MS) == HAL_OK;
}

/* Solo dos paginas con texto, y solo 48 columnas de cada una. */
static bool actualizar(void)
{
    return enviar_zona(2u, OLED_TEXTO_X, OLED_TEXTO_ANCHO) &&
           enviar_zona(4u, OLED_TEXTO_X, OLED_TEXTO_ANCHO);
}

bool pantalla_oled_inicializar(I2C_HandleTypeDef *hi2c)
{
    lista = false;
    i2c_oled = NULL;
    if (hi2c == NULL) return false;
    i2c_oled = hi2c;
    HAL_Delay(100u);

    const uint8_t inicio[] = {
        0xAE, 0xD5, 0x80, 0xA8, 0x3F, 0xD3, 0x00, 0x40,
        0x8D, 0x14, 0x20, 0x00, 0xA1, 0xC8, 0xDA, 0x12,
        0x81, 0x7F, 0xD9, 0xF1, 0xDB, 0x40, 0xA4, 0xA6, 0xAF
    };
    if (!comandos(inicio, (uint16_t)sizeof(inicio))) return false;

    /* Limpieza inicial completa, una sola vez, para borrar basura al arrancar. */
    memset(buffer, 0, sizeof(buffer));
    for (uint8_t pagina = 0u; pagina < OLED_PAGINAS; ++pagina) {
        if (!enviar_zona(pagina, 0u, OLED_ANCHO)) return false;
    }
    lista = true;
    return true;
}

bool pantalla_oled_mostrar_rpm(float rpm)
{
    if (!lista || i2c_oled == NULL || !isfinite(rpm)) return false;

    if (rpm > 99999.0f) rpm = 99999.0f;
    if (rpm < -99999.0f) rpm = -99999.0f;
    char numero[20];
    (void)snprintf(numero, sizeof(numero), "%d", (int)lroundf(rpm));

    /* Borrar las dos zonas evita restos de cifras anteriores. */
    memset(&buffer[2u * OLED_ANCHO + OLED_TEXTO_X], 0, OLED_TEXTO_ANCHO);
    memset(&buffer[4u * OLED_ANCHO + OLED_TEXTO_X], 0, OLED_TEXTO_ANCHO);
    texto(OLED_TEXTO_X, 2u, "RPM");
    texto(OLED_TEXTO_X, 4u, numero);
    return actualizar();
}
