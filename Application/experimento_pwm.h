#ifndef EXPERIMENTO_PWM_H
#define EXPERIMENTO_PWM_H

#include <stdint.h>

/* Capa Application. Unico trabajo: los dos experimentos de identificacion del
 * motor (respuesta al escalon y PRBS9) para la actividad de modelado.
 *
 * COMO SE USA (con la placa ya flasheada):
 *   1a pulsacion del boton azul -> prueba del ESCALON (8 s).
 *   2a pulsacion del boton azul -> prueba PRBS9 (32 s).
 *   (despues vuelve a alternar: escalon, PRBS9, escalon, ...)
 *   Espera a que termine de mandar los datos ("EXP,FIN") antes de volver a presionar.
 *
 * COMO FUNCIONA
 *   - En CADA ciclo de 10 ms (el de TIM2 / control_velocidad) guarda en RAM la
 *     cuenta del encoder y el PWM aplicado. Durante la prueba NO manda nada por
 *     el puerto serie, asi el muestreo queda parejo.
 *   - Al terminar apaga el motor y manda todo por USART2 (115200):
 *
 *       EXP,INICIO,run=1,modo=1,quien=1,bajo=38,alto=74,ts_ms=10,n=800,ini=400,cpv=685,sgn=-1,bitm=0,perdidas=0
 *       EXP,0,0,51234        <- EXP,<muestra k>,<PWM %>,<cuenta del encoder>
 *       ...
 *       EXP,FIN
 *
 *     t = k x 10 ms. pwm[k] es el PWM que se aplica DESPUES de medir la muestra k
 *     y hasta la k+1. Las RPM se calculan en la computadora (analizar_motor.py).
 *   - Presionar el boton a la mitad aborta ("EXP,ABORT"); la siguiente pulsacion
 *     repite la MISMA prueba.
 *
 * Solo usa la API publica de control_velocidad. Es para LAZO ABIERTO: cuando
 * entre el PID, poner EXPERIMENTO_PWM_HABILITADO en 0 y el modulo queda vacio.
 */

#define EXPERIMENTO_PWM_HABILITADO  1

/* ---------------------------------------------------------------------------
 * QUIEN CORRE EL EXPERIMENTO: cambia SOLO esta linea, compila y flashea.
 * Niveles de PWM unicos por integrante (ninguno se repite entre personas):
 *
 *   Integrante   Nivel BAJO   Nivel ALTO
 *   Daniel          38 %         74 %
 *   Mateo           37 %         78 %
 *   Angel           41 %         71 %
 *
 *   Escalon: 1 s en 0 %, 3 s en BAJO, 4 s en ALTO (el modelo usa el salto BAJO -> ALTO).
 *   PRBS9  : 1.5 s en BAJO y luego 511 bits que saltan entre BAJO y ALTO.
 * ------------------------------------------------------------------------- */
#define INTEGRANTE_DANIEL  1
#define INTEGRANTE_MATEO   2
#define INTEGRANTE_ANGEL   3

#define EXPERIMENTO_INTEGRANTE      INTEGRANTE_DANIEL

/* Duracion de cada bit de la PRBS9, en ciclos de 10 ms. Con 6 son 60 ms, unas
 * 2 constantes de tiempo del motor (~25-30 ms), y la secuencia dura 30.7 s. */
#define EXPERIMENTO_PRBS_BIT_MUESTRAS  6

/* Deja el modulo en espera y la referencia en 0.
 * Cuando: una vez en main.c, DESPUES de control_velocidad_iniciar(). */
void experimento_pwm_iniciar(void);

/* Gancho de muestreo. Lo llama control_velocidad_ejecutar() cada vez que pasan
 * uno o mas ciclos de 10 ms (nunca desde una ISR).
 * Recibe: cuenta = cuenta del encoder tomada por la ISR de 10 ms;
 *         periodos = ciclos de 10 ms transcurridos (normalmente 1). */
void experimento_pwm_muestrear(uint16_t cuenta, uint32_t periodos);

/* Envio por serie, una linea por llamada. Cuando: en cada vuelta del while(1),
 * nunca desde una ISR. Regresa de inmediato si no hay nada que enviar. */
void experimento_pwm_actualizar(void);

/* Devuelve: 1 mientras una prueba corre o se estan enviando los datos, 0 si no.
 * Para que main.c silencie la telemetria normal y no se mezcle con los datos. */
int experimento_pwm_activo(void);

#endif
