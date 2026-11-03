#ifndef LECTOR_ENCODER_H
#define LECTOR_ENCODER_H

#include <stdint.h>

/* Capa HAL. Unico trabajo: leer el contador del encoder (TIM3, modo TI12).
 * Devuelve: cuenta cruda de 16 bits (0..65535); se desborda sola.
 * Cuando: la llama la ISR de 10 ms (control_velocidad). Es una sola lectura.
 * Requisito: HAL_TIM_Encoder_Start(&htim3, TIM_CHANNEL_ALL) ya hecho en main.c.
 */
uint16_t lector_encoder_leer(void);

#endif
