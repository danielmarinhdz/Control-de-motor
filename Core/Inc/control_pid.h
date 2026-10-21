#ifndef CALCULO_PID_H
#define CALCULO_PID_H

/* INTERFAZ FUTURA: no hay calculo_pid.c en este periodo.
 * No llamar estas funciones hasta implementar el modulo.
 */

/* Configura Kp, Ki, Kd. Ki [1/s], Kd [s] si el error se expresa en RPM
 * y las ganancias se normalizan a porcentaje de salida por RPM.
 * dt_s: periodo de muestreo en segundos (> 0, previsto 0.010).
 * Devuelve false ante dt_s <= 0 o ganancias negativas.
 */
#include <stdbool.h>
bool calculo_pid_configurar(float kp, float ki, float kd, float dt_s);

/* Borra acumulador y error previo. Llamar al arrancar/reiniciar. */
void calculo_pid_reiniciar(void);

/* referencia_rpm y medida_rpm: RPM con signo del eje de salida.
 * Devuelve mando firmado en porcentaje [-100, 100].
 * Llamar cada 10 ms en la futura etapa de lazo cerrado.
 */
float calculo_pid_actualizar(float referencia_rpm, float medida_rpm);

#endif