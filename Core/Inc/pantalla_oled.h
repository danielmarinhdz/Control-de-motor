#ifndef PANTALLA_OLED_H
#define PANTALLA_OLED_H

#include <stdbool.h>
#include "stm32f4xx_hal.h"

/* OLED SSD1306 128x64: I2C1, PB8=SCL y PB9=SDA (confirmados).
 * Alimentacion: VCC=3V3 y GND=GND. Direccion 7-bit 0x3C
 * (verificar direccion real del modulo; algunas OLED usan 0x3D).
 * hi2c: &hi2c1 configurado en CubeMX por Dani; llamar desde main, NO ISR.
 * Devuelve false si falla la comunicacion I2C.
 */
bool pantalla_oled_inicializar(I2C_HandleTypeDef *hi2c);

/* Muestra RPM con signo del eje de salida. Llamar ~cada 100 ms
 * desde control_velocidad, nunca desde interrupcion.
 * Devuelve false si I2C falla; la falla NO debe detener el motor.
 */
bool pantalla_oled_mostrar_rpm(float rpm);

#endif