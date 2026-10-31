#ifndef GENERADOR_PWM_H
#define GENERADOR_PWM_H

/* Capa HAL. Unico trabajo: convertir un porcentaje con signo en el PWM de
 * TIM1 (CH1 = RPWM en PA8, CH2 = LPWM en PA9) y manejar R_EN (PA10) y
 * L_EN (PB10) del BTS7960.
 *
 * Convencion de signo (igual que calculo_rpm):
 *   porcentaje > 0 -> RPWM activo, LPWM en 0  (sentido "adelante")
 *   porcentaje < 0 -> LPWM activo, RPWM en 0  (sentido "atras")
 *   porcentaje = 0 -> ambos en 0              (motor sin par)
 * Nunca hay PWM en los dos canales al mismo tiempo.
 *
 * Requisito: TIM1 CH1 y CH2 ya arrancados con HAL_TIM_PWM_Start en main.c.
 * Este modulo no conoce a ningun otro modulo; solo lo llama control_velocidad.
 */

typedef enum {
    GENERADOR_PWM_OK = 0,   /* El porcentaje se aplico tal cual */
    GENERADOR_PWM_SATURADO  /* Venia fuera de [-100, 100]; se aplico el limite */
} generador_pwm_resultado_t;

/* Deja el puente en estado seguro: duty 0 en ambos canales y R_EN/L_EN en bajo.
 * Recibe: nada. Devuelve: nada.
 * Cuando: una vez al arrancar (INIT), antes de cualquier generador_pwm_fijar().
 * Errores: ninguno.
 */
void generador_pwm_iniciar(void);

/* Aplica un mando al motor.
 * Recibe: porcentaje [%] con signo, rango valido [-100.0, 100.0].
 *         Fuera de rango se satura al limite mas cercano.
 * Devuelve: GENERADOR_PWM_OK o GENERADOR_PWM_SATURADO.
 * Efecto: pone R_EN y L_EN en alto (el BTS7960 necesita ambos para girar
 *         en cualquier sentido) y duty = |porcentaje| * (ARR + 1) / 100.
 * Cuando: cada 10 ms desde control_velocidad, solo en estado RUN.
 */
generador_pwm_resultado_t generador_pwm_fijar(float porcentaje);

/* Apaga el motor: duty 0 en ambos canales y R_EN/L_EN en bajo.
 * Recibe: nada. Devuelve: nada.
 * Cuando: al pasar a IDLE o FAULT. Se puede llamar en cualquier momento,
 *         las veces que sea; siempre deja el motor parado.
 * Errores: ninguno.
 */
void generador_pwm_detener(void);

#endif
