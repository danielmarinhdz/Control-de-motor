#ifndef CALCULO_PID_H
#define CALCULO_PID_H

#include <stdbool.h>

/* Capa Application (archivos en Core). Unico trabajo: calcular el mando PID de
 * velocidad en forma incremental, discretizado por Tustin:
 *
 *     e(k) = r(k) - y(k)
 *     u(k) = u(k-1) + q0*e(k) + q1*e(k-1) + q2*e(k-2)
 *
 *     q0 =  kp + ki*Ts/2 + kd/Ts
 *     q1 = -kp + ki*Ts/2 - 2*kd/Ts
 *     q2 =  kd/Ts
 *
 * r y y en RPM del eje de salida (con signo); u en % de PWM con signo.
 * La salida se satura a [-100, 100] y el valor saturado se guarda como u(k-1),
 * asi la forma incremental no acumula windup.
 */

/* Ganancias de diseno (MATLAB, cancelacion polo-cero).
 * Planta: G(s) = 3.8006 / (0.0362 s + 1)  [RPM / %]
 * Objetivo: establecimiento <= 1 s (2 %) y sobreimpulso <= 5 %.
 * Con Kd = 0 es un PI: q0 ~ 0.04336, q1 ~ -0.03284. */
#define CALCULO_PID_KP  0.0381f   /* [% / RPM]          */
#define CALCULO_PID_KI  1.0525f   /* [% / (RPM * s)]    */
#define CALCULO_PID_KD  0.0f      /* [% * s / RPM]      */

/* Calcula los coeficientes q0, q1, q2 a partir de las ganancias.
 * Recibe: kp, ki [1/s], kd [s] (todas >= 0) y dt_s = periodo de muestreo
 *         en segundos (> 0, previsto 0.010).
 * Devuelve: true si se configuro; false ante dt_s <= 0 o alguna ganancia
 *           negativa (en ese caso no cambia nada).
 * Cuando: una vez al iniciar (control_velocidad_iniciar), antes de actualizar.
 */
bool calculo_pid_configurar(float kp, float ki, float kd, float dt_s);

/* Borra el estado: u(k-1), e(k-1) y e(k-2) a 0.
 * Recibe: nada. Devuelve: nada.
 * Cuando: al iniciar y cada vez que el motor arranca o se detiene, para que
 *         cada arranque empiece sin estado previo. Nunca desde una ISR.
 */
void calculo_pid_reiniciar(void);

/* Calcula el mando del ciclo actual.
 * Recibe: referencia_rpm y medida_rpm, RPM con signo del eje de salida.
 * Devuelve: mando con signo en % de PWM, saturado a [-100, 100].
 * Cuando: cada 10 ms, solo en RUN, desde el while(1). Nunca desde una ISR.
 */
float calculo_pid_actualizar(float referencia_rpm, float medida_rpm);

#endif
