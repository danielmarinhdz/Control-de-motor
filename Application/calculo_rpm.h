#ifndef CALCULO_RPM_H
#define CALCULO_RPM_H

#include <stdint.h>

/* Capa Application. Unico trabajo: convertir cuentas del encoder en RPM
 * del eje de salida, con signo (+ = mismo sentido que RPWM, ver SIGNO).
 *
 * CUENTAS_POR_VUELTA = cuentas del contador por 1 vuelta del EJE DE SALIDA
 *   = pulsos del encoder por vuelta del motor x 4 (modo TI12) x relacion de la caja.
 * !! VALOR SUPUESTO (11 x 4 x 34). VERIFICAR: girar el eje de salida una vuelta
 *    a mano y leer TIM3->CNT en el debugger. Si da otro numero, cambiarlo aqui.
 */
#define CALCULO_RPM_CUENTAS_POR_VUELTA  1496.0f
#define CALCULO_RPM_SIGNO               1.0f   /* poner -1.0f si sale al reves */

/* Guarda la cuenta inicial como referencia. Recibe: cuenta actual del encoder.
 * Cuando: una vez, al arrancar (control_velocidad_iniciar). */
void calculo_rpm_iniciar(uint16_t cuenta);

/* Recibe: cuenta actual y dt_s [s] desde la llamada anterior (> 0).
 * Devuelve: RPM con signo del eje de salida. Si dt_s <= 0 devuelve 0.
 * Maneja el desbordamiento del contador de 16 bits y el sentido de giro. */
float calculo_rpm_actualizar(uint16_t cuenta, float dt_s);

#endif
