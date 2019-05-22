#ifndef SRC_NAVE_H_
#define SRC_NAVE_H_

#include <stdbool.h>

/* rutina de tratamiento de la señal SIGTERM */
void manejador_SIGTERM(int sig);

/*inicializa los parámetros de la estructura sigaction para enlazarlo con el manejador_SIGTERM */
int manejador_SIGTERM_create(struct sigaction act);

/* Crea la estructura tipo_Nave otorgandole cierta posición en el mapa */
tipo_nave *nave_create(int numEquipo, int numNave);

/* Controla las acciones que realiza la nave */
void nave_update(tipo_nave *nave);

/* Comprueba si es posible que la nave en cuestión realice una acción de ataque */
tipo_nave nave_atacar(tipo_mapa *mapa, tipo_nave *nave, int i);

/* Rastrea la nave más cercana para moverse hacia ella */
tipo_nave nave_rastrear(tipo_mapa *mapa, tipo_nave *nave, int i);

int nave_seguirX(tipo_nave *nave, tipo_nave nave_enemiga);

int nave_seguirY(tipo_nave *nave, tipo_nave nave_enemiga);

#endif /* SRC_NAVE_H_ */