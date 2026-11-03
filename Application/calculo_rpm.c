#include "calculo_rpm.h"

static uint16_t cuenta_previa = 0;

void calculo_rpm_iniciar(uint16_t cuenta)
{
    cuenta_previa = cuenta;
}

float calculo_rpm_actualizar(uint16_t cuenta, float dt_s)
{
    if (dt_s <= 0.0f) {
        return 0.0f;
    }

    /* 1. Cuantas cuentas avanzo. La resta en 16 bits y el cast a int16_t
     *    resuelven solos el desbordamiento (65535 -> 0) y el sentido (+/-). */
    int16_t delta = (int16_t)(uint16_t)(cuenta - cuenta_previa);
    cuenta_previa = cuenta;

    /* 2. Cuentas -> vueltas -> por minuto. */
    float vueltas = (float)delta / CALCULO_RPM_CUENTAS_POR_VUELTA;
    return CALCULO_RPM_SIGNO * vueltas * 60.0f / dt_s;
}
