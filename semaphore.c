#include "semaphore.h"
#include "common.h"

int createSEM(key_t key, int size){
  int sem_id = semget(key, size, 0666|IPC_CREAT);

  if(sem_id == -1){
    printf("Errore: impossiblie creare i semafori\n");
    exit(EXIT_FAILURE);
  }
  return sem_id;
}

void getSemID(){
  SEM_ID_CELLE = createSEM(ftok("./semaphore.c",10),SO_ALTEZZA*SO_BASE);
  SEM_ID_G = createSEM(ftok("./semaphore.c",20), SO_NUM_G);
  SEM_ID_P = createSEM(ftok("./semaphore.c",30), SO_NUM_P);
  SEM_ID_GAME = createSEM(ftok("./semaphore.c", 40), 3);
  SEM_ID_GENERIC = createSEM(ftok("./semaphore.c", 50), 2);
}

int inizializza_Sem(int sem_id, int sem_num, int val){
  union semun sem_val;
  sem_val.val = val;
  return semctl(sem_id, sem_num, SETVAL, sem_val);
}

/* Uso della flag per non attendere, eventualmente, i processi */
int reserve_Sem(int sem_id, int sem_num, int flag){
  struct sembuf sops;
  sops.sem_num = sem_num;
  sops.sem_op = -1;
  sops.sem_flg = flag;
  return semop(sem_id, &sops, 1);
}

int release_Sem(int sem_id, int sem_num){
  struct sembuf sops;
  sops.sem_num = sem_num;
  sops.sem_op = 1;
  sops.sem_flg = 0;
  return semop(sem_id, &sops, 1);
}

int wait_Sem(int sem_id, int sem_num){
  struct sembuf sops;
  sops.sem_num = sem_num;
  sops.sem_op = 0;
  sops.sem_flg = 0;
  return semop(sem_id, &sops, 1);
}

/* Pulizia memoria per i semafori */
void deleteSEM(int sem_id){

	if(semctl(sem_id, 0, IPC_RMID) == -1)	/* Deallocazione dei set di semafori */
	{
		fprintf(stderr, "%s: %d. Errore nel deallocare il set di semafori (semctl #%03d: %s)\n",
				__FILE__, __LINE__, errno, strerror(errno));
		printf("Impossibile deallocare %d set di semafori\n", sem_id);
		exit(EXIT_FAILURE);
	}
}
