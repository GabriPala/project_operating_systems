#define _SEMAPHORE_H_
#define _GNU_SOURCE
#include<stdio.h>
#include<stdlib.h>
#include<sys/sem.h>
#include<sys/types.h>
#include<sys/ipc.h>

/* Chiavi dei set di semafori */
int SEM_ID_CELLE;
int SEM_ID_G;
int SEM_ID_P;
int SEM_ID_GAME;
int SEM_ID_GENERIC;

/* union usata solo per l'inizializzazione dei semafori */
union semun{
  int val;
  struct semid_ds* buf;
  unsigned short* array;
};

int createSEM(key_t, int );
void getSemID();
int inizializza_Sem(int, int, int );
int reserve_Sem(int, int, int );
int release_Sem(int, int );
int wait_Sem(int, int );
void deleteSEM(int );
