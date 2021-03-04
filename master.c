/* Segnali gestiti:
SIGINT = segnale per interrompere i processi (giocatore)
SIGCHLD = segnale per acquisire lo stato modificato dei processi figli
SIGTERM = segnale per terminare un processo (in caso di errore)
SIGILL = segnale per istruzione illegale
SIGKILL = segnale per uccidere i processi
SIGALRM = segnale per acquisire lo scadere di un timer
SIGUSR1 = segnale per attivare i giocatori */

#include "common.h"
#include "semaphore.h"

/* Prototipi per la gestione delle bandierine */
int generaBandierine();
int contaBandierine();
void posizionaBandierine();
/* Prototipo per la gestione dei giocatori */
void creaGiocatori(char*, int );
/* Prototipo per la gestione della chiusura del gioco */
void chiusuraGioco();
/* Prototipi per le stampe a video */
void stampaScacchiera();
void stampaClassifica();
void stampaEColora(char );
void stampaTempo(int );
/* Prototipo per la gestione dei segnali */
void handleSignal(int );

int numBandierine;
int *PIDGiocatori;
time_t start;

int main(int argc, char *argv[]){
	int i, j;
	struct timespec toWait;
	NUM_ROUND = 1;

  leggiFile(argv[1]);

	toWait.tv_sec = 0;
  toWait.tv_nsec = SO_MIN_HOLD_NSEC;

	/* Set della maschera vuota */
	sigemptyset(&my_mask);
  bzero(&sa, sizeof(sa));
	sa.sa_mask = my_mask;
	sa.sa_handler = handleSignal;
	sa.sa_flags = SA_NODEFER;

	/* Gestione del segnale SIGINT */
	bzero(&saINT, sizeof(saINT));
	saINT.sa_mask = my_mask;
	saINT.sa_handler = chiusuraGioco;		/* Se viene premuto Ctrl-C il gioco termina e viene stampata la classifica */
	saINT.sa_flags = SA_NODEFER;

	/* Settaggio segnali */
  sigaction(SIGINT, &saINT, NULL);
	sigaction(SIGTERM, &sa, NULL);
	sigaction(SIGILL, &sa, NULL);
	sigaction(SIGKILL, &sa, NULL);
	sigaction(SIGCHLD, &sa, NULL);
	sigaction(SIGUSR1, &sa, NULL);
  sigaction(SIGALRM, &sa, NULL);

	/* Creazione memorie condivise (scacchiera, infoG) */
	createAttachMem();
	/* Creazione set di semafori */
	getSemID();

  /* Inizializzazione scacchiera e semafori delle celle */
  for(i=0; i<SO_ALTEZZA; i++){
		for(j=0; j<SO_BASE; j++){
			scacchiera[i*SO_BASE+j].simbolo=' ';
			scacchiera[i*SO_BASE+j].attr.punti=0;
			inizializza_Sem(SEM_ID_CELLE,i*SO_BASE+j, 1);
		}
	}

	/* Inizializzazione di tutti i semafori utilizzati (eccetto scacchiera) */
	for(i=0; i<SO_NUM_G; i++)			/* Semafori giocatori (tutti i giocatori sono bloccati) */
		inizializza_Sem(SEM_ID_G, i, 1);
	for(i=0; i<SO_NUM_P; i++)
		inizializza_Sem(SEM_ID_P, i, 1);		/* Semafori pedine (tutte le pedine sono bloccate) */
	inizializza_Sem(SEM_ID_GAME, 0, 1);		/* Blocco delle pedine ad ogni NUM_ROUND */
	inizializza_Sem(SEM_ID_GAME, 1, SO_NUM_G);	/* Controllo che i giocatori siano pronti */
	inizializza_Sem(SEM_ID_GAME, 2, 1); 	/* Inizio gioco */
	inizializza_Sem(SEM_ID_GENERIC, 0, 1);	/* Calcolo prima destinazione di ogni giocatore */
	/* inizializza_Sem(SEM_ID_GENERIC, 1, 1); 	  Assegnamento punteggio in modalita' mutua esclusione */

  /* Creazione di tutti i giocatori e tabella dei loro PID */
  PIDGiocatori = (int*)malloc(sizeof(int)*SO_NUM_G);
	for(i=0; i<SO_NUM_G; i++){
		creaGiocatori(argv[1], i); /* Creazione dei giocatori */
		/* Creazione tabella informazioni giocatori */
		infoG[i].numeroID = i;		/* Numero identificativo dei giocatori usato per la classifica */
		infoG[i].punti = 0;
		infoG[i].mosseUsate = 0;
	}

	/* Gestione della creazione delle pedine in modo ordinato */
	reserve_Sem(SEM_ID_G, 0, 0);	/* Viene riservato il semaforo per il primo giocatore (puo' iniziare a piazzare le pedine) */
	for(i=0; i<SO_NUM_P; i++){
		reserve_Sem(SEM_ID_P, i, 0);
		for(j=0; j<SO_NUM_G; j++)
			wait_Sem(SEM_ID_G, (j+1)%SO_NUM_G);
		release_Sem(SEM_ID_P, i);
	}

	/* Il master puo' piazzare le bandierine */
	posizionaBandierine();
	reserve_Sem(SEM_ID_GENERIC, 0, 0);
	wait_Sem(SEM_ID_GAME, 1);
	stampaScacchiera();
	/* Inizio gioco */
	reserve_Sem(SEM_ID_GAME, 2, 0);

	/* Inizio calcolo tempo */
	start = time(NULL);
	alarm(SO_MAX_TIME);

	/* Il master imposta e conta i round effettuati, attende la termimazione del tempo */
  while(1){
		inizializza_Sem(SEM_ID_GAME, 0, 1);
		numBandierine = contaBandierine();

		if(numBandierine==0){
			alarm(0);
			inizializza_Sem(SEM_ID_G, 0, 1);

			posizionaBandierine();
			numBandierine = contaBandierine();
			NUM_ROUND++;
			for(i=0; i<SO_NUM_G; i++)
				kill(PIDGiocatori[i], SIGUSR1); /* Attiva i giocatori */
			inizializza_Sem(SEM_ID_G, 0, 0);
			/*wait_Sem(SEM_ID_GAME, 3);*/
			alarm(SO_MAX_TIME);
		}
		stampaScacchiera();
		inizializza_Sem(SEM_ID_GAME, 0, 0);
		nanosleep(&toWait, NULL);	/* Rallentamento stampe a video per una migliore comprensione */
	}
	/* Chiusura gioco in modo corretto, detachment e deallocazione di tutte le risorse IPC */
  chiusuraGioco();
}

/* ---- INIZIO METODI ---- */

/* Generazione numero di bandierine pseudo-randomicamente */
int generaBandierine(){
	struct timespec for_flag;
	for_flag.tv_sec = 0;
	for_flag.tv_nsec = SO_MIN_HOLD_NSEC;
	clock_gettime(CLOCK_REALTIME, &for_flag);
	/* Viene generato pseudo-randomicamente un numero tra SO_FLAG_MAX e SO_FLAG_MIN */
	return (for_flag.tv_nsec % (SO_FLAG_MAX-SO_FLAG_MIN+1)) + SO_FLAG_MIN;
}

/* Conta numero bandierine presenti sulla scacchiera */
int contaBandierine(){
	int i, j, cont = 0;

	for(i=0; i<SO_BASE; i++){
		for(j=0; j<SO_ALTEZZA; j++){
			if(scacchiera[j*SO_BASE+i].simbolo == 'B')
				cont++;
		}
	}
	return cont;
}

/* Assegnamento punteggio e posizione delle bandierine */
void posizionaBandierine(){
	int i, j, riga, colonna, *punti;
	time_t t;
	srand((unsigned) time(&t));

	numBandierine = generaBandierine();
	punti = (int*)malloc(sizeof(int)*numBandierine);

	/* Inizio assegnamento punteggi */
	for(i=0; i<numBandierine; i++)
		punti[i] = 0;

	for(i=0; i<SO_ROUND_SCORE/2; i++)		/* Assegna in modo uniforme la prima meta' */
		punti[i%numBandierine]++;
	for(i=0; i<SO_ROUND_SCORE/2; i++)			/* Assegna in modo pseudo-randomico la seconda parte */
		punti[rand()%numBandierine]++;

	if(SO_ROUND_SCORE%2 == 1)					/* Caso SO_ROUND_SCORE dispari */
		punti[0]++;
	/* Fine assegnamento punteggi */

	/* Calcolo posizione della bandierina */
	for(i=0; i<numBandierine; i++){
		do{
			riga = (rand() % SO_ALTEZZA);
			colonna = (rand() % SO_BASE);
		}while(scacchiera[riga*SO_BASE+colonna].simbolo!=' ');

		/* Assegnamento simbolo 'B' riferito alla bandierina nella memoria condivisa */
		scacchiera[riga*SO_BASE+colonna].simbolo = 'B';
		scacchiera[riga*SO_BASE+colonna].attr.punti = punti[i];
	}
	free(punti);
}

/* Creazione processo giocatore, passaggio informazioni, salvataggio del suo PID */
void creaGiocatori(char argv[], int i){
	char *(args)[4];
	char numGiocatore[10];
	itoa(i, numGiocatore);
	args[0] = "./player";
	args[1] = argv;		/* Pathname del file con valori in input */
	args[2] = numGiocatore;
	args[3] = NULL;

	switch(PIDGiocatori[i] = fork()){
		case -1:
			printf("Errore: impossibile creare il giocatore\n");
			exit(EXIT_FAILURE);
			break;
		case 0:
			execve("./player", args, NULL);
			exit(EXIT_FAILURE);
			break;
		default:
			break;
	}
}

/* Stampa informazioni conclusive del gioco e deallocazione risorse IPC */
void chiusuraGioco(){
	int i, tempoGioco, status;
	time_t end;

	end = time(NULL);
	tempoGioco = end-start;

	stampaScacchiera();
	printf("\n\nSONO STATI GIOCATI: %d ROUND\n", NUM_ROUND);
	printf("Numero di bandierine NON conquistate: %d\n", numBandierine);
	printf("Tempo impiegato: ");
	stampaTempo(tempoGioco);
	stampaClassifica();
	system("echo \" \a\"");		/* Segnale acustico per indicare terminazione esecuzione */

	for(i=0; i<SO_NUM_G; i++)
    kill(PIDGiocatori[i], SIGINT);	/* Uccisione di tutti i processi giocatore */

	/* Attesa della terminazione dei processi */
	while(wait(&status) != -1);

	free(PIDGiocatori);
	/* Detachment e deallocazione delle memorie condivise */
	shmdt(scacchiera);
	shmctl(scacchieraID, IPC_RMID, NULL);
	shmdt(infoG);
	shmctl(infoGID, IPC_RMID, NULL);
	/* Deallocazione dei semafori */
	deleteSEM(SEM_ID_G);
	deleteSEM(SEM_ID_P);
	deleteSEM(SEM_ID_GAME);
	deleteSEM(SEM_ID_GENERIC);
	deleteSEM(SEM_ID_CELLE);
	exit(EXIT_SUCCESS);
}

/* Stampa della scacchiera e informazioni di ogni giocatore durante il round */
void stampaScacchiera(){
	int i, j;

	printf("\n\n*************ROUND %d*************\n\n", NUM_ROUND);
	for(i=0;i<SO_NUM_G;i++){
		printf("Giocatore %d ", infoG[i].numeroID+1);
		printf("\n Punti: %d \n Mosse usate: %d \n Mosse rimanenti: %d\n",
			infoG[i].punti,infoG[i].mosseUsate,(SO_N_MOVES*SO_NUM_P)-infoG[i].mosseUsate);
	}
	printf("Ancora da conquistare: ");
	stampaEColora('F');
	printf("%d bandierine\033[0m\n\n", numBandierine);

	for(i=0; i<=SO_BASE+1; i++)
		printf("#");
	printf("\n");

	for(i=0;i<SO_ALTEZZA; i++){
		printf("|");
		for(j=0;j<SO_BASE; j++)
			stampaEColora(scacchiera[i*SO_BASE+j].simbolo);
		printf("|\n");
	}
	for(i=0; i<=SO_BASE+1; i++)
		printf("#");
	printf("\n");
}

/* Creazione e stampa della classifica dei giocatori in base al punteggio ottenuto */
void stampaClassifica(){
	int i, j, min;
	struct infoGiocatore temp;

	/* Ordinamento tramite Selection sort */
	for(i=0; i<SO_NUM_G-1; i++){
		min=i;
		for(j=i+1; j<SO_NUM_G; j++){
			if(infoG[j].punti>infoG[min].punti)
				min=j;
		}
		temp = infoG[min];
		infoG[min] = infoG[i];
		infoG[i] = temp;
	}

	printf("\n\n\t\t         CLASSIFICA      \n");
	printf(" Giocatore  |      Punti      |   Mosse usate   |  Mosse rimanenti\n");
	for(i=0; i<SO_NUM_G; i++){
		/* Gestione stampa del vincitore */
		if(i==0){
			stampaEColora('G');
			printf("   %3d      \033[0m|", infoG[i].numeroID+1);
			stampaEColora('G');
			printf("    %6d       \033[0m|", infoG[i].punti);
			stampaEColora('G');
			printf("    %6d       \033[0m|", infoG[i].mosseUsate);
			stampaEColora('G');
			printf("    %6d       	\033[0m\n", (SO_N_MOVES*SO_NUM_P)-infoG[i].mosseUsate);
		}
		else
			printf("   %3d      |    %6d       |    %6d       |    %6d\n",
				infoG[i].numeroID+1, infoG[i].punti, infoG[i].mosseUsate,
				(SO_N_MOVES*SO_NUM_P)-infoG[i].mosseUsate);
	}
}

/* Stampa e colora simboli mostrati a video */
void stampaEColora(char simbolo){
	if(simbolo=='G')	/* Vincitore color oro su sfondo viola */
		printf("\033[33;45;5m\033[1m");
	else if(simbolo=='B')	/* Simbolo B delle bandierine di colore verde su scacchiera */
		printf("\033[1;32mB\033[0m");
	else if(simbolo=='F')	/* Testo riferito al # di bandierine da conquistare di colore verde */
		printf("\033[1;32m");
	else if(simbolo=='P'){ /* Segna posizione pedina su scacchiera */
		printf("O");
	}else		/* Segna celle vuote nella stampa della scacchiera */
		printf(".");
}

/* Stampa tempo (sec e min) del tempo impiegato dal gioco (intero) */
void stampaTempo(int secondi){
	int s, m;
	s = secondi % 60;
	m = (secondi-s)/60;

	printf("%02d:%02d\n", m, s);
}

/* Gestione segnali */
void handleSignal(int signal){
	int i, status;

	if(signal==SIGTERM || signal==SIGILL || signal==SIGCHLD || signal==SIGKILL){
		for(i=0; i<SO_NUM_G; i++)
      kill(PIDGiocatori[i], SIGINT);		/* Uccisione di tutti i processi giocatore */

		/* Attesa della terminazione dei processi */
		while(wait(&status) != -1);

		free(PIDGiocatori);
		/* Detachment e deallocazione delle memorie condivise */
		shmdt(scacchiera);
		shmctl(scacchieraID, IPC_RMID, NULL);
		shmdt(infoG);
		shmctl(infoGID, IPC_RMID, NULL);
		/* Deallocazione dei semafori */
		deleteSEM(SEM_ID_G);
		deleteSEM(SEM_ID_P);
		deleteSEM(SEM_ID_GAME);
		deleteSEM(SEM_ID_GENERIC);
		deleteSEM(SEM_ID_CELLE);
		exit(EXIT_FAILURE);
	}

	/* Gestione chiusura forzona, sono strascorsi SO_MAX_TIME secondi
		 senza che il round sia terminato */
	if(signal==SIGALRM){
		if(numBandierine>0)	/* Gestione caso: tutte le bandierine sono state catturate,
													 ma e' finito il tempo */
			chiusuraGioco();
	}
}
