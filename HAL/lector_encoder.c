#include "lector_encoder.h"
#include "main.h"

/* htim3 lo crea CubeMX en main.c; aqui solo lo usamos. */
extern TIM_HandleTypeDef htim3;

uint16_t lector_encoder_leer(void)
{
    return (uint16_t)__HAL_TIM_GET_COUNTER(&htim3);
}
