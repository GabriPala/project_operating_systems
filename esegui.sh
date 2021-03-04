#!/bin/bash
clear
#controlla la presenza del file pawn.c
if [ -f pawn.c ]
then
	#controlla la presenza del file player.c
	if [ -f player.c ]
	then
		#controlla la presenza del file master.c
		if [ -f master.c ]
		then
			#compila common.c
			gcc -std=c89 -pedantic common.c -c
			#compila semaphore.c
			gcc -std=c89 -pedantic semaphore.c -c
			#compila pedine
			gcc common.c semaphore.c pawn.c -std=c89 -pedantic -o pawn
			#compila giocatori
			gcc common.c semaphore.c player.c -std=c89 -pedantic -o player
			#compila master
			gcc common.c semaphore.c master.c -std=c89 -pedantic -o master

			#esegui
			echo "Scegli il file da usare per i parametri: "
			echo "1 - 'ParametersEasy.ini' "
			echo "2 - 'ParametersHard.ini' "
			echo "3 - 'ParametersCustom.ini' "
			echo "Inserisci numero: "
			read i
			if [ $i == 1 ]
			then
				./master Parameters/ParametersEasy.ini
			fi

			if [ $i == 2 ]
			then
				./master Parameters/ParametersHard.ini
			fi
			if [ $i == 3 ]
			then
				./master Parameters/ParametersCustom.ini
			fi
			
		else
			echo "In questa directory non e' presente master.c"
		fi
	else
		echo "In questa directory non e' presente player.c"
	fi
else
	echo "In questa directory non e' presente pawn.c"
fi


#Compilare ed eseguire con: chmod +rx esegui.sh poi ./esegui.sh
