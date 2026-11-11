#ifndef TELEMETRIA_SERIAL_H
#define TELEMETRIA_SERIAL_H

/* Capa Application. Unico trabajo: mandar por el puerto serie (USART2, el
 * puerto virtual del ST-LINK) una linea cada 100 ms con el estado, las RPM
 * medidas y la cuenta cruda del encoder:
 *
 *     est=RUN rpm=-123.4 cnt=51234
 *
 * Solo LEE de los demas modulos (control_velocidad, maquina_estados,
 * lector_encoder); no cambia nada del control.
 *
 * Requiere activar USART2 en CubeMX (Asynchronous, 115200 8N1). Si USART2 no
 * esta activado, este modulo compila y no hace nada.
 */

/* Llamar en cada vuelta del while(1) de main.c, nunca desde una ISR.
 * Revisa el tiempo solo; si no han pasado 100 ms regresa de inmediato. */
void telemetria_serial_actualizar(void);

#endif
