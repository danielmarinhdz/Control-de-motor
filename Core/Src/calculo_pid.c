#include "calculo_pid.h"

#define MANDO_MAX   100.0f   /* % de PWM */
#define MANDO_MIN  -100.0f

/* Coeficientes de la forma incremental (los calcula configurar). En 0 hasta
 * configurar: asi el mando sale en 0 si alguien llama actualizar antes. */
static float q0 = 0.0f;
static float q1 = 0.0f;
static float q2 = 0.0f;

/* Estado del controlador entre una llamada y la siguiente. */
static float u_previo = 0.0f;    /* u(k-1), ya saturado */
static float e_previo1 = 0.0f;   /* e(k-1) */
static float e_previo2 = 0.0f;   /* e(k-2) */

bool calculo_pid_configurar(float kp, float ki, float kd, float dt_s)
{
    /* Escrito como !(x > 0) para que tambien rechace NaN. */
    if (!(dt_s > 0.0f) || !(kp >= 0.0f) || !(ki >= 0.0f) || !(kd >= 0.0f)) {
        return false;
    }

    /* Tustin aplicado al PID en forma incremental. */
    q0 =  kp + ki * dt_s / 2.0f + kd / dt_s;
    q1 = -kp + ki * dt_s / 2.0f - 2.0f * kd / dt_s;
    q2 =  kd / dt_s;
    return true;
}

void calculo_pid_reiniciar(void)
{
    u_previo = 0.0f;
    e_previo1 = 0.0f;
    e_previo2 = 0.0f;
}

float calculo_pid_actualizar(float referencia_rpm, float medida_rpm)
{
    /* 1. Error del ciclo actual. */
    float e = referencia_rpm - medida_rpm;

    /* 2. Ecuacion en diferencias: u(k) = u(k-1) + q0 e(k) + q1 e(k-1) + q2 e(k-2). */
    float u = u_previo + q0 * e + q1 * e_previo1 + q2 * e_previo2;

    /* 3. Saturar al rango del PWM. */
    if (u > MANDO_MAX) {
        u = MANDO_MAX;
    } else if (u < MANDO_MIN) {
        u = MANDO_MIN;
    }

    /* 4. Guardar el estado. Se guarda el valor SATURADO como u(k-1): si el
     *    motor no alcanza la referencia, u no sigue creciendo por encima del
     *    100 % (anti-windup de la forma incremental). */
    u_previo = u;
    e_previo2 = e_previo1;
    e_previo1 = e;

    return u;
}
