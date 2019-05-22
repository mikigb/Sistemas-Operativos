/**
 *
 * Descripcion: proceso simulador que se encarga de generar los procesos equipos
 *			y demás recursos necesarios para actualizar los datos del mapa.
 *
 * Fichero: simulador.c
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
#include <mapa.h>
#include <semaphore.h>
#include <nave.h>
#include <time.h>

/* Variables globales */
tipo_mapa *mapa;
int fd_shm;
bool alrm_flag = false;
int turno = 0;
mqd_t queue;
int fd1[N_EQUIPOS][2];
sem_t *sem_ctrl = NULL;

/****************************************************************************/
/* Funcion: manejador_SIGINT                                                */
/*                                                                          */
/* Descripcion: rutina de tratamiento de la señal SIGINT.                   */
/*                                                                          */
/* Parametros de entrada:                                                   */
/*		int *sig: señal SIGINT                                              */
/*                                                                          */
/* Parametros de salida: void                                               */
/****************************************************************************/
void manejador_SIGINT(int sig) {
	munmap(mapa, sizeof(*mapa));
	shm_unlink(SHM_MAP_NAME);
    mq_close(queue);
	mq_unlink(MQ_NAME);
	sem_close(sem_ctrl);
    sem_unlink(SEM_CTRL);
	while(wait(NULL) > 0);
	exit(EXIT_SUCCESS);
}

/****************************************************************************/
/* Funcion: manejador_SIGINT_create                                         */
/*                                                                          */
/* Descripcion: inicializa los parámetros de la estructura sigaction para   */
/*		enlazarlo con el manejador_SIGINT.                                  */
/*                                                                          */
/* Parametros de entrada:                                                   */
/*		struct sigaction act: estructura del manejador                      */
/*                                                                          */
/* Parametros de salida: retorna positivo si no se produce ningún error o   */
/*		negativo en caso contrario.                                         */
/****************************************************************************/
int manejador_SIGINT_create(struct sigaction act){
	sigemptyset(&(act.sa_mask));
    act.sa_flags = 0;
    act.sa_handler = manejador_SIGINT;
    if(sigaction(SIGINT, &act, NULL) < 0) {
        return -1;
    }

    return 1;
}

/****************************************************************************/
/* Funcion: shm_create                                                      */
/*                                                                          */
/* Descripcion: crea el segmento de memoria compartida necesario para       */
/*		gestionar el mapa.                                                  */
/*                                                                          */
/* Parametros de entrada:                                                   */
/*                                                                          */
/* Parametros de salida: retorna positivo si no se produce ningún error o   */
/*		negativo en caso contrario.                                         */
/****************************************************************************/
int shm_create() {
	/* Creación de la memoria compartida */
	fd_shm = shm_open(SHM_MAP_NAME, O_RDWR | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR);

	if(fd_shm == -1) {
		printf("ERROR DE SIMULADOR: creando el segmento de memoria compartida.\n");
		return -1;
	}

	/* Redimensionamiento de la memoria compartida */
	if(ftruncate(fd_shm, sizeof(tipo_mapa)) == -1) {
		printf("ERROR DE SIMULADOR: redimensionando el segmento de memoria compartida.\n");
		shm_unlink(SHM_MAP_NAME);
		return -1;
	}

	/* Mapeo de la memoria compartida */
	mapa = (tipo_mapa *)mmap(NULL, sizeof(*mapa), PROT_READ | PROT_WRITE, MAP_SHARED, fd_shm, 0);

	if(mapa == MAP_FAILED) {
		printf("ERROR DE SIMULADOR: mapeando el segmento de memoria compartida.\n");
		shm_unlink(SHM_MAP_NAME);
		return -1;
	}

	return 1;
}

/****************************************************************************/
/* Funcion: pipe_write                                                      */
/*                                                                          */
/* Descripcion: escribe en la tubería recibida el mensaje pasado.           */
/*                                                                          */
/* Parametros de entrada:                                                   */
/*		int *fd: pipe                                                       */
/*		char *buffer: mensaje a transmitir                                  */
/* Parametros de salida: retorna positivo si no se produce ningún error o   */
/*		negativo en caso contrario.                                         */
/****************************************************************************/
int pipe_write(int *fd, char *buffer) {
	close(fd[0]);
	if(write(fd[1], buffer, PIPE_MAXSIZE) < 0)
		return -1;
	return 1;
}

/****************************************************************************/
/* Funcion: pipe_read                                                       */
/*                                                                          */
/* Descripcion: lee de la tubería recibida y lo vuelca en el buffer.        */
/*                                                                          */
/* Parametros de entrada:                                                   */
/*		int *fd: pipe                                                       */
/*		char *buffer: destino del mensaje                                   */
/* Parametros de salida: retorna positivo si no se produce ningún error o   */
/*		negativo en caso contrario.                                         */
/****************************************************************************/
int pipe_read(int *fd, char *buffer) {
	close(fd[1]);
	if(read(fd[0], buffer, PIPE_MAXSIZE) < 0) 
		return -1;
	return 1;
}

/****************************************************************************/
/* Funcion: manejador_SIGALRM                                               */
/*                                                                          */
/* Descripcion: rutina de tratamiento de la señal SIGALRM. Se encarga de    */
/*		restaurar el mapa, comprobar si hay algún equipo ganador y enviar   */
/*		los mensajes de nuevo turno a los procesos 'jefes' por las tuberías.*/
/*                                                                          */
/* Parametros de entrada:                                                   */
/*		int *sig: señal SIGALRM                                             */
/*                                                                          */
/* Parametros de salida: void                                               */
/****************************************************************************/
void manejador_SIGALRM(int sig) {
	char buffer[PIPE_MAXSIZE];
	int campeon, flag = 0;

	/* Restaura el mapa dejando solo los símbolos que sean naves */
	mapa_restore(mapa);

	/* Comprueba si hay algún equipo ganador */
	for(int i = 0; i < N_EQUIPOS; i++) {
		if(mapa_get_num_naves(mapa, i) > 0) {
			campeon = i;
			flag++;
		}
	}

	/* Si hay un equipo ganador, lo notifica y envía la orden 'FIN' a todos los procesos 'jefes' */
	if(flag < 2) {
		fprintf(stdout, "****** EQUIPO GANADOR %c *******\n", campeon+65);

		sprintf(buffer, "FIN");
		for(int i = 0; i < N_EQUIPOS; i++) {
			if(pipe_write(fd1[i], buffer) < 0) {
				printf("ERROR DE SIMULADOR: escribiendo en la tubería.\n");
				exit(EXIT_FAILURE);
			}
		}

		/* Luego espera a que acaben su ejecución y libera los recursos */
		
		while(wait(NULL) > 0);

		munmap(mapa, sizeof(*mapa));
		shm_unlink(SHM_MAP_NAME);
	    mq_close(queue);
		mq_unlink(MQ_NAME);
		sem_close(sem_ctrl);
	    sem_unlink(SEM_CTRL);

		exit(EXIT_SUCCESS);
	}

	/* Si no hay un ganador, envía la orden 'TURNO' a los procesos 'jefes' */
	fprintf(stdout, "\nNew TURNO\n");
	sprintf(buffer, "TURNO");
	for(int i = 0; i < N_EQUIPOS; i++) {
		if(pipe_write(fd1[i], buffer) < 0) {
			printf("ERROR DE SIMULADOR: escribiendo en la tubería.\n");
			exit(EXIT_FAILURE);
		}
	}

	/* Vuelve a establecer la alarma */
	alarm(TURNO_SECS);
}

/****************************************************************************/
/* Funcion: manejador_SIGALRM_create                                        */
/*                                                                          */
/* Descripcion: inicializa los parámetros de la estructura sigaction para   */
/*		enlazarlo con el manejador_SIGALRM.                                 */
/*                                                                          */
/* Parametros de entrada:                                                   */
/*		struct sigaction act: estructura del manejador                      */
/*                                                                          */
/* Parametros de salida: retorna positivo si no se produce ningún error o   */
/*		negativo en caso contrario.                                         */
/****************************************************************************/
int manejador_SIGALRM_create(struct sigaction act){
	sigemptyset(&(act.sa_mask));
    act.sa_flags = 0;
    act.sa_handler = manejador_SIGALRM;
    if(sigaction(SIGALRM, &act, NULL) < 0) {
        return -1;
    }

    return 1;
}

/****************************************************************************/
/* Funcion: accion_moverAleatorioY                                          */
/*                                                                          */
/* Descripcion: genera un número aleatorio para el moviemiento de una nave  */
/* 		en la coordenada 'y'.                                               */
/*                                                                          */
/* Parametros de entrada:                                                   */
/*		int posY: posición de la coordena 'y'                               */
/* Parametros de salida: la posición de la coordenada 'y' a donde se        */
/*		desplaza.                                                           */
/****************************************************************************/
int accion_moverAleatorioY(int posY) {
	int maxY = 0, minY = 0;

	maxY = posY + MOVER_ALCANCE;
	minY = posY - MOVER_ALCANCE;

	if(posY >= MAPA_MAXY - 1)
		maxY = minY;
	else if(posY <= 0)
		minY = maxY;

	srand(time(NULL)*getpid());
	return (rand() % (maxY + 1 - minY)) + minY;
}

/****************************************************************************/
/* Funcion: accion_moverAleatorioX                                          */
/*                                                                          */
/* Descripcion: genera un número aleatorio para el moviemiento de una nave  */
/* 		en la coordenada 'x'.                                               */
/*                                                                          */
/* Parametros de entrada:                                                   */
/*		int posY: posición de la coordena 'x'                               */
/* Parametros de salida: la posición de la coordenada 'x' a donde se        */
/*		desplaza.                                                           */
/****************************************************************************/
int accion_moverAleatorioX(int posX) {
	int maxX = 0, minX = 0;

	maxX = posX + MOVER_ALCANCE;
	minX = posX - MOVER_ALCANCE;

	if(posX >= MAPA_MAXX - 1)
		maxX = minX;
	else if(posX <= 0)
		minX = maxX;

	srand(time(NULL)*getpid());
	return (rand() % (maxX + 1 - minX)) + minX;
}

/****************************************************************************/
/* Funcion: simulador_update                                                */
/*                                                                          */
/* Descripcion: esta función se encarga de procesar los mensajes que le     */
/*		le llegan al simulador por la cola de mensajes y de actualizar      */
/*		el mapa en función a esos parámetros.                               */
/*                                                                          */
/* Parametros de entrada:                                                   */
/*		tipo_accion accion: estructura que almacena los datos de una acción */
/* Parametros de salida: void                                               */
/****************************************************************************/
void simulador_update(tipo_accion accion) {
	char buffer[PIPE_MAXSIZE];
	tipo_nave nave;
	nave = mapa_get_nave(mapa, accion.equipo, accion.nave);
	
	/* Puede ocurrir que otro proceso destruya esta nave en el mismo turno y quede algún mensaje en la cola */	
	if(nave.viva == true) {
		if(strcmp(accion.tipo, "ACCION MOVER") == 0) {
			/* Con esta comprobación se pretende frenar los movimientos que invaden posiciones ocupadas */
			if(mapa_is_casilla_vacia(mapa, accion.desY, accion.desY) == false) {
				fprintf(stdout, "%s [%c%d] %d,%d -> %d,%d: fallo\n", accion.tipo, accion.equipo+65, accion.nave, accion.oriY, accion.oriX, accion.desY, accion.desX);
			} else {
				/* Si no está ocupada, se mueve a dicha posición */
				tipo_casilla casilla;
				casilla = mapa_get_casilla(mapa, accion.desY, accion.desX);
				if(casilla.equipo < 0) {
					mapa_clean_casilla(mapa, nave.posy, nave.posx);
					nave.posy = accion.desY;
					nave.posx = accion.desX;
					mapa_set_nave(mapa, nave);
					fprintf(stdout, "%s [%c%d] %d,%d -> %d,%d: éxito\n", accion.tipo, accion.equipo+65, accion.nave, accion.oriY, accion.oriX, accion.desY, accion.desX);
				} else {
					fprintf(stdout, "%s [%c%d] %d,%d -> %d,%d: fallo\n", accion.tipo, accion.equipo+65, accion.nave, accion.oriY, accion.oriX, accion.desY, accion.desX);
				}
			}
		} else if(strcmp(accion.tipo, "ACCION ATAQUE") == 0) {

			/* Envía un misil */
			mapa_send_misil(mapa, accion.oriY, accion.oriX, accion.desY, accion.desX);

			tipo_casilla casilla;
			casilla = mapa_get_casilla(mapa, accion.desY, accion.desX);

			/* Si la casilla está vacía se marca como agua */
			if(casilla.equipo == -1 || casilla.equipo == accion.equipo) {
				mapa_set_symbol(mapa, accion.desY, accion.desX, SYMB_AGUA);
				fprintf(stdout, "%s [%c%d] %d,%d -> %d,%d: FALLIDO: Casilla target vacia\n", accion.tipo, accion.equipo+65, accion.nave, accion.oriY, accion.oriX, accion.desY, accion.desX);
			} else {
					
				tipo_nave nave_enemiga;
				nave_enemiga = mapa_get_nave(mapa, casilla.equipo, casilla.numNave);
				nave_enemiga.vida -= ATAQUE_DANO;
				/* Se comprueba la vida y si es menor que cero se envía destruir la nave */
				if(nave_enemiga.vida <= 0) {
					nave_enemiga.viva = false;
					mapa_set_nave(mapa, nave_enemiga);
					mapa_set_symbol(mapa, nave_enemiga.posy, nave_enemiga.posx, SYMB_DESTRUIDO);
					fprintf(stdout, "%s [%c%d] %d,%d -> %d,%d: target destruido\n", accion.tipo, accion.equipo+65, accion.nave, accion.oriY, accion.oriX, accion.desY, accion.desX);
					bzero(buffer, sizeof(buffer));
					sprintf(buffer, "DESTRUIR <%d>", nave_enemiga.numNave);
					if(pipe_write(fd1[nave_enemiga.equipo], buffer) < 0) {
						printf("ERROR DE SIMULADOR: escribiendo en la tubería.\n");
						exit(EXIT_FAILURE);
					}
				} else {
					/* Si no se marca como tocado */
					mapa_set_nave(mapa, nave_enemiga);
					fprintf(stdout, "%s [%c%d] %d,%d -> %d,%d: target a %d de vida\n", accion.tipo, accion.equipo+65, accion.nave, accion.oriY, accion.oriX, accion.desY, accion.desX, nave_enemiga.vida);
					mapa_set_symbol(mapa, nave_enemiga.posy, nave_enemiga.posx, SYMB_TOCADO);
				}

			}
		}
	}
}


int main() {

	pid_t PIDjefe, PIDnave;
	struct sigaction act_SIGINT, act_SIGALRM;
	int pipe_status;
	char buffer[PIPE_MAXSIZE];

	/* Se establecen los atributos de la cola de mensajes */
	struct mq_attr attributes = {
		.mq_flags = 0,
		.mq_maxmsg = 10,
		.mq_curmsgs = 0,
		.mq_msgsize = QUEUE_MAXSIZE
	};

	/* Se crea la cola de mensajes */
	fprintf(stdout, "Simulador gestionando MQ\n");
	queue = mq_open(MQ_NAME, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR, &attributes);
	if(queue == (mqd_t)-1) {
		printf("ERROR DE SIMULADOR: creando la cola de mensajes.\n");
		exit(EXIT_FAILURE);
	}

	/* Creación de la memoria compartida para el mapa */
	fprintf(stdout, "Simulador gestionando SHM\n");
	if(shm_create() < 0) {
		exit(EXIT_FAILURE);
	}

	/* Semáforo de control para gestionar el mapa */
	fprintf(stdout, "Simulador gestionando SEM (control)\n");
	if ((sem_ctrl = sem_open(SEM_CTRL, O_CREAT, S_IRUSR | S_IWUSR, 0)) == SEM_FAILED) {
        printf("ERROR DE SIMULADOR: creando el semaforo de cola de mensajes.\n");
		exit(EXIT_FAILURE);
    }

    sem_post(sem_ctrl);

    /* Creación de la tubería SIMULADOR-JEFES */
    fprintf(stdout, "Simulador gestionando PIPES (Simulador-Jefes)\n");
    for(int i = 0; i < N_EQUIPOS; i++) {
	    pipe_status = pipe(fd1[i]);
		if(pipe_status == -1) {
			printf("ERROR DE SIMULADOR: creando la tuberia SIMULADOR-JEFES.\n");
			exit(EXIT_FAILURE);
		}
	}

	/* Iniciar las casillas del mapa como vacías */
	fprintf(stdout, "Inicializando el mapa\n");
	for(int i = 0; i < MAPA_MAXX; i++) {
		for(int j = 0; j < MAPA_MAXY; j++) {
			mapa_clean_casilla(mapa, j, i);
		}
	}

	for(int i = 0; i < N_EQUIPOS; i++) {
		PIDjefe = fork();
        if(PIDjefe < 0) {
            printf("ERROR DE SIMULADOR: creando el EQUIPO <%d>.\n", i);
            exit(EXIT_FAILURE);
        }

        else if(PIDjefe == 0) {

        	int fd2[N_NAVES][2];
        	int pid_naves[N_NAVES];
			int pipe_status;

		    /* Creación de la tubería SIMULADOR-JEFES */
		    for(int i = 0; i < N_NAVES; i++) {
			    pipe_status = pipe(fd2[i]);
				if(pipe_status == -1) {
					printf("ERROR DE JEFE: creando la tuberia JEFES-NAVES.\n");
					exit(EXIT_FAILURE);
				}
			}

			/* Ajustar el número de naves de cada equipo */
			mapa_set_num_naves(mapa, i, N_NAVES);

        	for(int j = 0; j < N_NAVES; j++) {

				PIDnave = fork();
		        if(PIDnave < 0) {
		            printf("ERROR DE SIMULADOR: creando la NAVE <%d>.\n", j);
		            exit(EXIT_FAILURE);
		        }

		        else if(PIDnave == 0) {

		        	struct sigaction act_SIGTERM;

					/* Creación del manejador encargado de capturar SIGTERM */
					if(manejador_SIGTERM_create(act_SIGTERM) < 0) {
					    printf("ERROR DE NAVE: creando el manejador_SIGTERM.\n");
					    exit(EXIT_FAILURE);
					}

					/* Creación de una nave */
		        	tipo_nave *nave = nave_create(i, j);
		        	if(nave == NULL) {
		        		printf("ERROR DE NAVE: creando la estructura nave.\n");
						exit(EXIT_FAILURE);
		        	}

		        	mapa_set_nave(mapa, *nave);

		        	int flag = 1;
		        	while(1) {

		        		bzero(buffer, sizeof(buffer));
		        		if(pipe_read(fd2[j], buffer) < 0) {
						 	printf("ERROR DE NAVE: leyendo del pipe con el jefe.\n");
						 	exit(EXIT_FAILURE);
						}

						if(flag) {
							tipo_accion accion;
							tipo_nave nave_aux;

							nave_aux = mapa_get_nave(mapa, i, j);
							nave = &nave_aux;

							if(strcmp(buffer, "DESTRUIR") == 0) {
								mapa_set_num_naves(mapa, i, mapa_get_num_naves(mapa, i) - 1);
								flag = 0;

							} else if(strcmp(buffer, "ACCION ATAQUE") == 0) {
								tipo_nave nave_enemiga;

								nave_enemiga.equipo = -1;

								/* Si la nave se encunetra en posición de atacar */
								nave_enemiga = nave_atacar(mapa, nave, i);
								if(nave_enemiga.equipo != -1) {
									strcpy(accion.tipo, buffer);
									accion.equipo = i;
									accion.nave = j;
									accion.oriY = nave->posy;
									accion.oriX = nave->posx;
									accion.desY = nave_enemiga.posy;
									accion.desX = nave_enemiga.posx;

									if(mq_send(queue, (char*)&accion, sizeof(accion), 1) == -1) {
										printf("ERROR DE NAVE: enviando mensaje por la cola de mensajes\n");
										exit(EXIT_FAILURE);
									}
							    } else {
							    	/* Si no, realiza un movimiento hacia un enemigo */
							    	strcpy(accion.tipo, "ACCION MOVER");

							    	nave_enemiga = nave_rastrear(mapa, nave, i);
							    	accion.equipo = i;
									accion.nave = j;
									accion.oriY = nave->posy;
									accion.oriX = nave->posx;
							    	if(nave_enemiga.equipo != -1) {
										accion.desY = nave->posy + nave_seguirY(nave, nave_enemiga);
										accion.desX = nave->posx + nave_seguirX(nave, nave_enemiga);
									} else {
										accion.desY = nave->posy;
										accion.desX = nave->posx;
									}

									if(mq_send(queue, (char*)&accion, sizeof(accion), 1) == -1) {
										printf("ERROR DE NAVE: enviando mensaje por la cola de mensajes\n");
										exit(EXIT_FAILURE);
									}
							    }
							} 
							/* Realiza un movimiento aleatorio */
							strcpy(accion.tipo, "ACCION MOVER");
							accion.equipo = i;
							accion.nave = j;
							accion.oriY = nave->posy;
							accion.oriX = nave->posx;
							int aleatY = accion_moverAleatorioY(accion.desY);
							int aleatX = accion_moverAleatorioX(accion.desX);
							if(mapa_is_casilla_vacia(mapa, aleatY, aleatX) == true) {
								accion.desY = aleatY;
								accion.desX = aleatX;
							} 
							
							if(mq_send(queue, (char*)&accion, sizeof(accion), 1) == -1) {
								printf("ERROR DE NAVE: enviando mensaje por la cola de mensajes\n");
								exit(EXIT_FAILURE);
							}
						}

						sleep(1);
					}
		        } else {
		        	/* Guarda el pid de la nave recién creada para poder mandar la señal sigterm al finalizar */
		        	pid_naves[j] = PIDnave;
		        }
			}

        	while(1) {

        		if(pipe_read(fd1[i], buffer) < 0) {
				 	printf("ERROR DE JEFE: leyendo del pipe con el simulador.\n");
				 	exit(EXIT_FAILURE);
				}

				if(strcmp(buffer, "TURNO") == 0) {

					for(int numOwnNave = 0; numOwnNave < N_NAVES; numOwnNave++) {	
						bzero(buffer, sizeof(buffer));		
						sprintf(buffer, "ACCION ATAQUE");
						if(pipe_write(fd2[numOwnNave], buffer) < 0) {
							printf("ERROR DE JEFE: escribiendo en la tubería.\n");
							exit(EXIT_FAILURE);
						}
					}
				} else if(strcmp(buffer, "FIN") == 0) {
					/* Manda SIGTERM a todas las naves y espera para finalizar su ejecución */
					for(int k = 0; k < N_NAVES; k++) {
						kill(pid_naves[k], SIGTERM);
					}

					while(wait(NULL) > 0);
					exit(EXIT_SUCCESS);

				} else {

					for(int numOwnNave = 0; numOwnNave < N_NAVES; numOwnNave++) {
						/* Comprueba que proceso a de destruir */
						char buffer_aux[PIPE_MAXSIZE];	
						sprintf(buffer_aux, "DESTRUIR <%d>", numOwnNave);
						if(strcmp(buffer, buffer_aux) == 0) {
							bzero(buffer, sizeof(buffer));		
							sprintf(buffer, "DESTRUIR");
							if(pipe_write(fd2[numOwnNave], buffer) < 0) {
								printf("ERROR DE JEFE: escribiendo en la tubería.\n");
								exit(EXIT_FAILURE);
							}
						}
					}
				}

				sleep(1);
			}
        }
	}
 	
 	fprintf(stdout, "Simulador gestionando senales\n");
	/* Creación del manejador encargado de capturar SIGINT */
	if(manejador_SIGINT_create(act_SIGINT) < 0) {
	    printf("ERROR DE SIMULADOR: creando el manejador_SIGINT.\n");
	    exit(EXIT_FAILURE);
	}

	/* Creación del manejador encargado de capturar SIGALRM */
	if(manejador_SIGALRM_create(act_SIGALRM) < 0) {
	    printf("ERROR DE SIMULADOR: creando el manejador_SIGALRM.\n");
	    exit(EXIT_FAILURE);
	}

	alarm(TURNO_SECS);

	sleep(TURNO_SECS+1);

    while(1) {

    	tipo_accion accion;

    	fprintf(stdout, "Simulador: escuchando cola mensajes\n");

    	/* Recibe el mensaje de la cola de mensajes */
    	mq_receive(queue, (char*)&accion, QUEUE_MAXSIZE, NULL);

    	fprintf(stdout, "Simulador: recibido en cola de mensajes\n");

		simulador_update(accion);

	    usleep(SIM_REFRESH);
    }
}



