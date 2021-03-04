/* Segnali gestiti:
SIGINT = segnale per interrompere i processi (pedina)
SIGTERM = segnale per terminare un processo (in caso di errore)
SIGILL = segnale per istruzione illegale
SIGKILL = segnale per uccidere i processi
SIGCHLD = segnale per acquisire lo stato modificato dei processi figli
SIGUSR1 = segnale per attivare le pedine */

#include "common.h"
#include "semaphore.h"

/* Prototipo per la gestione delle pedine */
void creaPedine(char*, int, int );
void posizionaPedine(char*, int );
void azzeraDestinazione();
int trovaBandierine();
void inizializzaDestinazionePedina();
/* Prototipo per la gestione dei segnali */
void handleSignal(int );

struct infoBandierina *infoB;
boolean nuovoRound = FALSE;
int *PIDPedine;

int main(int argc, char *argv[]){
  int i,j;

  leggiFile(argv[1]);

  /* Set della maschera vuota */
  sigemptyset(&my_mask);
  bzero(&sa, sizeof(sa));
  sa.sa_mask = my_mask;
	sa.sa_handler = handleSignal;
  sa.sa_flags = SA_NODEFER;

  /* Settaggio segnali */
  sigaction(SIGINT, &sa, NULL);
  sigaction(SIGTERM, &sa, NULL);
  sigaction(SIGILL, &sa, NULL);
  sigaction(SIGKILL, &sa, NULL);
  sigaction(SIGUSR1, &sa, NULL);
  sigaction(SIGCHLD, &sa, NULL);

  /* Identificatore del giocatore (riferimento a 'numGiocatore' in master) */
  turno = atoi(argv[2]);
  /* Creazione memoria condivisa per le informazioni delle pedine del giocatore stesso */
  infoPID = createSharedMem(ftok("./pawn.c", turno), sizeof(struct Destinazione)*SO_NUM_P);
  infoPedine = attachMem(infoPID);
  /* Attachment memorie condivise (scacchiera, infoG) */
  createAttachMem();
  /* Attachment set di semafori */
  getSemID();

  /* Inizializzazione per tutte le pedine al numero massimo di mosse disponibili */
  for(i=0; i<SO_NUM_P; i++)
    infoPedine[i].mosseDisponibili = SO_N_MOVES;

  /* Creazione (ordinata) delle pedine del giocatore stesso e tabella dei loro PID */
  PIDPedine = (int*)malloc(sizeof(int)*SO_NUM_P);
  for(i=0; i<SO_NUM_P; i++){
    wait_Sem(SEM_ID_P, i);
    wait_Sem(SEM_ID_G, turno);
    release_Sem(SEM_ID_G, turno);
    /* Genera posizione e crea processo */
    posizionaPedine(argv[1], i);
  }

  /* Attesa che tutte le pedine (di tutti i giocatori) siano pronte */
  wait_Sem(SEM_ID_GENERIC, 0);
  /* Azzera e indica la prima destinazione alle pedine */
  azzeraDestinazione();
  inizializzaDestinazionePedina();
  /* Le pedine di ogni giocatore attendono che tutti i giocatori
     abbiano dato alle proprie pedine le indicazioni
     sui primi spostamenti da fare */
  reserve_Sem(SEM_ID_GAME, 1, 0);
  /* Attesa dell'inizio del gioco da parte del master per poter svegliare le pedine */
  wait_Sem(SEM_ID_GAME, 2);

  /* Segnale per attivare le pedine in attesa di potersi muovere */
  for(i=0; i<SO_NUM_P; i++)
    kill(PIDPedine[i], SIGUSR1);

  while(1){
    /* Reset destinazione pedine (all'inizio del nuovo round) */
    if(nuovoRound){
      azzeraDestinazione();
      inizializzaDestinazionePedina();
      for(i=0; i<SO_NUM_P; i++)
        kill(PIDPedine[i], SIGUSR1);
      nuovoRound = FALSE;
    }
    /* Attesa della terminazione del round o del gioco */
    pause();
  }
}

/* ---- INIZIO METODI ---- */

/* Creazione processo pedina, passaggio informazioni, salvataggio del suo PID */
void creaPedine(char argv[], int turno, int i){
  char *(args)[5];
  char numPedina[10];
  char numGiocatore[10];
  itoa(i, numPedina);
  itoa(turno, numGiocatore);

  args[0] = "./pawn";
  args[1] = argv;   /* Pathname del file con valori in input */
  args[2] = numPedina;
  args[3] = numGiocatore;
  args[4] = NULL;

	switch(PIDPedine[i] = fork()){
	case -1:
    printf("Errore: impossibile creare la pedina\n");
    exit(EXIT_FAILURE);
    break;
	case 0:
    execve("./pawn", args, NULL);
    exit(EXIT_FAILURE);
    break;
	default:
    break;
	}
}

/* Genera posizione pedina e la crea */
void posizionaPedine(char argv[], int i){
  int riga, colonna;
  time_t t;
  srand((unsigned) time(&t));

  /* Generazione posizione in modalita' pseudo-randomica
     di una pedina (se ritorna -1 la cella e' occupata) */
  do{
    riga = (rand() % SO_ALTEZZA);
    colonna = (rand() % SO_BASE);
  }while(reserve_Sem(SEM_ID_CELLE, riga*SO_BASE+colonna, IPC_NOWAIT)==-1);

  scacchiera[riga*SO_BASE+colonna].simbolo = 'P';
  infoPedine[i].precRiga = riga;
  infoPedine[i].precColonna = colonna;
  /* Creazione effettiva del processo pedina */
  creaPedine(argv, turno, i);
}

/* Inizializzazione informazioni per la pedina */
void azzeraDestinazione(){
  int i;

  for(i=0; i<SO_NUM_P; i++){
    /* Distanza inizializzata a SO_BASE+SO_ALTEZZA per indicare
       alla pedina di non spostarsi (per non sprecare mosse) */
    infoPedine[i].distanza = SO_BASE+SO_ALTEZZA;
    infoPedine[i].destinazioneRiga = SO_BASE+1;
    infoPedine[i].destinazioneColonna = SO_ALTEZZA+1;
  }
}

/* Conta numero bandierine presenti sulla scacchiera e ne salva la posizione */
int trovaBandierine(){
  int i, j, cont = 0;
  infoB = (struct infoBandierina*)malloc(sizeof(struct infoBandierina)*SO_FLAG_MAX);

  for(i=0; i<SO_BASE; i++){
    for(j=0; j<SO_ALTEZZA; j++){
      if(scacchiera[j*SO_BASE+i].simbolo == 'B'){
        infoB[cont].posizioneRiga = j;
        infoB[cont].posizioneColonna = i;
        cont++;
      }
    }
  }
  return cont;
}

/* Inizializzazione assegnamento destinazione alle pedina */
void inizializzaDestinazionePedina(){
  int i, j, k, minimo, minPrec, numBandierine;
  int riga, colonna, rigaBandierina, colonnaBandierina;
  int *array, **matrice;

  /* Calcolo di quante bandierine sono presenti sulla scacchiera */
  numBandierine = trovaBandierine();
  /* Array per memorizzare le posizioni (colonne) delle pedine
     con distanza minima rispetto a una bandierina;
     la corrispondenza nell'array stesso della
     bandierina la si ha con l'indice */
  array = (int*)malloc(sizeof(int)*numBandierine);
  /* Matrice bandierineXpedine (del singolo giocatore)
     per memorizzare le distanze e determinarne la minima */
  matrice = (int**)malloc(sizeof(int*)*numBandierine);

  /* Per ogni bandierina si calcolano tutte le distanze con tutte le pedine
     e si tiene traccia della piu' vicina */
  for(i=0; i<numBandierine; i++){
    matrice[i] = (int*)malloc(sizeof(int)*SO_NUM_P);
    /* Inizializzazione variabili d'appoggio */
    minimo = SO_BASE+SO_ALTEZZA;
    for(j=0; j<SO_NUM_P; j++){
      riga = infoPedine[j].precRiga;
      colonna = infoPedine[j].precColonna;
      rigaBandierina = infoB[i].posizioneRiga;
      colonnaBandierina = infoB[i].posizioneColonna;

      /* Calcolo della distanza tra pedina e bandierina */
      matrice[i][j] = valoreAssoluto(riga, rigaBandierina)
                      +valoreAssoluto(colonna, colonnaBandierina);

      /* Se la distanza nella matrice e' la minore viene salvata la sua
         posizione nell'array per essere poi confrontata con le rimanenti */
      if(matrice[i][j]<minimo){
        minimo = matrice[i][j];
        array[i] = j;
      }
    }
  }

  /* Inizio assegnamento delle bandierine alle pedine */
  for(i=0; i<numBandierine; i++){
    if(array[i]!=-1){   /* -1 indica che la bandierina e' stata assegnata a una pedina */
      minPrec = i; /* Memorizzazione indice della bandierina per cui
                      la distanza della pedina e' minima */
      minimo = matrice[i][array[i]];
      /* Controllo se la pedina con distanza minima e' piu'
         vicina a un'altra bandierina e contemporaneamente
         questa distanza e' a sua volta minimale tra le
         altre pedine */
      for(j=i+1; j<numBandierine; j++){
        if(array[j]!=-1 && array[j]==array[i] && matrice[j][array[i]]<minimo){
          minimo = matrice[j][array[i]];
          matrice[minPrec][array[i]] = SO_BASE+SO_ALTEZZA;
          array[minPrec] = -2;    /* -2 indica che va ricalcolata la
                                     distanza minima per una determinata bandierina */
          minPrec = j;
        }
      }
      /* Assegnamento alla pedina la destinazione */
      infoPedine[array[minPrec]].distanza = matrice[minPrec][array[i]];
      infoPedine[array[minPrec]].destinazioneRiga = infoB[minPrec].posizioneRiga;
      infoPedine[array[minPrec]].destinazioneColonna = infoB[minPrec].posizioneColonna;
      array[minPrec] = -1;  /* -1 indica che la bandierina e' stata assegnata a una pedina */

      /* Ricalcola la distanza minina per tutte le bandinerine che non
         hanno una pedina con distanza minima assegnata */
      for(j=0; j<numBandierine; j++){
        if(array[j]==-2){
          minimo = matrice[j][0];
          array[j] = 0;
          for(k=1; k<SO_NUM_P; k++){
            if(matrice[j][k]<minimo){
              minimo = matrice[j][k];
              array[j] = k;
            }
          }
        }
      } /* Chiusura ciclo per il ricalcolo della distanza minima*/
    }   /* Chiusura condizione per l'assegnamento delle bandierine alle pedine */
  } /* Fine assegnamento delle bandierine alle pedine */

  /* Deallocazione memorie */
  free(array);
  for(i=0; i<numBandierine; i++)
    free(matrice[i]);
  free(matrice);
}

/* Gestione segnali */
void handleSignal(int signal){
  int i, status;

  if(signal==SIGTERM || signal==SIGILL || signal==SIGKILL){
    for(i=0; i<SO_NUM_P; i++)
      kill(PIDPedine[i], SIGINT); /* Uccisione di tutti i processi pedina */

    /* Attesa della terminazione dei processi */
    while(wait(&status) != -1);
    free(infoB);
    free(PIDPedine);
    /* Detachment e deallocazione delle memorie condivise */
    shmdt(infoPedine);
    shmctl(infoPID, IPC_RMID, NULL);
    shmdt(scacchiera);
    exit(EXIT_FAILURE);
  }

  /* Segnale per la terminazione con la stampa della classifica (nel master) */
  if(signal==SIGINT){
    for(i=0; i<SO_NUM_P; i++)
      kill(PIDPedine[i], SIGINT); /* Uccisione di tutti i processi pedina */

    /* Attesa della terminazione dei processi */
    while(wait(&status) != -1);
    free(infoB);
    free(PIDPedine);
    /* Detachment e deallocazione delle memorie condivise */
    shmdt(infoPedine);
    shmctl(infoPID, IPC_RMID, NULL);
    shmdt(scacchiera);
    exit(EXIT_SUCCESS);
  }

  /* Segnale lanciato dal master per avvisare
     i giocatori dell'inizio di un nuovo round */
  if(signal==SIGUSR1){
    nuovoRound = TRUE;
  }
}
