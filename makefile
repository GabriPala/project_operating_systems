# Compila ed esegue con i seguenti comandi: 1) make  2) ./master Parameters/NomeFile.ini

CFLAGS = -std=c89 -pedantic

output: master.c player.c pawn.c common.o semaphore.o
	gcc common.o semaphore.o master.c $(CFLAGS) -o master
	gcc common.o semaphore.o player.c $(CFLAGS) -o player
	gcc common.o semaphore.o pawn.c $(CFLAGS) -o pawn

common.o: common.c common.h
	gcc common.c -c

semaphore.o: semaphore.c semaphore.h
	gcc semaphore.c -c

