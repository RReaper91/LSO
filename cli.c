#ifndef W_OS_LINUX
	#include <ncurses.h>	/* initscr, attron, move, addch, refresh, attroff, mvprintw, noecho, start_color, curs_set, cbreak, timeout, keypad */
#elif TARGET_OS_X
	#include <curses.h>		/* simile a ncurses.h */
#endif

#include <stdio.h>		/* printf, perror, getch, fflush, sprintf */
#include <stdlib.h>		/* atoi, malloc, exit */
#include <string.h>		/* strcpy, strncpy, strlen, strcmp */
#include <sys/types.h>	/* socket, connect */
#include <sys/socket.h>	/* socket, inet_aton, inet_addr, connect */
#include <arpa/inet.h>	/* htons, inet_aton, inet_addr */
#include <unistd.h>		/* close, write, read, exit, sleep, tcgetattr, getch */
#include <termios.h>	/* tcgetattr, getch */
#include <pthread.h>	/* pthread_create, pthread_exit, pthread_join */
#include <signal.h>		/* signal */
#include <errno.h>		/* perror */
#include <netinet/in.h>	/* inet_aton, inet_addr */
#include "fun.h"

#define GAMENAME			"CLASH UNIVERSITY"
#define RELESE				"Alpha 0.2.1"
#define GREEN_COLOR_TEXT	"\x1b[32m"
#define RED_COLOR_TEXT		"\x1b[31m"
#define RESET_COLOR_TEXT	"\x1b[0m"

#define KK	0
#define RK	1
#define GK	2
#define YK	3
#define BK	4

int sock_client;
int porta = 0;

/* funzione main */
int main(int argc, char* argv[]){
	int choise, check;
	
	// inizializzazione dei segnali da catturare
	signal(SIGINT, myHandler_SIGINT);		// SIGINT - interruzione da tastiera
	signal(SIGPIPE, myHandler_SIGPIPE);		// SIGPIPE - broken pipe: scrittura sulla pipe senza lettori
	
	// argv[1] = porta
	// argv[3] = indirizzo ip del server
	if (argc != 5) {	// errore di inserimento dati
		printHelp();
		exit(EXIT_FAILURE);
	} else{
		if (strcmp((argv[1]), "-p") == 0)
			porta = atoi(argv[2]);
		else{
			printHelp();
			exit(EXIT_FAILURE);
		}
		if (strcmp((argv[3]), "-ip") != 0){
			printHelp();
			exit(EXIT_FAILURE);
		}
	}

	// creazione della socket
	if ((sock_client = socket(AF_INET, SOCK_STREAM, 0)) == -1){
		printf("Socket client error!\n");
		exit(EXIT_FAILURE);
	}

	// associazione dei parametri per la connessione
	server.sin_family = AF_INET;
	server.sin_port = htons(porta);
	inet_aton(argv[4], &server.sin_addr);
	//server.sin_addr.s_addr = inet_addr(argv[4]);

	// connessione al server
	if ((check = connect(sock_client, (struct sockaddr *)&server, sizeof(struct sockaddr))) == -1){
		system("clear");
		printf("[x] Server %s è "RED_COLOR_TEXT"OFFLINE"RESET_COLOR_TEXT" o irragiungibile!\n\n\n", GAMENAME);
		exit(EXIT_FAILURE);
	}

	// gestione del menu
	system("clear");
	while ((choise = firstMenu()) != 3) {
		switch(choise){
			case 1:	// menu di login dell'utente
				check = userManagement("LOG\0");
				// return -1 = utente gia online; 
				// return -2 = utente non registrato; 
				// return 1 = login effettuato
				if (check == -1){
					printf(RED_COLOR_TEXT"\n\tLogin rifiutato. Questo utente è gia"RESET_COLOR_TEXT""GREEN_COLOR_TEXT" ONLINE!\n"RESET_COLOR_TEXT);
					sleep(2);
					break;
				} else if (check == -2){
					printf(RED_COLOR_TEXT"\n\tLogin rifiutato. Questo utente non è registrato!\n"RESET_COLOR_TEXT);
					sleep(2);
					break;
				} else if (check == 1){
					printf(GREEN_COLOR_TEXT"\n\tLogin effettuato con successo!\n"RESET_COLOR_TEXT);
					sleep(2);
					int log = secondMenu();
					if (log == 1)
						write(sock_client, "202", 3);		// il giocatore è pronto per iniziare la partita
					else if (log == 2){
						write(sock_client, "203", 3);		// logout
						break;
					}
					_lobby();

					printf(GREEN_COLOR_TEXT"\n\tAttesa avversari..\n"RESET_COLOR_TEXT);

					// attesa di avversari
					pthread_t onlinePlayersAcquisition_th;
					pthread_create(&onlinePlayersAcquisition_th, NULL, onlinePlayerAcquisition, NULL);
					pthread_join(onlinePlayersAcquisition_th, NULL);

					// stretta di mano per transazione avvenuta
					write(sock_client, "204", 3);
					char answer[3];
					read(sock_client, &answer, 3);
					if (atoi(answer) == 777){
						// attesa per l'acquisizione della mappa di gioco
						pthread_t mapDataAcquisition_th;
						pthread_create(&mapDataAcquisition_th, NULL, mapDataAcquisition, NULL);
						pthread_join(mapDataAcquisition_th, NULL);
					} else{
						printf(RED_COLOR_TEXT"\n\tErrore di comunicazione col server. Chiusura inaspettata dell'applicazione.\n"RESET_COLOR_TEXT);
						exit(EXIT_FAILURE);
					}

					runGame();
					system("clear");
					exit(EXIT_SUCCESS);
				}
				break;

			case 2:	// menu di registrazione dell'utente
				printf("Registrazione nuovo utente\nPer poter giocare a %s è necessario registrarsi!\n\n", GAMENAME);
				check = userManagement("REG\0");
				if (check == 1){
					printf(GREEN_COLOR_TEXT"\n\tRegistrazione avvenuta con successo!\n"RESET_COLOR_TEXT);
					sleep(2);
				} else{
					printf(RED_COLOR_TEXT"\n\tErrore nella registrazione. Registrazione fallita!\n"RESET_COLOR_TEXT);
				}
				break;
		}
	}

	write(sock_client, "000", 3); // exit
	close(sock_client);
	exit(EXIT_SUCCESS);
}
