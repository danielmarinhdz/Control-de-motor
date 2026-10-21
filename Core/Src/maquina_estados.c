#include "maquina_estados.h"

static maquina_estados_estado_t estado_actual = ESTADO_INIT;

void maquina_estados_inicializar(void)
{
    estado_actual = ESTADO_INIT;
}

bool maquina_estados_listo(void)
{
    if (estado_actual != ESTADO_INIT) return false;
    estado_actual = ESTADO_IDLE;
    return true;
}

bool maquina_estados_arrancar(void)
{
    if (estado_actual != ESTADO_IDLE) return false;
    estado_actual = ESTADO_RUN;
    return true;
}

bool maquina_estados_detener(void)
{
    if (estado_actual != ESTADO_RUN) return false;
    estado_actual = ESTADO_IDLE;
    return true;
}

void maquina_estados_reportar_falla(void)
{
    estado_actual = ESTADO_FAULT;
}

bool maquina_estados_reiniciar_manual(void)
{
    if (estado_actual != ESTADO_FAULT) return false;
    estado_actual = ESTADO_IDLE;
    return true;
}

maquina_estados_estado_t maquina_estados_obtener(void)
{
    return estado_actual;
}

bool maquina_estados_permite_motor(void)
{
    return estado_actual == ESTADO_RUN;
}