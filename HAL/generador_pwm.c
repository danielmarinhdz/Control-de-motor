#include "generador_pwm.h"
#include "main.h"

/* htim1 lo crea CubeMX en main.c; aqui solo lo usamos. */
extern TIM_HandleTypeDef htim1;

/* CubeMX genero nombres poco claros para los enables; les damos alias. */
#define R_EN_PUERTO  GPIO_OutputA10_GPIO_Port   /* PA10 */
#define R_EN_PIN     GPIO_OutputA10_Pin
#define L_EN_PUERTO  GPIO_Output_GPIO_Port      /* PB10 */
#define L_EN_PIN     GPIO_Output_Pin

#define CANAL_RPWM   TIM_CHANNEL_1              /* PA8 */
#define CANAL_LPWM   TIM_CHANNEL_2              /* PA9 */

#define PORCENTAJE_MAX  100.0f

/* Pone ambos canales en duty 0 (sin tocar los enables). */
static void apagar_canales(void)
{
    __HAL_TIM_SET_COMPARE(&htim1, CANAL_RPWM, 0);
    __HAL_TIM_SET_COMPARE(&htim1, CANAL_LPWM, 0);
}

static void escribir_enables(GPIO_PinState estado)
{
    HAL_GPIO_WritePin(R_EN_PUERTO, R_EN_PIN, estado);
    HAL_GPIO_WritePin(L_EN_PUERTO, L_EN_PIN, estado);
}

void generador_pwm_iniciar(void)
{
    apagar_canales();
    escribir_enables(GPIO_PIN_RESET);
}

generador_pwm_resultado_t generador_pwm_fijar(float porcentaje)
{
    generador_pwm_resultado_t resultado = GENERADOR_PWM_OK;

    /* 1. Saturar a [-100, 100] */
    if (porcentaje > PORCENTAJE_MAX) {
        porcentaje = PORCENTAJE_MAX;
        resultado = GENERADOR_PWM_SATURADO;
    } else if (porcentaje < -PORCENTAJE_MAX) {
        porcentaje = -PORCENTAJE_MAX;
        resultado = GENERADOR_PWM_SATURADO;
    }

    /* 2. Convertir |%| a cuentas de CCR. Se lee ARR del timer para no
     *    depender de que el Period sea 8999: con ARR = 8999, 100 % = 9000. */
    float magnitud = (porcentaje >= 0.0f) ? porcentaje : -porcentaje;
    uint32_t periodo = __HAL_TIM_GET_AUTORELOAD(&htim1) + 1U;
    uint32_t duty = (uint32_t)(magnitud * (float)periodo / PORCENTAJE_MAX + 0.5f);

    /* 3. Primero apagar ambos canales; asi, al cambiar de sentido, nunca hay
     *    un instante con RPWM y LPWM activos a la vez. */
    apagar_canales();
    escribir_enables(GPIO_PIN_SET);

    /* 4. Activar solo el canal del sentido pedido */
    if (porcentaje > 0.0f) {
        __HAL_TIM_SET_COMPARE(&htim1, CANAL_RPWM, duty);
    } else if (porcentaje < 0.0f) {
        __HAL_TIM_SET_COMPARE(&htim1, CANAL_LPWM, duty);
    }
    /* porcentaje == 0: ambos canales quedan en 0 */

    return resultado;
}

void generador_pwm_detener(void)
{
    apagar_canales();
    escribir_enables(GPIO_PIN_RESET);
}
