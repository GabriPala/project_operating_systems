#define _GNU_SOURCE
#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<string.h>
#include<time.h>
#include<sys/signal.h>
#include<sys/wait.h>
#include<sys/ipc.h>
#include<sys/shm.h>
#include<sys/sem.h>
#include<sys/types.h>
#include<errno.h>

typedef enum {FALSE, TRUE} boolean;
/* typedef enum {P, B} type;			 P = pedina  B = bandierina */

int SO_NUM_G;
int SO_NUM_P;
int SO_ROUND_SCORE;
int SO_BASE;
int SO_ALTEZZA;
int SO_FLAG_MIN;
int SO_FLAG_MAX;
int SO_MIN_HOLD_NSEC;
int SO_MAX_TIME;
int SO_N_MOVES;
int NUM_ROUND;

/* Dichiarazione di tutte le strutture utilizzate */
struct Pedina{
	unsigned int PIDGiocatore;
	unsigned int PIDPedina;
};

union Attributi{
	unsigned int punti;
	struct Pedina ped;
};

struct Cella{
  char simbolo;
	union Attributi attr;
};

struct Destinazione{
	unsigned int precRiga;  /* Contiene la riga della pedina (posizione) */
	unsigned int precColonna; /* Contiene la colonna della pedina (posizione) */
	unsigned int distanza;
	unsigned int destinazioneColonna;
	unsigned int destinazioneRiga;
	unsigned int mosseDisponibili;
};

struct infoGiocatore{
	unsigned int numeroID;	/* Contiene il numero identificatore del giocatore
													in base all'ordine con cui viene creato
													il processo, non e' il PID, permette di realizzare la
													classifica finale */
	unsigned int mosseUsate;
	unsigned int punti;
};

struct infoBandierina{
  int posizioneRiga;
  int posizioneColonna;
};

struct Cella *scacchiera;
struct infoGiocatore *infoG;
struct Destinazione *infoPedine;

/* Strutture per catturare i segnali */
struct sigaction sa;
struct sigaction saINT;
sigset_t my_mask;

/* Chiavi delle memorie condivise */
int scacchieraID;
int infoGID;
int infoPID;
/* Identificatore di giocatori o di pedine */
int turno;

void leggiFile(char* );
int createSharedMem(int, int );
void* attachMem(int );
void createAttachMem();
int valoreAssoluto(int, int );
void itoa(int, char* );
