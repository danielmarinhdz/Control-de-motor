#ifndef COMANDO_SERIAL_H
#define COMANDO_SERIAL_H

/* Capa Application. Unico trabajo: recibir por el puerto serie (USART2, el
 * puerto virtual del ST-LINK) una referencia de velocidad escrita en la
 * terminal y pasarla a control_velocidad.
 *
 * Formato: un numero en RPM terminado en Enter (CR, LF o CR+LF), por ejemplo
 *     150      -120      180.5
 * Respuesta por la misma terminal:
 *     ref=150.0 OK        se guardo tal cual
 *     ref=342.0 LIMITE    venia fuera de rango; se guardo el limite
 *     ref=ERROR           no era un numero; la referencia no cambia
 *
 * Requiere en CubeMX: USART2 -> NVIC Settings -> "USART2 global interrupt"
 * activado. La interrupcion solo guarda cada caracter en una cola; el numero
 * se interpreta en el while(1). Si USART2 no esta activado, el modulo compila
 * y no hace nada.
 */

/* Empieza a escuchar el puerto serie.
 * Cuando: una vez en main.c (USER CODE BEGIN 2), despues de MX_USART2_UART_Init().
 */
void comando_serial_iniciar(void);

/* Procesa los caracteres que llegaron; si hay una linea completa, fija la
 * referencia y contesta. Si no hay nada, regresa de inmediato.
 * Cuando: en cada vuelta del while(1) de main.c, nunca desde una ISR.
 */
void comando_serial_actualizar(void);

#endif
