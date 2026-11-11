#include "experimento_pwm.h"

#if EXPERIMENTO_PWM_HABILITADO

#include "main.h"
#include "control_velocidad.h"
#include "maquina_estados.h"
#include "calculo_rpm.h"

#ifdef HAL_UART_MODULE_ENABLED

#include <stdio.h>
#include <string.h>

extern UART_HandleTypeDef huart2;

/* ---------------------------------------------------------------------------
 * Niveles de PWM (en %) de cada integrante. Unicos: no se repiten entre personas.
 * ------------------------------------------------------------------------- */
#if   EXPERIMENTO_INTEGRANTE == INTEGRANTE_DANIEL
#define NIVEL_BAJO  38u
#define NIVEL_ALTO  74u
#elif EXPERIMENTO_INTEGRANTE == INTEGRANTE_MATEO
#define NIVEL_BAJO  37u
#define NIVEL_ALTO  78u
#elif EXPERIMENTO_INTEGRANTE == INTEGRANTE_ANGEL
#define NIVEL_BAJO  41u
#define NIVEL_ALTO  71u
#else
#error "EXPERIMENTO_INTEGRANTE debe ser INTEGRANTE_DANIEL, INTEGRANTE_MATEO o INTEGRANTE_ANGEL"
#endif

#define TS_MS            10u    /* periodo de TIM2 / control_velocidad            */
#define MAX_MUESTRAS     4500u  /* 4500 x 3 bytes = 13.5 KB de RAM                */
#define TIMEOUT_TX_MS    20u

#define MODO_ESCALON     1u
#define MODO_PRBS9       2u

/* Duraciones en ciclos de 10 ms. */
#define ESC_CERO         100u   /* 1.0 s en 0 %                                   */
#define ESC_BAJO         300u   /* 3.0 s en BAJO                                  */
#define ESC_ALTO         400u   /* 4.0 s en ALTO                                  */
#define ESC_TOTAL        (ESC_CERO + ESC_BAJO + ESC_ALTO)
#define ESC_INICIO       (ESC_CERO + ESC_BAJO)            /* salto BAJO -> ALTO   */

#define PRBS_CALENTAR    150u   /* 1.5 s en BAJO antes de la secuencia            */
#define PRBS_BITS        511u   /* PRBS9: 2^9 - 1                                 */
#define PRBS_TOTAL       (PRBS_CALENTAR + PRBS_BITS * EXPERIMENTO_PRBS_BIT_MUESTRAS)
#define PRBS_INICIO      PRBS_CALENTAR                    /* primer bit           */

_Static_assert(PRBS_TOTAL <= MAX_MUESTRAS,
               "La PRBS9 no cabe en la memoria: baja EXPERIMENTO_PRBS_BIT_MUESTRAS (maximo 8)");
_Static_assert(ESC_TOTAL <= MAX_MUESTRAS, "El escalon no cabe en la memoria");

typedef enum { ESPERANDO, CORRIENDO, VOLCANDO } fase_t;

static fase_t   fase = ESPERANDO;
static uint8_t  modo = MODO_ESCALON;          /* prueba en curso / la que sigue    */
static uint32_t corrida = 0u;                 /* numero de prueba completa o en curso */
static uint32_t n = 0u;                       /* muestras guardadas                */
static uint32_t n_total = 0u;                 /* muestras que lleva la prueba      */
static uint32_t perdidas = 0u;                /* ciclos de 10 ms que se interpolaron */
static uint16_t cuenta_previa = 0u;
static uint16_t cuentas[MAX_MUESTRAS];
static uint8_t  pwms[MAX_MUESTRAS];
static uint32_t idx_envio = 0u;               /* 0 = falta el encabezado; k+1 = siguiente muestra */
static uint8_t  aviso = 0u;                   /* 1 = "corriendo", 2 = "abort"      */

/* PRBS9: registro de 9 bits, polinomio x^9 + x^5 + 1 (periodo 511), semilla 0x1FF. */
static uint16_t lfsr = 0x1FFu;
static uint8_t  bit_actual = 0u;

static uint8_t siguiente_bit(void)
{
    uint8_t bit = (uint8_t)((lfsr >> 8) & 1u);
    uint16_t realimentacion = (uint16_t)(((lfsr >> 8) ^ (lfsr >> 4)) & 1u);
    lfsr = (uint16_t)(((lfsr << 1) | realimentacion) & 0x1FFu);
    return bit;
}

/* Aplica un % de PWM (0..100) por la API publica de control_velocidad. */
static void aplicar_pwm(uint8_t pct)
{
    (void)control_velocidad_fijar_referencia((float)pct * CONTROL_VELOCIDAD_RPM_MAX / 100.0f);
}

/* PWM que toca aplicar despues de medir la muestra k. Se llama UNA vez por k, en orden. */
static uint8_t perfil(uint32_t k)
{
    if (modo == MODO_ESCALON) {
        if (k < ESC_CERO)             return 0u;
        if (k < ESC_CERO + ESC_BAJO)  return NIVEL_BAJO;
        return NIVEL_ALTO;
    }
    /* MODO_PRBS9 */
    if (k < PRBS_CALENTAR) {
        return NIVEL_BAJO;
    }
    if (((k - PRBS_CALENTAR) % EXPERIMENTO_PRBS_BIT_MUESTRAS) == 0u) {
        bit_actual = siguiente_bit();
    }
    return bit_actual ? NIVEL_ALTO : NIVEL_BAJO;
}

/* Guarda la muestra k (cuenta medida) y decide/aplica el PWM que sigue. */
static void registrar(uint16_t cuenta)
{
    cuentas[n] = cuenta;
    pwms[n] = perfil(n);
    aplicar_pwm(pwms[n]);
    n++;
}

static void enviar(const char *texto, uint16_t largo)
{
    (void)HAL_UART_Transmit(&huart2, (uint8_t *)texto, largo, TIMEOUT_TX_MS);
}

static void enviar_texto(const char *texto)
{
    enviar(texto, (uint16_t)strlen(texto));
}

void experimento_pwm_iniciar(void)
{
    fase = ESPERANDO;
    modo = MODO_ESCALON;
    corrida = 0u;
    aviso = 0u;
    aplicar_pwm(0u);   /* pisa cualquier referencia de prueba de main.c */
}

int experimento_pwm_activo(void)
{
    return (fase != ESPERANDO) ? 1 : 0;
}

void experimento_pwm_muestrear(uint16_t cuenta, uint32_t periodos)
{
    if (fase == VOLCANDO) {
        return;
    }

    if (fase == ESPERANDO) {
        /* Arranca cuando el boton azul lleva la maquina de estados a RUN. */
        if (maquina_estados_obtener() != ESTADO_RUN) {
            return;
        }
        corrida++;
        n = 0u;
        n_total = (modo == MODO_ESCALON) ? ESC_TOTAL : PRBS_TOTAL;
        perdidas = 0u;
        lfsr = 0x1FFu;
        bit_actual = 0u;
        cuenta_previa = cuenta;
        fase = CORRIENDO;
        aviso = 1u;
        registrar(cuenta);      /* muestra 0 */
        return;
    }

    /* fase == CORRIENDO */
    if (maquina_estados_obtener() != ESTADO_RUN) {
        aplicar_pwm(0u);        /* el boton ya apago el motor; se repetira la misma prueba */
        corrida--;
        fase = ESPERANDO;
        aviso = 2u;
        return;
    }

    /* Normalmente periodos = 1. Si el while(1) se atraso, se rellenan las
     * muestras que faltan interpolando la cuenta, y se anotan como perdidas. */
    int32_t delta = (int16_t)(uint16_t)(cuenta - cuenta_previa);
    for (uint32_t i = 1u; i <= periodos && n < n_total; ++i) {
        registrar((uint16_t)(cuenta_previa + (delta * (int32_t)i) / (int32_t)periodos));
    }
    if (periodos > 1u) {
        perdidas += periodos - 1u;
    }
    cuenta_previa = cuenta;

    if (n >= n_total) {
        aplicar_pwm(0u);
        (void)control_velocidad_detener();   /* RUN -> IDLE, motor apagado */
        idx_envio = 0u;
        fase = VOLCANDO;
    }
}

void experimento_pwm_actualizar(void)
{
    char linea[160];
    int largo;

    if (aviso != 0u) {
        enviar_texto((aviso == 1u) ? "EXP,CORRIENDO\r\n" : "EXP,ABORT\r\n");
        aviso = 0u;
        return;
    }
    if (fase != VOLCANDO) {
        return;
    }

    if (idx_envio == 0u) {
        largo = snprintf(linea, sizeof(linea),
                         "EXP,INICIO,run=%lu,modo=%u,quien=%d,bajo=%u,alto=%u,ts_ms=%u,n=%lu,"
                         "ini=%u,cpv=%d,sgn=%d,bitm=%u,perdidas=%lu\r\n",
                         (unsigned long)corrida, (unsigned)modo, (int)EXPERIMENTO_INTEGRANTE,
                         (unsigned)NIVEL_BAJO, (unsigned)NIVEL_ALTO, (unsigned)TS_MS,
                         (unsigned long)n,
                         (unsigned)((modo == MODO_ESCALON) ? ESC_INICIO : PRBS_INICIO),
                         (int)CALCULO_RPM_CUENTAS_POR_VUELTA, (int)CALCULO_RPM_SIGNO,
                         (unsigned)((modo == MODO_ESCALON) ? 0u : EXPERIMENTO_PRBS_BIT_MUESTRAS),
                         (unsigned long)perdidas);
        idx_envio = 1u;
    } else if (idx_envio <= n) {
        uint32_t k = idx_envio - 1u;
        largo = snprintf(linea, sizeof(linea), "EXP,%lu,%u,%u\r\n",
                         (unsigned long)k, (unsigned)pwms[k], (unsigned)cuentas[k]);
        idx_envio++;
    } else {
        enviar_texto("EXP,FIN\r\n");
        modo = (modo == MODO_ESCALON) ? MODO_PRBS9 : MODO_ESCALON;   /* la siguiente pulsacion hace la otra prueba */
        fase = ESPERANDO;
        return;
    }
    if (largo > 0) {
        enviar(linea, (uint16_t)largo);
    }
}

#else  /* USART2 no activado: el modulo no hace nada */

void experimento_pwm_iniciar(void) {}
void experimento_pwm_muestrear(uint16_t cuenta, uint32_t periodos) { (void)cuenta; (void)periodos; }
void experimento_pwm_actualizar(void) {}
int  experimento_pwm_activo(void) { return 0; }

#endif /* HAL_UART_MODULE_ENABLED */

#else  /* EXPERIMENTO_PWM_HABILITADO == 0 */

void experimento_pwm_iniciar(void) {}
void experimento_pwm_muestrear(uint16_t cuenta, uint32_t periodos) { (void)cuenta; (void)periodos; }
void experimento_pwm_actualizar(void) {}
int  experimento_pwm_activo(void) { return 0; }

#endif
