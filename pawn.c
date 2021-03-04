/* Segnali gestiti:
SIGINT = segnale per interrompere i processi (pedina)
SIGUSR1 = segnale per attivare le pedine */

#include "common.h"
#include "semaphore.h"

typedef enum {SOPRA, SOTTO, DESTRA, SINISTRA} move;

/* Prototipi per la gestione degli spostamenti delle pedine */
boolean controllaDestinazione();
boolean confrontaDestinazione();
void scegliDirezioneESposta();
void sposta(move );
/* Prototipo per la gestione dei segnali */
void handleSignal(int );

int posRiga;
int posColonna;
int turnoGiocatore;

int main(int argc, char* argv[]){

  leggiFile(argv[1]);

  /* Set della maschera vuota */
  sigemptyset(&my_mask);
  bzero(&sa, sizeof(sa));
  sa.sa_mask = my_mask;
  sa.sa_handler = handleSignal;

  /* Settaggio segnali */
  sigaction(SIGINT, &sa, NULL);
  sigaction(SIGUSR1, &sa, NULL);

  /* Identificatore della pedina (riferimento a 'numPedina' in player) */
  turno = atoi(argv[2]);
  /* Identificatore del giocatore (riferimento a 'numGiocatore' in player) */
  turnoGiocatore = atoi(argv[3]);
  /* Atachment memoria condivisa per le informazioni delle pedine di un giocatore */
  infoPID = createSharedMem(ftok("./pawn.c", turnoGiocatore), sizeof(struct Destinazione)*SO_NUM_P);
  infoPedine = attachMem(infoPID);
  /* Atachment memorie condivise (scacchiera, infoG) */
  createAttachMem();
  /* Atachment set di semafori */
  getSemID();

  /* Copia della posizione della pedina per non modificare il valore originale
     durante l'esecuzione dei metodi per gli spostamenti */
  posColonna = infoPedine[turno].precColonna;
  posRiga = infoPedine[turno].precRiga;
  /* Inizializzazione informazioni su una cella della scacchiera */
  scacchiera[posRiga*SO_BASE+posColonna].attr.ped.PIDGiocatore = getppid();
  scacchiera[posRiga*SO_BASE+posColonna].attr.ped.PIDPedina = getpid();

  /* Viene liberato il prossimo giocatore per permettere
     una creazione ordinata delle pedine */
  reserve_Sem(SEM_ID_G, (turnoGiocatore+1)%SO_NUM_G, 0);
  if(turnoGiocatore!=SO_NUM_G-1 || turno!=SO_NUM_P-1)
    pause();
  else
    /* Avviso al master che tutte le pedine di tutti i giocatori
       sono state piazzate (prossimo passo = piazzare le bandierine) */
    release_Sem(SEM_ID_G, 0);

  /* Calcolo spostamenti ripetuti */
  while(1){
    wait_Sem(SEM_ID_GAME, 0);
    /* Controllo che la pedina non abbia una destinazione o che non abbia
       sufficienti mosse per arrivarci */
    if(controllaDestinazione()){
      /* Se la pedina non puo' raggiungere una bandierina in un numero minore
         di mosse rispetto a un'altra (dello stesso giocatore) va in attesa */
      if(!confrontaDestinazione())
        pause();
    }
    scegliDirezioneESposta();
  }
}

/* ---- INIZIO METODI ---- */

/* Controllo delle mosse residue della pedina
   sufficienti per raggiungere la bandierina (successiva alla prima) */
boolean controllaDestinazione(){
  int distanza = 0;

  /* Controllo che la pedina non sia stata assegnata a una bandierina */
  if(infoPedine[turno].distanza == SO_BASE+SO_ALTEZZA)
     return TRUE;
  else{
    distanza = valoreAssoluto(posRiga, infoPedine[turno].destinazioneRiga)
               +valoreAssoluto(posColonna, infoPedine[turno].destinazioneColonna);
    /* Ritorna vero se la pedina non ha sufficienti mosse per raggiungere la destinazione */
    return infoPedine[turno].mosseDisponibili<distanza;
  }
}

/* Controllo se una pedina puo' raggiungere una bandierina
   prima di un'altra (dello stesso giocatore) */
boolean confrontaDestinazione(){
  int i, j, minRiga, minColonna, distanza, minimo;

  minimo = SO_BASE+SO_ALTEZZA;
  for(i=0; i<SO_BASE; i++){
    for(j=0; j<SO_ALTEZZA; j++){
      if(scacchiera[j*SO_BASE+i].simbolo == 'B'){
        /* Calcola la distanza tra pedina e bandierina */
        distanza = valoreAssoluto(posRiga, j)+valoreAssoluto(posColonna, i);

        /* Calcola qual e' la bandierina piu' vicina alla pedina stessa */
        if(distanza<minimo && distanza<=infoPedine[turno].mosseDisponibili){
            minimo = distanza;
            minRiga = j;
            minColonna = i;
        }
      }
    }
  }

  /* Se la pedina e' piu' lontana da tutte le bandierine
     rispetto alle altre pedine allora non spreca mosse e va in attesa */
  if(minimo==SO_BASE+SO_ALTEZZA)
    return FALSE;
  /* Cerca la pedina che punta alla bandierina piu' vicina alla pedina stessa
     e confronta le distanze */
  for(i=0; i<SO_NUM_P; i++){
    if(minRiga==infoPedine[i].destinazioneRiga && minColonna==infoPedine[i].destinazioneColonna)
      if(minimo<infoPedine[i].distanza)
        /* La pedina che punta alla bandierina viene bloccata (messa in attesa) */
        infoPedine[i].distanza = SO_BASE+SO_ALTEZZA;
      else
        /* La pedina non punta a una bandierina (non coveniente),
           quindi va in attesa */
        return FALSE;
  }

  /* Assegnamento bandierina alla pedina stessa */
  infoPedine[turno].distanza = minimo;
  infoPedine[turno].destinazioneRiga = minRiga;
  infoPedine[turno].destinazioneColonna = minColonna;
  return TRUE;  /* La pedina puo' muoversi */
}

/* Scelta spostamento piu' conveniente secondo una diagonale tra pedina e bandierina
(se la pedina non si trova sulla diagonale cerca il modo piu' veloce per arrivarci) */
void scegliDirezioneESposta(){
/* Significato valori assunti dalla variabile 'diretto':
   >0 = spostamento in orizzontale
   <=0 = spostamento in verticale
   Il suo valore assoluto corrisponde allo spostamento per raggiungere
   la diagonale (piu' vicina) della bandierina */

  int diretto;
  struct timespec toWait;
  toWait.tv_sec = 0;
  toWait.tv_nsec = SO_MIN_HOLD_NSEC;

  /* Controllo che la bandierina non sia gia' stata presa da altre pedine */
  if(scacchiera[infoPedine[turno].destinazioneRiga*SO_BASE+
  	 infoPedine[turno].destinazioneColonna].simbolo == 'B'){
  	/* Distanza della pedina dalla diagonale piu' vicina */
  	diretto = valoreAssoluto(posColonna,infoPedine[turno].destinazioneColonna)
  			      -valoreAssoluto(posRiga,infoPedine[turno].destinazioneRiga);

    /* Movimento in orizzontale */
  	if(diretto>0){
  		/* Movimento verso sinistra */
  		if(posColonna>infoPedine[turno].destinazioneColonna){
  			if(reserve_Sem(SEM_ID_CELLE, posRiga*SO_BASE+posColonna-1,IPC_NOWAIT)!=-1)
  				sposta(SINISTRA);
  			/* Se la pedina non riesce ad andare a sinistra,
           prova o sotto o sopra (a seconda della posizione della bandierina) */
  			else if(posRiga>infoPedine[turno].destinazioneRiga){
  				if(reserve_Sem(SEM_ID_CELLE,(posRiga-1)*SO_BASE+posColonna,IPC_NOWAIT)!=-1)
  					sposta(SOPRA);
  			}
  			else if(posRiga<infoPedine[turno].destinazioneRiga){
  				if(reserve_Sem(SEM_ID_CELLE,(posRiga+1)*SO_BASE+posColonna,IPC_NOWAIT)!=-1)
  					sposta(SOTTO);
  			}
  		}
  		/* Movimento verso destra */
  		else{
  			if(reserve_Sem(SEM_ID_CELLE,posRiga*SO_BASE+posColonna+1,IPC_NOWAIT)!=-1)
  				sposta(DESTRA);
  			/* Se la pedina non riesce ad andare a destra,
           prova o sotto o sopra (a seconda della posizione della bandierina) */
  			else if(posRiga>infoPedine[turno].destinazioneRiga){
  				if(reserve_Sem(SEM_ID_CELLE,(posRiga-1)*SO_BASE+posColonna,IPC_NOWAIT)!=-1)
  					sposta(SOPRA);
  			}
  			else if(posRiga<infoPedine[turno].destinazioneRiga){
  				if(reserve_Sem(SEM_ID_CELLE,(posRiga+1)*SO_BASE+posColonna,IPC_NOWAIT)!=-1)
  					sposta(SOTTO);
  			}
  		}
  	}
    /* Movimento in verticale (anche quando la pedina e' sulla diagonale) */
    else if(diretto<=0){
  		/* Movimento verso il basso */
  		if(posRiga<infoPedine[turno].destinazioneRiga){
  			if(reserve_Sem(SEM_ID_CELLE,(posRiga+1)*SO_BASE+posColonna,IPC_NOWAIT)!=-1)
  				sposta(SOTTO);
  			/* Se la pedina non riesce ad andare verso il basso,
           prova o a destra o a sinistra (a seconda della posizione della bandierina) */
  			else if(posColonna<infoPedine[turno].destinazioneColonna){
  				if(reserve_Sem(SEM_ID_CELLE,posRiga*SO_BASE+posColonna+1,IPC_NOWAIT)!=-1)
  					sposta(DESTRA);
  			}
  			else if(posColonna>infoPedine[turno].destinazioneColonna){
  				if(reserve_Sem(SEM_ID_CELLE, posRiga*SO_BASE+posColonna-1,IPC_NOWAIT)!=-1)
  					sposta(SINISTRA);
  			}
  		}
  		/* Movimento verso l'alto */
  		else{
  			if(reserve_Sem(SEM_ID_CELLE,(posRiga-1)*SO_BASE+posColonna,IPC_NOWAIT)!=-1)
  				sposta(SOPRA);
  			/* Se la pedina non riesce ad andare verso l'alto,
           prova o a destra o a sinistra (a seconda della posizione della bandierina) */
  			else if(posColonna<infoPedine[turno].destinazioneColonna){
  				if(reserve_Sem(SEM_ID_CELLE,posRiga*SO_BASE+posColonna+1,IPC_NOWAIT)!=-1)
  					sposta(DESTRA);
  			}
  			else if(posColonna>infoPedine[turno].destinazioneColonna){
  				if(reserve_Sem(SEM_ID_CELLE, posRiga*SO_BASE+posColonna-1,IPC_NOWAIT)!=-1)
  					sposta(SINISTRA);
  			}
  		} /* Chiusura else per movimento verso l'alto */
  	}  /* Chiusura if per movimento verticale */
  } /* Chiusura controllo che la bandinerina non sia gia' stata presa da altre pedine */
  nanosleep(&toWait, NULL);
}

/* Spostamento effettivo della pedina sulla scacchiera verso la bandierina */
void sposta(move direzione){
  int i;
	boolean bandierinaCat = 0;

  /* Azzeramento e rilascio della memoria della posizione precedente */
  scacchiera[posRiga*SO_BASE+posColonna].simbolo = ' ';
  scacchiera[posRiga*SO_BASE+posColonna].attr.punti = 0;
  /* Rilascio semaforo riservato in scegliDirezioneESposta() */
  release_Sem(SEM_ID_CELLE,posRiga*SO_BASE+posColonna);

	switch (direzione){
		case SOPRA:
			/* Controllo cattura di una bandierina */
			if(scacchiera[(posRiga-1)*SO_BASE+posColonna].simbolo=='B')
				bandierinaCat = 1;
			posRiga--; /* Aggiornamento della posizione della pedina */
		  break;
		case SOTTO:
			/* Controllo cattura di una bandierina */
			if(scacchiera[(posRiga+1)*SO_BASE+posColonna].simbolo=='B')
				bandierinaCat = 1;
			posRiga++; /* Aggiornamento della posizione della pedina */
	    break;
		case DESTRA:
			/* Controllo cattura di una bandierina */
			if(scacchiera[posRiga*SO_BASE+posColonna+1].simbolo=='B')
				bandierinaCat = 1;
			posColonna++;  	/* Aggiornamento della posizione della pedina */
		  break;
		case SINISTRA:
			/* Controllo cattura di una bandierina */
			if(scacchiera[posRiga*SO_BASE+posColonna-1].simbolo=='B')
				bandierinaCat = 1;
			posColonna--;  /* Aggiornamento della posizione della pedina */
		  break;
		default:    /* Questo caso non dovrebbe mai avvenire */
      printf("Errore: qualcosa e' andato storto nello spostamento della pedina\n");
      exit(EXIT_FAILURE);
		  break;
	}

  /* Aggiornamento della scacchiera */
  scacchiera[posRiga*SO_BASE+posColonna].simbolo = 'P';

	/* Aggiornamento della pedina nella MC delle informazioni delle pedine */
  infoPedine[turno].precRiga = posRiga;
  infoPedine[turno].precColonna = posColonna;
  infoPedine[turno].mosseDisponibili--;

	/* Aggiornamento delle mosse del giocatore (serve per la stampa della classifica) */
  reserve_Sem(SEM_ID_G, turnoGiocatore, 0);
  infoG[turnoGiocatore].mosseUsate++;
  release_Sem(SEM_ID_G, turnoGiocatore);
	/* Aggiornamento dati relativi alla cattura della bandierina */
  if(bandierinaCat){
		infoPedine[turno].distanza = SO_BASE+SO_ALTEZZA;   /* La pedina potra' puntare a un'altra bandierina */
		infoG[turnoGiocatore].punti+=scacchiera[posRiga*SO_BASE+posColonna].attr.punti;
		confrontaDestinazione();    /* Viene cercata un'altra bandierina vicina rispetto ad altre pedine */
	}
  /* Aggiornamento infomazioni su MC della scacchiera */
  scacchiera[posRiga*SO_BASE+posColonna].attr.ped.PIDGiocatore = getppid();
  scacchiera[posRiga*SO_BASE+posColonna].attr.ped.PIDPedina = getpid();
}

/* Gestione segnali */
void handleSignal(int signal){
  /* Segnale per la terminazione con la stampa della classifica (nel master) */
  if(signal==SIGINT){
    /* Detachment delle memorie condivise */
    shmdt(infoG);
    shmdt(infoPedine);
    shmdt(scacchiera);
    exit(EXIT_SUCCESS);
  }
}
