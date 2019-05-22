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

#include <simulador.h>
#include <gamescreen.h>
#include <mapa.h>

#define SEM_CTRL "/sem_ctrl"

/* Variables globales */
tipo_mapa *mapa;
int fd_shm;
sem_t *sem_ctrl = NULL;

/* manejador: rutina de tratamiento de la señal SIGINT. */
void manejador_SIGINT(int sig) {
	screen_end();
	munmap(mapa, sizeof(*mapa));
	exit(EXIT_SUCCESS);
}

void mapa_print(tipo_mapa *mapa)
{
	int i,j,k,m;
	char winner;
	char msg_winner[100], msg[32];

	/* Muestra las casillas del mapa */
	for(j=0;j<MAPA_MAXY;j++) {
		for(i=0,k=0;i<MAPA_MAXX;i++, k++) {
			tipo_casilla cas=mapa_get_casilla(mapa,j, i);
			//printf("%c",cas.simbolo);
			screen_addch(j, i+k, cas.simbolo);
			screen_addch(j, i+k+1, ' ');
		}
		//printf("\n");
	}

	/* Muestra la vida de las naves */
	for(j=0,m=0;j<N_EQUIPOS;j++,m++) {
		for(i=0,k=0;i<N_NAVES;i++) {
			tipo_nave nave=mapa_get_nave(mapa,j, i);
			sprintf(msg, "%c%d life: %d ", nave.equipo+65, nave.numNave, nave.vida);
			for(int l = 0; l < strlen(msg); l++) {
				screen_addch(j+m, MAPA_MAXX*2 + 2 + k + l, msg[l]);
			}
			k+=strlen(msg);
		}
		//printf("\n");
	}

	winner = mapa_get_ganador(mapa);

	/* Imprime un mensaje con el equipo ganador */
	if(winner == 'A' || winner == 'B' || winner == 'C' || winner == 'D') {
		sprintf(msg_winner, "%c WINS!", winner);
		for(int i = 0; i < strlen(msg_winner); i++) {
			screen_addch(j+m, MAPA_MAXX*2 + 2 + i, msg_winner[i]);
		}
	} 

	screen_refresh();
}


int main() {
	int sval;

	if ((sem_ctrl = sem_open(SEM_CTRL, O_CREAT, S_IRUSR | S_IWUSR, 0)) == SEM_FAILED) {
        printf ("ERROR DE SIMULADOR: creando el semaforo de cola de mensajes.\n");
		exit(EXIT_FAILURE);
    }

    sem_getvalue(sem_ctrl, &sval);
    if(sval == 0)
        printf("Esperando a ejecutar el proceso simulador...\n");

    sem_wait(sem_ctrl);

	/* Creación del manejador encargado de capturar SIGINT */
	struct sigaction act_SIGINT;
    sigemptyset(&(act_SIGINT.sa_mask));
    act_SIGINT.sa_flags = 0;
    act_SIGINT.sa_handler = manejador_SIGINT;
    if(sigaction(SIGUSR2, &act_SIGINT, NULL) < 0) {
        printf("ERROR DE MONITOR: creando el manejador_SIGINT.\n");
        exit(EXIT_FAILURE);
    }

	/* Apertura de la memoria compartida para el mapa */
	fd_shm = shm_open(SHM_MAP_NAME, O_RDONLY, 0); 
	if(fd_shm == -1) {
		printf("ERROR DE MONITOR: abriendo el segmento de memoria compartida.\n"); 
		exit(EXIT_FAILURE);
	}
	mapa = (tipo_mapa *)mmap(NULL, sizeof(*mapa), PROT_READ, MAP_SHARED, fd_shm, 0);
	if(mapa == MAP_FAILED) {
		printf("ERROR DE MONITOR: mapeando el segmento de memoria compartida.\n");
		exit(EXIT_FAILURE);
	}

	screen_init();

	while(1){
		sigset_t set, oset;

        /* Máscara que bloqueará las señales */
        sigemptyset(&set);
        sigaddset(&set, SIGINT);
        if (sigprocmask(SIG_BLOCK, &set, &oset) < 0) {
            perror("sigprocmask");
            exit(EXIT_FAILURE);
        }

        mapa_print(mapa);
            
        if (sigprocmask(SIG_UNBLOCK, &set, &oset) < 0) {
            perror("sigprocmask");
            exit(EXIT_FAILURE);
        }

        usleep(SCREEN_REFRESH);
	}
}
