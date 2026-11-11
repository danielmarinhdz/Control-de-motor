#ifndef CALCULO_RPM_H
#define CALCULO_RPM_H

#include <stdint.h>

/* Capa Application. Unico trabajo: convertir cuentas del encoder en RPM
 * del eje de salida, con signo (+ = mismo sentido que RPWM, ver SIGNO).
 *
 * CUENTAS_POR_VUELTA = cuentas del contador por 1 vuelta del EJE DE SALIDA
 *   = pulsos del encoder por vuelta del motor x 4 (modo TI12) x relacion de la caja.
 * VALOR MEDIDO: ~685 cuentas por vuelta del eje de salida. Salio de dos pruebas
 *    con el motor girando y las vueltas contadas: 10 vueltas = 6710 cuentas (671)
 *    y 15 vueltas = 10272 cuentas (685). El supuesto original (11 x 4 x 34 = 1496)
 *    y la medicion de una sola vuelta a mano (768) estaban mal.
 *    Incertidumbre ~2-3 %. Para afinar: una corrida mas larga (30 vueltas).
 * SIGNO = -1: el encoder cuenta hacia abajo cuando el motor gira en el sentido
 *    de RPWM (visto por serial con referencia positiva).
 */
#define CALCULO_RPM_CUENTAS_POR_VUELTA  685.0f
#define CALCULO_RPM_SIGNO               -1.0f   /* poner 1.0f si sale al reves */

/* Guarda la cuenta inicial como referencia. Recibe: cuenta actual del encoder.
 * Cuando: una vez, al arrancar (control_velocidad_iniciar). */
void calculo_rpm_iniciar(uint16_t cuenta);

/* Recibe: cuenta actual y dt_s [s] desde la llamada anterior (> 0).
 * Devuelve: RPM con signo del eje de salida. Si dt_s <= 0 devuelve 0.
 * Maneja el desbordamiento del contador de 16 bits y el sentido de giro. */
float calculo_rpm_actualizar(uint16_t cuenta, float dt_s);

#endif
