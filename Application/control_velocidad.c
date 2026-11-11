#include "control_velocidad.h"
#include "main.h"
#include "maquina_estados.h"
#include "generador_pwm.h"
#include "pantalla_oled.h"
#include "lector_encoder.h"
#include "calculo_rpm.h"
#include "experimento_pwm.h"

#define PERIODO_CONTROL_S  0.010f  /* TIM2: 10 ms */
#define PERIODOS_PANTALLA  10u     /* 10 x 10 ms = refrescar cada ~100 ms */

/* Compartidas con la ISR: volatile. */
static volatile uint16_t cuenta_isr = 0;
static volatile uint32_t periodos_pendientes = 0;

/* Solo se usan fuera de la ISR. */
static float referencia_rpm = 0.0f;
static float rpm_medida = 0.0f;
static uint32_t periodos_desde_pantalla = 0;

/* Lazo abierto: relacion lineal RPM -> %. Con el PID, aqui se llamara
 * a calculo_pid_actualizar(referencia_rpm, rpm_medida). */
static float calcular_mando(void)
{
    return referencia_rpm * 100.0f / CONTROL_VELOCIDAD_RPM_MAX;
}

/* Unica ISR de timers del proyecto: solo toma la foto del encoder y avisa.
 * Si el while(1) se atrasa, periodos_pendientes cuenta cuantos 10 ms pasaron. */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM2) {
        cuenta_isr = lector_encoder_leer();
        periodos_pendientes++;
    }
}

void control_velocidad_iniciar(void)
{
    maquina_estados_inicializar();
    generador_pwm_iniciar();

    /* Sin interrupciones para que la cuenta inicial y el contador de
     * periodos queden alineados. */
    __disable_irq();
    calculo_rpm_iniciar(lector_encoder_leer());
    periodos_pendientes = 0;
    __enable_irq();

    (void)maquina_estados_listo();
}

void control_velocidad_ejecutar(void)
{
    /* 1. Copiar lo que dejo la ISR y reiniciar el contador, sin que la ISR
     *    pueda entrar a la mitad. */
    __disable_irq();
    uint32_t periodos = periodos_pendientes;
    uint16_t cuenta = cuenta_isr;
    periodos_pendientes = 0;
    __enable_irq();

    if (periodos == 0u) {
        return;  /* Todavia no pasan 10 ms */
    }

    /* 2. Medir. dt real = periodos x 10 ms (normalmente 1). */
    float dt_s = (float)periodos * PERIODO_CONTROL_S;
    rpm_medida = calculo_rpm_actualizar(cuenta, dt_s);

    /* Gancho del experimento: guarda la muestra y fija el PWM de este ciclo.
     * Si el experimento esta apagado (o no corre), no hace nada. */
    experimento_pwm_muestrear(cuenta, periodos);

    /* 3. Actuar: solo RUN mueve el motor; IDLE y FAULT lo mantienen apagado. */
    if (maquina_estados_permite_motor()) {
        (void)generador_pwm_fijar(calcular_mando());
    } else {
        generador_pwm_detener();
    }

    /* 4. Pantalla cada ~100 ms. Si falla el I2C se ignora: el motor sigue. */
    periodos_desde_pantalla += periodos;
    if (periodos_desde_pantalla >= PERIODOS_PANTALLA) {
        periodos_desde_pantalla = 0;
        (void)pantalla_oled_mostrar_rpm(rpm_medida);
    }
}

bool control_velocidad_fijar_referencia(float rpm)
{
    bool en_rango = true;

    if (rpm > CONTROL_VELOCIDAD_RPM_MAX) {
        rpm = CONTROL_VELOCIDAD_RPM_MAX;
        en_rango = false;
    } else if (rpm < -CONTROL_VELOCIDAD_RPM_MAX) {
        rpm = -CONTROL_VELOCIDAD_RPM_MAX;
        en_rango = false;
    }

    referencia_rpm = rpm;
    return en_rango;
}

bool control_velocidad_arrancar(void)
{
    return maquina_estados_arrancar();
}

bool control_velocidad_detener(void)
{
    bool estaba_en_run = maquina_estados_detener();
    generador_pwm_detener();
    return estaba_en_run;
}

float control_velocidad_obtener_rpm(void)
{
    return rpm_medida;
}
