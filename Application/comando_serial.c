#include "comando_serial.h"
#include "main.h"
#include "control_velocidad.h"

#ifdef HAL_UART_MODULE_ENABLED

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

/* huart2 lo crea CubeMX en main.c al activar USART2; aqui solo lo usamos. */
extern UART_HandleTypeDef huart2;

#define TAM_COLA       32u   /* potencia de 2 */
#define TAM_LINEA      16u   /* "-342.5" cabe de sobra */
#define TIMEOUT_TX_MS  20u

/* Compartidas con la ISR: volatile. La ISR solo escribe 'entrada';
 * el while(1) solo escribe 'salida'. */
static uint8_t byte_rx;                      /* donde el HAL deja cada caracter */
static volatile uint8_t cola[TAM_COLA];
static volatile uint8_t entrada = 0;
static volatile uint8_t salida = 0;

/* Solo se usan fuera de la ISR. */
static char linea[TAM_LINEA];
static uint8_t largo = 0;
static bool linea_larga = false;

static void escuchar(void)
{
    (void)HAL_UART_Receive_IT(&huart2, &byte_rx, 1u);
}

/* Llega un caracter: guardarlo en la cola y volver a escuchar.
 * Si la cola esta llena, el caracter se descarta (la linea saldra ERROR). */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART2) {
        uint8_t siguiente = (uint8_t)((entrada + 1u) & (TAM_COLA - 1u));
        if (siguiente != salida) {
            cola[entrada] = byte_rx;
            entrada = siguiente;
        }
        escuchar();
    }
}

/* Error de recepcion (ej. se encimo un caracter): el HAL deja de escuchar,
 * asi que volvemos a pedirlo. */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART2) {
        escuchar();
    }
}

/* Convierte "150", "-120", "180.5" a float. Devuelve false si no es un numero.
 * Se hace a mano para no depender de strtof. */
static bool convertir(const char *texto, float *valor)
{
    float signo = 1.0f;
    float resultado = 0.0f;
    float escala = 0.1f;
    bool hay_digitos = false;
    bool en_decimales = false;

    if (*texto == '-' || *texto == '+') {
        if (*texto == '-') {
            signo = -1.0f;
        }
        texto++;
    }

    for (; *texto != '\0'; texto++) {
        char c = *texto;
        if (c >= '0' && c <= '9') {
            hay_digitos = true;
            if (en_decimales) {
                resultado += (float)(c - '0') * escala;
                escala *= 0.1f;
            } else {
                resultado = resultado * 10.0f + (float)(c - '0');
            }
        } else if (c == '.' && !en_decimales) {
            en_decimales = true;
        } else if (c == ' ') {
            /* espacios al final o en medio: se ignoran */
        } else {
            return false;
        }
    }

    if (!hay_digitos) {
        return false;
    }
    *valor = signo * resultado;
    return true;
}

static void contestar(const char *texto, int n)
{
    if (n > 0) {
        (void)HAL_UART_Transmit(&huart2, (uint8_t *)texto, (uint16_t)n, TIMEOUT_TX_MS);
    }
}

/* Una linea completa: fijar la referencia y contestar. */
static void procesar_linea(void)
{
    char respuesta[32];
    float rpm = 0.0f;

    if (linea_larga || !convertir(linea, &rpm)) {
        contestar("ref=ERROR\r\n", 11);
        return;
    }

    bool en_rango = control_velocidad_fijar_referencia(rpm);
    if (rpm > CONTROL_VELOCIDAD_RPM_MAX) {
        rpm = CONTROL_VELOCIDAD_RPM_MAX;
    } else if (rpm < -CONTROL_VELOCIDAD_RPM_MAX) {
        rpm = -CONTROL_VELOCIDAD_RPM_MAX;
    }

    /* Un decimal usando enteros: el printf "nano" no imprime float. */
    long rpm_x10 = lroundf(rpm * 10.0f);
    const char *signo = (rpm_x10 < 0L) ? "-" : "";
    long magnitud = (rpm_x10 < 0L) ? -rpm_x10 : rpm_x10;
    int n = snprintf(respuesta, sizeof(respuesta), "ref=%s%ld.%ld %s\r\n",
                     signo, magnitud / 10L, magnitud % 10L,
                     en_rango ? "OK" : "LIMITE");
    contestar(respuesta, n);
}

void comando_serial_iniciar(void)
{
    entrada = 0;
    salida = 0;
    largo = 0;
    linea_larga = false;
    escuchar();
}

void comando_serial_actualizar(void)
{
    /* Por si la recepcion se quedo detenida (ej. un error), volver a escuchar. */
    __disable_irq();
    if (huart2.RxState == HAL_UART_STATE_READY) {
        escuchar();
    }
    __enable_irq();

    while (salida != entrada) {
        char c = (char)cola[salida];
        salida = (uint8_t)((salida + 1u) & (TAM_COLA - 1u));

        if (c == '\r' || c == '\n') {
            if (largo > 0u || linea_larga) {   /* CR+LF: la segunda se ignora */
                linea[largo] = '\0';
                procesar_linea();
            }
            largo = 0;
            linea_larga = false;
        } else if (largo < (TAM_LINEA - 1u)) {
            linea[largo++] = c;
        } else {
            linea_larga = true;               /* demasiado larga: saldra ERROR */
        }
    }
}

#else  /* USART2 todavia no activado en CubeMX: no hace nada */

void comando_serial_iniciar(void)
{
}

void comando_serial_actualizar(void)
{
}

#endif
