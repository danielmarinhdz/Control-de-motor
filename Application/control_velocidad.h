#ifndef CONTROL_VELOCIDAD_H
#define CONTROL_VELOCIDAD_H

/* Capa Application. Unico trabajo: cada 10 ms (TIM2) ejecutar
 * medir -> calcular -> actuar, guardar la referencia en RPM y refrescar
 * la pantalla cada ~100 ms. Es el unico modulo que conoce a los demas.
 *
 * Contiene el unico HAL_TIM_PeriodElapsedCallback del proyecto. La ISR solo
 * lee la cuenta del encoder y avisa; todo el trabajo corre en el while(1).
 *
 * RPM siempre del eje de salida y con signo (+ adelante, - atras).
 */

#include <stdbool.h>

#define CONTROL_VELOCIDAD_RPM_MAX  342.0f  /* Sin carga a 100 % con bateria de 11.1 V (con fuente de 12 V: ~369). Reescalado a 685 cuentas/vuelta */

/* Inicia maquina_estados, generador_pwm y calculo_rpm; deja el motor parado
 * y pasa de INIT a IDLE.
 * Recibe: nada. Devuelve: nada.
 * Cuando: una vez en main.c (USER CODE BEGIN 2), despues de arrancar los
 *         timers y de pantalla_oled_inicializar().
 * Errores: ninguno.
 */
void control_velocidad_iniciar(void);

/* Hace el ciclo de control si ya pasaron 10 ms; si no, regresa de inmediato.
 * Recibe: nada. Devuelve: nada.
 * Cuando: en cada vuelta del while(1) de main.c. Nunca desde una ISR.
 * Errores: si la pantalla falla, se ignora y el motor sigue.
 */
void control_velocidad_ejecutar(void);

/* Guarda la velocidad deseada.
 * Recibe: rpm [RPM] con signo, rango [-RPM_MAX, RPM_MAX].
 * Devuelve: true si se guardo tal cual; false si venia fuera de rango
 *           (en ese caso se guarda el limite mas cercano).
 * Cuando: en cualquier momento fuera de ISR. Se aplica en el siguiente ciclo.
 */
bool control_velocidad_fijar_referencia(float rpm);

/* Pide IDLE -> RUN. El motor empieza a moverse en el siguiente ciclo.
 * Devuelve: false si la maquina de estados no lo permite (ej. en FAULT).
 */
bool control_velocidad_arrancar(void);

/* Pide RUN -> IDLE y apaga el motor de inmediato (no espera al ciclo).
 * Devuelve: false si no estaba en RUN (el motor se apaga igual).
 */
bool control_velocidad_detener(void);

/* Devuelve: ultima velocidad medida [RPM] con signo. Cuando: cualquier momento. */
float control_velocidad_obtener_rpm(void);

#endif
