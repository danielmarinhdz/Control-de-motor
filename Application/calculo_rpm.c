#include "calculo_rpm.h"

static uint16_t cuenta_previa = 0;
static float rpm_filtrada = 0.0f;

void calculo_rpm_iniciar(uint16_t cuenta)
{
    cuenta_previa = cuenta;
    rpm_filtrada = 0.0f;   /* el motor arranca parado */
}

float calculo_rpm_actualizar(uint16_t cuenta, float dt_s)
{
    if (dt_s <= 0.0f) {
        return rpm_filtrada;
    }

    /* 1. Cuantas cuentas avanzo. La resta en 16 bits y el cast a int16_t
     *    resuelven solos el desbordamiento (65535 -> 0) y el sentido (+/-). */
    int16_t delta = (int16_t)(uint16_t)(cuenta - cuenta_previa);
    cuenta_previa = cuenta;

    /* 2. Cuentas -> vueltas -> por minuto. */
    float vueltas = (float)delta / CALCULO_RPM_CUENTAS_POR_VUELTA;
    float rpm_cruda = CALCULO_RPM_SIGNO * vueltas * 60.0f / dt_s;

    /* 3. Filtro pasa-bajas de primer orden (EMA):
     *      y[k] = y[k-1] + alfa * (x[k] - y[k-1]),  alfa = dt / (tau + dt)
     *    alfa se calcula con el dt real, asi que si se atraso el while(1)
     *    (dt = 20, 30 ms...) el filtro sigue teniendo la misma tau. */
    float alfa = dt_s / (CALCULO_RPM_TAU_FILTRO_S + dt_s);
    rpm_filtrada += alfa * (rpm_cruda - rpm_filtrada);
    return rpm_filtrada;
}
