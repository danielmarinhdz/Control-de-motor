#ifndef MAQUINA_ESTADOS_H
#define MAQUINA_ESTADOS_H

#include <stdbool.h>

typedef enum {
    ESTADO_INIT = 0,
    ESTADO_IDLE,
    ESTADO_RUN,
    ESTADO_FAULT
} maquina_estados_estado_t;

/* Inicia en INIT. Llamar una vez al arrancar; sin unidades ni errores. */
void maquina_estados_inicializar(void);
/* INIT -> IDLE cuando la inicializacion termina. Devuelve false si no procede. */
bool maquina_estados_listo(void);
/* IDLE -> RUN; false si no se permite arrancar. */
bool maquina_estados_arrancar(void);
/* RUN -> IDLE; false si no esta en RUN. */
bool maquina_estados_detener(void);
/* Cualquier estado -> FAULT. El coordinador debe apagar el motor inmediatamente. */
void maquina_estados_reportar_falla(void);
/* FAULT -> IDLE SOLO por accion manual tras resolver la falla. No arranca motor. */
bool maquina_estados_reiniciar_manual(void);
/* Consulta estado actual, sin unidades. */
maquina_estados_estado_t maquina_estados_obtener(void);
/* true solo en RUN; util para permitir el PWM. */
bool maquina_estados_permite_motor(void);

#endif
