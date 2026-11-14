#include "telemetria_serial.h"
#include "main.h"
#include "control_velocidad.h"
#include "maquina_estados.h"
#include "lector_encoder.h"

#ifdef HAL_UART_MODULE_ENABLED

#include <math.h>
#include <stdint.h>
#include <stdio.h>

/* huart2 lo crea CubeMX en main.c al activar USART2; aqui solo lo usamos. */
extern UART_HandleTypeDef huart2;

#define PERIODO_ENVIO_MS  100u
#define TIMEOUT_TX_MS     20u

static const char *nombre_estado(maquina_estados_estado_t estado)
{
    switch (estado) {
    case ESTADO_INIT:  return "INIT";
    case ESTADO_IDLE:  return "IDLE";
    case ESTADO_RUN:   return "RUN";
    case ESTADO_FAULT: return "FAULT";
    default:           return "?";
    }
}

void telemetria_serial_actualizar(void)
{
    static uint32_t ultimo_ms = 0u;

    uint32_t ahora = HAL_GetTick();
    if ((ahora - ultimo_ms) < PERIODO_ENVIO_MS) {
        return;
    }
    ultimo_ms = ahora;

    /* RPM con un decimal usando enteros: el printf "nano" no imprime float. */
    float rpm = control_velocidad_obtener_rpm();
    long rpm_x10 = isfinite(rpm) ? lroundf(rpm * 10.0f) : 0L;
    const char *signo = (rpm_x10 < 0L) ? "-" : "";
    long magnitud = (rpm_x10 < 0L) ? -rpm_x10 : rpm_x10;

    char linea[64];
    int n = snprintf(linea, sizeof(linea), "est=%s rpm=%s%ld.%ld cnt=%u\r\n",
                     nombre_estado(maquina_estados_obtener()),
                     signo, magnitud / 10L, magnitud % 10L,
                     (unsigned)lector_encoder_leer());
    if (n > 0) {
        (void)HAL_UART_Transmit(&huart2, (uint8_t *)linea, (uint16_t)n, TIMEOUT_TX_MS);
    }
}

#else  /* USART2 todavia no activado en CubeMX: no hace nada */

void telemetria_serial_actualizar(void)
{
}

#endif
