/**
 *
 * Descripcion: gestiona alguna de las funciones de los procesos nave, entre ellos 
 *		el manejador de la señal SIGTERM, la creación de la estructura nave o el 
 *		rastreador de naves cercanas.
 *
 * Fichero: nave.c
 * Autor: Miguel González Bustamante, miguel.gonzalezb@estudiante.uam.es
 * Grupo: 2261
 * Fecha: 08-05-2019
 *
 */

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <mqueue.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <math.h>
#include <stdbool.h>
#include <unistd.h>
#include <semaphore.h>
#include <mapa.h>
#include <simulador.h>

/****************************************************************************/
/* Funcion: manejador_SIGTERM                                               */
/*                                                                          */
/* Descripcion: rutina de tratamiento de la señal SIGTERM.                  */
/*                                                                          */
/* Parametros de entrada:                                                   */
/*		int *sig: señal SIGTERM                                             */
/*                                                                          */
/* Parametros de salida: void                                               */
/****************************************************************************/
void manejador_SIGTERM(int sig) {
	exit(EXIT_SUCCESS);
}

/****************************************************************************/
/* Funcion: manejador_SIGTERM_create                                        */
/*                                                                          */
/* Descripcion: inicializa los parámetros de la estructura sigaction para   */
/*		enlazarlo con el manejador_SIGTERM.                                 */
/*                                                                          */
/* Parametros de entrada:                                                   */
/*		struct sigaction act: estructura del manejador                      */
/*                                                                          */
/* Parametros de salida: retorna positivo si no se produce ningún error o   */
/*		negativo en caso contrario.                                         */
/****************************************************************************/
int manejador_SIGTERM_create(struct sigaction act){
	sigemptyset(&(act.sa_mask));
    act.sa_flags = 0;
    act.sa_handler = manejador_SIGTERM;
    if(sigaction(SIGTERM, &act, NULL) < 0) {
        return -1;
    }

    return 1;
}

/****************************************************************************/
/* Funcion: nave_create                                                     */
/*                                                                          */
/* Descripcion: se encarga de crear las estructuras de las naves que guardan*/
/*		su información y posicionarlas sobre el mapa.                       */
/*                                                                          */
/* Parametros de entrada:                                                   */
/*		int numEquipo: número del equipo                                    */
/*		int numNave: número de la nave                                      */
/*                                                                          */
/* Parametros de salida: retorna la estructura de la nave o NULL si no ha   */
/*		sido posible crearla.                                               */
/****************************************************************************/
tipo_nave *nave_create(int numEquipo, int numNave) {
	tipo_nave *nave = (tipo_nave*)malloc(sizeof(tipo_nave));
	if(nave == NULL)
		return NULL;

	if(N_EQUIPOS > 4) {
		/* Estos calculos son necesarios para centrar las naves */
		double spaceX_aux = MAPA_MAXX/N_EQUIPOS;
		int spaceX = (int)floor(spaceX_aux);
		int leftSpaceX = (int)floor((MAPA_MAXX - (N_EQUIPOS - 1) * spaceX_aux)/2);
		nave->posx = numEquipo*spaceX + leftSpaceX;

		double spaceY_aux = MAPA_MAXY/N_EQUIPOS;
		int spaceY = (int)floor(spaceY_aux);
		int leftSpaceY = (int)floor((MAPA_MAXY - (N_NAVES - 1) * spaceY_aux)/2);
		nave->posy = numNave*spaceY + leftSpaceY;
	} 

	else {
		int sqrtNaves = 2;
		if(N_NAVES > 3)
			sqrtNaves = (int)floor(sqrt(N_NAVES));

		if(numEquipo == 0) {
			nave->posx = numNave%sqrtNaves;
			nave->posy = (int)floor(numNave/sqrtNaves);
		} 

		else if(numEquipo == 1) {
			nave->posx = (MAPA_MAXX - 1) - numNave%sqrtNaves;
			nave->posy = (int)floor(numNave/sqrtNaves);
		} 

		else if(numEquipo == 2) {
			nave->posx = numNave%sqrtNaves;
			nave->posy = (MAPA_MAXY - 1) - (int)floor(numNave/sqrtNaves);
		} 

		else if(numEquipo == 3) {
			nave->posx = (MAPA_MAXX - 1) - numNave%sqrtNaves;
			nave->posy = (MAPA_MAXY - 1) - (int)floor(numNave/sqrtNaves);
		}
	}

	nave->vida = VIDA_MAX;
	nave->equipo = numEquipo;
	nave->numNave = numNave;
	nave->viva = true;
	
	return nave;
}

/****************************************************************************/
/* Funcion: nave_rastrear                                                   */
/*                                                                          */
/* Descripcion: se encarga de rastrear la nave más cercana.                 */
/*                                                                          */
/* Parametros de entrada:                                                   */
/*		tipo_mapa *mapa: estructura del mapa                                */
/*		tipo_nave *nave: esrtuctura de la nave                              */
/*		int i: número de la nave                                            */
/*                                                                          */
/* Parametros de salida: retorna la estructura de la nave.                  */
/****************************************************************************/
tipo_nave nave_rastrear(tipo_mapa *mapa, tipo_nave *nave, int i) {
	tipo_nave nave_rastreada;
	tipo_nave nave_enemiga;
	nave_rastreada.equipo = -1;
	int numEquipoEnemigo;

	numEquipoEnemigo = rand() % N_EQUIPOS;

	for(int cont = 0; cont < N_EQUIPOS; cont++, numEquipoEnemigo++) {
		if(numEquipoEnemigo == N_EQUIPOS)
			numEquipoEnemigo = 0;
		/* Solo se actua sobre enemigos */
		if(numEquipoEnemigo != i) {
			for(int numNaveEnemiga = 0; numNaveEnemiga < N_NAVES; numNaveEnemiga++) {
				nave_enemiga = mapa_get_nave(mapa, numEquipoEnemigo, numNaveEnemiga);
				if((nave_rastreada.equipo == -1) && (nave_enemiga.viva == true)) {
					nave_rastreada = mapa_get_nave(mapa, numEquipoEnemigo, numNaveEnemiga);
				} else if((mapa_get_distancia(mapa, nave->posy, nave->posx, nave_enemiga.posy, nave_enemiga.posx) < 
					mapa_get_distancia(mapa, nave->posy, nave->posx, nave_rastreada.posy, nave_rastreada.posx))
					&& nave_enemiga.viva == true) {
					nave_rastreada = mapa_get_nave(mapa, numEquipoEnemigo, numNaveEnemiga);
				}
			}	
		}
	}
	return nave_rastreada;
}

/****************************************************************************/
/* Funcion: nave_atacar                                                     */
/*                                                                          */
/* Descripcion: se encarga de comprobar si la nave más cercana está a una   */
/*		distancia de alcance.                                               */
/*                                                                          */
/* Parametros de entrada:                                                   */
/*		tipo_mapa *mapa: estructura del mapa                                */
/*		tipo_nave *nave: esrtuctura de la nave                              */
/*		int i: número de la nave                                            */
/*                                                                          */
/* Parametros de salida: retorna la estructura de la nave.                  */
/****************************************************************************/
tipo_nave nave_atacar(tipo_mapa *mapa, tipo_nave *nave, int i) {
	tipo_nave nave_enemiga, nave_enemiga_aux;
	int distancia = 0;

	nave_enemiga_aux = nave_rastrear(mapa, nave, i);
	if(((distancia = mapa_get_distancia(mapa, nave->posy, nave->posx, nave_enemiga_aux.posy, nave_enemiga_aux.posx))< ATAQUE_ALCANCE)
		&& nave_enemiga_aux.viva == true) {
		return nave_enemiga_aux;
	}
	nave_enemiga.equipo = -1;
	return nave_enemiga;
}

/****************************************************************************/
/* Funcion: nave_seguirX                                                    */
/*                                                                          */
/* Descripcion: se encarga de retornar la dirección en la coordenada 'x'    */
/*		en la que se desplaza la nave.                                      */
/*                                                                          */
/* Parametros de entrada:                                                   */
/*		tipo_nave *nave: esrtuctura de la nave                              */
/*		tipo_nave nave_enemiga: esrtuctura de la nave enemiga               */
/*                                                                          */
/* Parametros de salida: retorna la dirección en la que se desplaza la nave.*/
/****************************************************************************/
int nave_seguirX(tipo_nave *nave, tipo_nave nave_enemiga) {
	if(nave->posx < nave_enemiga.posx) 
		return 1;
	else if(nave->posx > nave_enemiga.posx)
		return -1;
	else
		return 0;
}

/****************************************************************************/
/* Funcion: nave_seguirY                                                    */
/*                                                                          */
/* Descripcion: se encarga de retornar la dirección en la coordenada 'y'    */
/*		en la que se desplaza la nave.                                      */
/*                                                                          */
/* Parametros de entrada:                                                   */
/*		tipo_nave *nave: esrtuctura de la nave                              */
/*		tipo_nave nave_enemiga: esrtuctura de la nave enemiga               */
/*                                                                          */
/* Parametros de salida: retorna la dirección en la que se desplaza la nave.*/
/****************************************************************************/
int nave_seguirY(tipo_nave *nave, tipo_nave nave_enemiga) {
	if(nave->posy < nave_enemiga.posy) 
		return 1;
	else if(nave->posy > nave_enemiga.posy)
		return -1;
	else
		return 0;
}