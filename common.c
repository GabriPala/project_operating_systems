#include "common.h"

/* Inizializzazione e lettura parametri */
void leggiFile(char *pathname){
	FILE *fs;
	char ch;
	int val = 0;
	int i = 0;

	if(pathname!=NULL){
		/* Apertura file in lettura */
		fs = fopen(pathname, "r");

		if(fs!=NULL){
			ch = fgetc(fs);
			while(i<10){
				if(ch=='#'){		/* I caratteri che seguono il carattere # sono commenti, quindi ignorati */
					while(ch!='\n')
						ch = fgetc(fs);
				}
				else if(ch == '\n')
					ch = fgetc(fs);

				else if(ch>=48 && ch<=57){
					while(ch != '\n'){
						val=(val*10)+(ch-48);
						ch = fgetc(fs);
					}
					switch (++i){
						case 1:
							SO_NUM_G = val;
							break;
						case 2:
							SO_NUM_P = val;
							break;
						case 3:
							SO_MAX_TIME = val;
							break;
						case 4:
							SO_ALTEZZA = val;
							break;
						case 5:
							SO_BASE = val;
							break;
						case 6:
							SO_FLAG_MIN = val;
							break;
						case 7:
							SO_FLAG_MAX = val;
							break;
						case 8:
							SO_ROUND_SCORE = val;
							break;
						case 9:
							SO_N_MOVES = val;
							break;
						case 10:
							SO_MIN_HOLD_NSEC = val;
							break;
					}
					val = 0;
				}
			}
		}
		else{
			printf("Errore nei parametri\n");
			fclose(fs);
			exit(EXIT_FAILURE);
		}

		if(SO_NUM_G<=1 || SO_NUM_P<=0 || SO_ALTEZZA<=0 || SO_BASE<=0
			|| SO_MAX_TIME<=0 || (SO_FLAG_MIN==0 && SO_FLAG_MAX==0)
			|| SO_FLAG_MIN>SO_FLAG_MAX){
			printf("Errore: parametri non validi\n");
			fclose(fs);
			exit(EXIT_FAILURE);
		}
		if(SO_ALTEZZA*SO_BASE<((SO_NUM_G*SO_NUM_P)+SO_FLAG_MAX)){
			printf("Errore: non c'e' sufficiente spazio per pedine e bandierine\n");
			fclose(fs);
			exit(EXIT_FAILURE);
		}
		fclose(fs);
	}
}

int createSharedMem(int key, int size){
  int i, shm_id = shmget(key, size, 0666|IPC_CREAT);
  void *attach;

  if(shm_id == -1){
    printf("Errore: impossibile creare la memoria condivisa\n");
    exit(EXIT_FAILURE);
  }
  return shm_id;
}

void* attachMem(int shm_id){
  void *attach;
  attach = shmat(shm_id, 0, 0);

  if(attach == (void*) -1){
    perror("Errore: impossibile agganciarsi alla memoria condivisa\n");
    exit(EXIT_FAILURE);
  }
	return attach;
}

void createAttachMem(){
	scacchieraID = createSharedMem(ftok("./master", 5), sizeof(struct Cella)*SO_ALTEZZA*SO_BASE);
	scacchiera = attachMem(scacchieraID);
	infoGID = createSharedMem(ftok("./master", 10), sizeof(struct infoGiocatore)*SO_NUM_G);
	infoG = attachMem(infoGID);
}

int valoreAssoluto(int a, int b){
	return ((a-b) > 0) ? (a-b) : -(a-b);
}

/* Conversione int -> char */
void inverti(char* s){
  int i, j;
  char c;

  for(i=0, j=strlen(s)-1; i<j; i++, j--){
      c = s[i];
      s[i] = s[j];
      s[j] = c;
  }
}

void itoa(int n, char* s){
  int i=0;

  do{
      s[i++] = n%10+'0';
  }while((n/=10)>0);

  s[i] = '\0';    /* Indicazione fine stringa */
  inverti(s);
}
