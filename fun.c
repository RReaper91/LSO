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

/* Variabili globali */
/* Variabili di "Game" per mappa, giocatori, bandiere, ostacoli ecc */
unsigned char **globalMap;
int globalRow, globalCol;
int keyPressed;
int startRowPos, startColPos;

User_t userInfo;
OnPlayerServer_t *list_onUser = NULL;

/* verifica se la lista è vuota */
int isempty(OnPlayerServer_t *head){
	if (head == NULL)
		return 1;
	else
		return -1;
}

/* inserimento di un nuovo giocatore online */
OnPlayerServer_t *addUser(OnPlayerServer_t *head, char utente[]){
	OnPlayerServer_t *temp = (OnPlayerServer_t*)malloc(sizeof(OnPlayerServer_t));

	strcpy(temp->username, utente);
	temp->succ = NULL;
	if (isempty(head) == 1)
		head = temp;
	else{
		temp->succ = head;
		head = temp;
	}
	return head;
}

/* funzione per creare una lettura bloccante */
/* getch() ha, apparentemente, una coda di input */
int readKey(){
	int ch;
	// ERR è una costante della libreria <ncurses>
	// ERR indica che non è stato premuto alcun tasto
	while (getch() != ERR);	// pulisce il buffer dalla presenza di tutti i caratteri
	while ((ch = getch()) == ERR); // attesa di un carattere e da mettere in ch
	return ch;
}

/* funzione che permette di interagire (graficamente) col terminale */
char getchLinux(){
	char buf = 0;
	struct termios old = {0};

	fflush(stdout);

	if (tcgetattr(STDIN_FILENO, &old) < 0)	// ottieni la configurazione corrente dell'interfaccia seriale
		perror("tcsetattr()");

	// lflag: local flag (bitmask)
	// ICANON abilita la modalità di input canonico
	// ECHO carattere di input echo
	old.c_lflag &= ~ICANON;	// ICANON off
	old.c_lflag &= ~ECHO;	// ECHO off
	old.c_cc[VMIN] = 1;		// numero di byte sufficienti per tornare alla read()
	old.c_cc[VTIME] = 0;	// timer tra caratteri off

	if (tcsetattr(STDIN_FILENO, TCSANOW, &old) < 0)	// cambia la configurazione (immediatamente)
		perror("tcsetattr ICANON");
	if (read(STDIN_FILENO, &buf, 1) < 0)
		perror("read()");

	old.c_lflag |= ICANON;	// tutto viene memorizzato in un buffer e può essere modificato fi quando non viene immesso un ritorno o un feed di riga
	old.c_lflag |= ECHO;

	if (tcsetattr(STDIN_FILENO, TCSADRAIN, &old) < 0)	// cambia la configurazione dopo che tutti gli output scritti sul fd sono stati trasmessi 
		perror ("tcsetattr ~ICANON");

	return buf;
}

/* funzione di inizializzazione dei vari item */
void initItem(){
	int i, j;
	char blueFlag = 'P';
	char redFlag = 'P';

	// inizializzazione delle coppie di colori
	init_pair(YK, COLOR_YELLOW, COLOR_BLACK);	// YK
	init_pair(RK, COLOR_RED, COLOR_BLACK);		// RK
	init_pair(GK, COLOR_GREEN, COLOR_BLACK);	// GK
	init_pair(BK, COLOR_BLUE, COLOR_BLACK);		// BK

	// posizionamento delle mine sulla mappa
	for(i=0; i<globalRow; i++){
		for(j=0; j<globalCol; j++){
			switch(globalMap[i][j]){
				case '1':		// mina di colore GIALLO
					attron(COLOR_PAIR(YK));		// attiva quale coppia di colori viene utilizzata
					move(i+1, j+1);				// sposta il cursore alla posizione indicata
					addch('X');					// equivalente alla putch
					refresh();					// permette la visualizzazione di tutti i comandi eseguiti
					attroff(COLOR_PAIR(YK));	// disattiva la coppia di colori prima abilitata
					break;

				case '0':		// ostacolo di colore NERO
					attron(COLOR_PAIR(KK));
					move(i+1, j+1);
					addch('O');
					refresh();
					attroff(COLOR_PAIR(KK));
					break;

				case 'r':		// bandiera di colore ROSSO
					if (userInfo.faction == 'r'){
						attron(COLOR_PAIR(RK));
						move(i+1, j+1);
						addch(redFlag);
						attroff(COLOR_PAIR(RK));
						refresh();
					} else if (userInfo.faction == 'b'){
						move(i+1, j+1);
						addch(' ');
						refresh();
					}
					break;

				case 'b':		// bandiera di colore BLU
					if (userInfo.faction == 'b'){
						move(i+1, j+1);
						attron(COLOR_PAIR(BK));
						addch(blueFlag);
						attroff(COLOR_PAIR(BK));
						refresh();
					} else if (userInfo.faction == 'r'){
						move(i+1, j+1);
						addch(' ');
						refresh();
					}
					break;

				case 'B':		// player di colore BLU
					if (userInfo.faction == 'b'){
						move(i+1, j+1);
						attron(COLOR_PAIR(BK));
						addch('@');
						attroff(COLOR_PAIR(BK));
						refresh();
					} else if (userInfo.faction == 'r'){
						move(i+1, j+1);
						addch(' ');
						refresh();
					}
					break;

				case 'R':		// player di colore ROSSO
					if (userInfo.faction == 'r'){
						move(i+1, j+1);
						attron(COLOR_PAIR(RK));
						addch('@');
						attroff(COLOR_PAIR(RK));
						refresh();
					} else if (userInfo.faction == 'b'){
						move(i+1, j+1);
						addch(' ');
						refresh();
					}
					break;

				case '2':		// medikit di colore VERDE
					attron(COLOR_PAIR(GK));
					move(i+1, j+1);
					addch('+');
					attroff(COLOR_PAIR(GK));
					refresh();
					break;

				case ' ':		// spazio di movimento
					move(i+1, j+1);
					addch(' ');
					refresh();
					break;
			}
		}
	}
	refresh();
}

/* funzione che permette di disegnare la mappa */
void drawsMap(){
	int i, j;

	for(i=0; i<=globalRow+1; i++){
		for(j=0; j<=globalCol+1; j++){
			move(i, j);
			if ((i == 0) || (i == globalRow+1)){
				if ((i == 0) && (j == globalCol+1))
					addch(ACS_URCORNER);	// angolo superiore destro
				else if ((i == 0) && (j == 0))
					addch(ACS_ULCORNER);	// angolo superiore sinistro
				else if ((i == globalRow+1) && (j == 0))
					addch(ACS_LLCORNER);	// angolo inferiore sinistro
				else if ((i == globalRow+1) && (j == globalCol+1))
					addch(ACS_LRCORNER);	// angolo inferiore destro
				else
					addch(ACS_HLINE);		// linea orizzontale
			} else if ((j == 0) || (j == globalCol+1))
				addch(ACS_VLINE);			// linea verticale
			else
				addch(globalMap[i-1][j-1]);
		}
	}
	refresh();
	initItem();
}

/* funzione che alloca dinamicamente la mappa */
unsigned char **allocateMap(){
	int i, j;
	unsigned char **map = (unsigned char **)malloc(globalRow * sizeof(unsigned char*));	

	// allocazione mappa
	for(i=0; i<globalRow; i++){
		map[i] = (unsigned char *)malloc(globalCol * sizeof(unsigned char));
		for(j=0; j<globalCol; j++){
			map[i][j] = ' ';
		}
	}
	return map;
}

/* funzione per la registrazione e/o login dell'utente sul server */
int userManagement(char richiesta[]){
	char userCredentials[1024];
	char username[10];
	char password[10];
	int nreadUser, nreadPas;
	char serverReply[5];

	if ((strcmp(richiesta, "REG\0")) == 0){
		write(sock_client, "200", 3);		// richiesta di registrazione di un nuovo utente

		// inizio procedura di richiesta, modifica e invio credenziali al server
		write(STDOUT_FILENO, "\tNew username: ", strlen("\tNew username: "));
		nreadUser = read(STDIN_FILENO, &username, 10);
		write(STDOUT_FILENO, "\tNew password: ", strlen("\tNew password: "));
		nreadPas = read(STDIN_FILENO, &password, 10);

		// creazione della stringa <username,password>
		username[nreadUser-1] = '\0';
		password[nreadPas-1] = '\0';
		int n = sprintf(userCredentials, "%s,%s;", username, password);	

		write(sock_client, &userCredentials, n+1);		// invio delle credenziali al server

		// conferma del server
		read(sock_client, &serverReply, 5);
		if ((strcmp(serverReply, "SUCC\0")) == 0)
			return 1;
		else
			return 0;
	} else if ((strcmp(richiesta, "LOG\0")) == 0){
		write(sock_client, "201", 3);		// richiesta di una login
		
		// input da tastiera delle credenziali
		write(STDOUT_FILENO, "\tUsername: ", strlen("\tUsername: "));
		nreadUser = read(STDIN_FILENO, &username, 10);
		write(STDOUT_FILENO, "\tPassword: ", strlen("\tPassword: "));
		nreadPas = read(STDIN_FILENO, &password, 10);

		// creazione della stringa <username,password>
		username[nreadUser-1] = '\0';
		password[nreadPas-1] = '\0';
		int n = sprintf(userCredentials, "%s,%s;", username, password);

		write(sock_client, &userCredentials, n+1);		// invio delle credenziali al server

		// conferma del server
		read(sock_client, &serverReply, 3);
		if (atoi(serverReply) == 665 ){
			// salvataggio dei dati che (corretti) nella struct User_t (database)
			strncpy(userInfo.username, username, nreadUser);
			strncpy(userInfo.password, password, nreadPas);
			return 1;
		} else if (atoi(serverReply) == 666 ){	// utente già online
			return -1;
		} else if (atoi(serverReply) == 667 ){	// utente non registrato
			return -2;
		}
	}
	return 0;
}

/* funzione di acquisizione dati della mappa */
void *mapDataAcquisition(){
	// acquisizione della dimensione della mappa
	char sizeMatrix[2];
	read(sock_client, &sizeMatrix, 2);
	globalRow = globalCol = atoi(sizeMatrix);

	globalMap = allocateMap();	// creazione della mappa

	// acquisizione della vita iniziale del giocatore
	char initHP[4];
	read(sock_client, &initHP, 4);
	userInfo.healthPoint = atoi(initHP);

	// lettura della mappa
	int i, j;
	for(i=0; i<globalRow; i++){
		for(j=0; j<globalCol; j++){
			  read(sock_client, &globalMap[i][j], sizeof(char));
		}
	}
	pthread_exit(NULL);
}

/* funzione per l'acquisizione dei nuovi dati della mappa */
void newMapDataAcquisition(){
	// lettura della vita del giocatore
	char currentHP[4];
	read(sock_client, &currentHP, 4);
	userInfo.healthPoint = atoi(currentHP);

	if ((userInfo.healthPoint > 0) && (userInfo.faction == 'r')){
		mvprintw(17, 110, "     ");	// simile alla printf, permette di indicare le coordinate dello schermo
		refresh();
		mvprintw(17, 110, "%d", userInfo.healthPoint);
	} else if ((userInfo.healthPoint <= 0) && (userInfo.faction == 'r')){
		mvprintw(17, 110, "0     ");
		mvprintw(16, 110, "Health");
	}
	if ((userInfo.healthPoint > 0) && (userInfo.faction == 'b')){
		mvprintw(17, 56, "     ");
		refresh();
		mvprintw(17, 56, " %d", userInfo.healthPoint);
	} else if ((userInfo.healthPoint <= 0) && (userInfo.faction == 'b')){
		mvprintw(17, 56, "0    ");
		mvprintw(16, 55, "Health");
	}
	refresh();

	// aggiornamento della mappa
	int i, j;
	for(i=0; i<globalRow; i++){
		for(j=0; j<globalCol; j++){
			read(sock_client, &globalMap[i][j], sizeof(char));
		}
	}
}

/* funzione per la lettura dal server della stringa <giocatori,fazione> */
void *onlinePlayerAcquisition(){
	char command[200], player[200], nickname[200];
	int j;
	int rigaB = 18,
		rigaR = 18;
		
	command[0] = '\0';
	nickname[0] = '\0';
	player[0] = '\0';

	// command = colore1,nickanme1;colore2,nickanme2;...
	int nread = read(sock_client, &command, 200);
	int i = 0,
		posNick = 0;

	while (i < nread){
		// player = colore,nickname;
		while (command[i] != ';'){
			player[posNick] = command[i];
			i++;
			posNick++;
		}
		player[posNick] = '\0';

		list_onUser = addUser(list_onUser, player);		// inserimento dell'utente nella lista dei giocatori online

		// stampa dell'utente all'interno della fazione di appartenenza
		if (player[0] == 'b'){
			int offs = 2;		// prelievo del nome
			for(j=2; j<strlen(player); j++){
				nickname[j] = player[offs];
				offs++;
			}
			nickname[j] = '\0';
			if ((strcmp(nickname, userInfo.username)) == 0){
			} else
				mvprintw(rigaB, 40, "%s", &nickname);
			refresh();
			rigaB++;
		} else if (player[0] == 'r'){
			int offs = 2;		// prelievo del nome
			for(j=2; j<strlen(player); j++){
				nickname[j] = player[offs];
				offs++;
			}
			nickname[j] = '\0';
			mvprintw(rigaR, 100, "%s", &nickname);
			refresh();
			rigaR++;
		}
		player[0] = '\0';
		nickname[0] = '\0';
		posNick = 0;
		i++;
		refresh();
	}
	pthread_exit(NULL);
}

/* funzione di gioco */
void runGame(){
	int exitGame = 0;

	// start curses mode
	initscr();				// prepara la libreria per essere utilizzata
	noecho();				// permette di non far visualizzare a schermo ciò che si scrive
	start_color();			// permette di utilizzare i colori
	curs_set(0);			// visibilità del cursore (invisibile)
	cbreak();				// rende immediatamente disponibili al programma i tasti premuti senza dover premere <invio>
	timeout(0);				// permette di regolare il comportamento del programma. se si fornisce come parametro 0, getch() non aspetta che l'utente prema un tasto e prosegue
	keypad(stdscr,true);	// attiva/disattiva la gestione dei tasti funzione

	// inizializzazione delle coppie di colori
	init_pair(RK, COLOR_RED, COLOR_BLACK);
	init_pair(BK, COLOR_BLUE, COLOR_BLACK);

	drawsMap();

	// stampa del terminale alle coordinate prestabilite
	mvprintw(1, 50, "Benvenuto in %s <%s>, soldato %s", GAMENAME, RELESE, userInfo.username);
	mvprintw(3, 40, "Regole generali:");
	mvprintw(4, 41, "Cattura la bandiera avversaria e sarai il vincitore!!");
	mvprintw(6, 40, "Effetti:");
	mvprintw(7, 41, "X <mina> = -600 PF");
	mvprintw(8, 41, "O <ostacolo>");
	mvprintw(9, 41, "@ <giocatore>");
	mvprintw(10, 41, "+ <medikit> = +300 PF");
	mvprintw(11, 41, "P <bandiera>");
	mvprintw(13, 40, "Comandi:");
	mvprintw(14, 41, "Frecce direzionali");
	mvprintw(16, 40, "BLU");
	mvprintw(16, 100, "ROSSI");

	// stampa del giocatore nella fazione di appartenenza
	if (userInfo.faction == 'b'){
		mvprintw(17, 56, "PF");
		mvprintw(17, 72, "Mossa");
		mvprintw(18, 40, "%s", userInfo.username);
		mvprintw(18, 56, "%d", userInfo.healthPoint);
		refresh();
	} else if (userInfo.faction == 'r'){
		mvprintw(17, 110, "PF");
		mvprintw(17, 127, "Mossa");
		mvprintw(18, 100, "%s", userInfo.username);
		mvprintw(18, 110, "%d", userInfo.healthPoint);
		refresh();
	}
	refresh();

	// condizioni di uscita dal gioco
	while (exitGame != 1){
		if ((userInfo.healthPoint) <= 0){
			mvprintw(18, 72, "MORTO   ");
			refresh();
			// istruzione fittizia per simulare la presenza del giocatore morto
			write(sock_client, "1", 1);
		} else{
			keyPressed = readKey();
			if ((keyPressed != KEY_UP) && (keyPressed != KEY_DOWN) && (keyPressed != KEY_LEFT) && (keyPressed != KEY_RIGHT))
				continue;
			switch(keyPressed){
				case KEY_UP:
					write (sock_client, "1", 1);
					mvprintw(18, 128, "          ");
					mvprintw(18, 73, "          ");
					refresh();
					// stampa della mossa effettuata
					if (userInfo.faction == 'b'){
						if (userInfo.healthPoint <= 0){
							mvprintw(18, 72, "MORTO");
							break;
						}
						mvprintw(18, 72, "SU");
					} else if (userInfo.faction == 'r'){
						if (userInfo.healthPoint <= 0){
							mvprintw(18, 127, "MORTO");
							break;
						}
						mvprintw(18, 127, "SU");
					}
					break;

				case KEY_DOWN:
					write(sock_client, "2", 1);
					mvprintw(18, 128, "          ");
					mvprintw(18, 73, "          ");
					refresh();
					// stampa della mossa effettuata
					if (userInfo.faction == 'b'){
						if (userInfo.healthPoint <= 0){
							mvprintw(18, 72, "MORTO");
							break;
						}
						mvprintw(18, 72, "GIU");
					} else if (userInfo.faction == 'r'){
						if (userInfo.healthPoint <= 0){
							mvprintw(18, 127, "MORTO");
							break;
						}
						mvprintw(18, 127, "GIU");
					}
					break;

				case KEY_LEFT:
					write(sock_client, "3", 1);
					mvprintw(18, 128, "          ");
					mvprintw(18, 73, "          ");
					refresh();
					// stampa della mossa effettuata
					if (userInfo.faction == 'b'){
						if (userInfo.healthPoint <= 0){
							mvprintw(18, 72, "MORTO");
							break;
						}
						mvprintw(18, 72, "SINISTRA");
					} else if (userInfo.faction == 'r'){
						if (userInfo.healthPoint <= 0){
							mvprintw(18, 127, "MORTO");
							break;
						}
						mvprintw(18, 127, "SINISTRA");
					}
					break;

				case KEY_RIGHT:
					write(sock_client, "4", 1);
					mvprintw(18, 128, "          ");
					mvprintw(18, 73, "          ");
					refresh();
					// stampa della mossa effettuata
					if (userInfo.faction == 'b'){
						if (userInfo.healthPoint <= 0){
							mvprintw(18, 72, "MORTO");
							break;
						}
						mvprintw(18, 72, "DESTRA");
					} else if (userInfo.faction == 'r'){
						if (userInfo.healthPoint <= 0){
							mvprintw(18, 127, "MORTO");
							break;
						}
						mvprintw(18, 127, "DESTRA");
					}
					break;
				}
				mvprintw(21, 50, "Mossa effettuata, attendi che gli altri terminino la loro..");
				refresh();
			} // fine else

		char command;
		read(sock_client, &command, 3);
		switch(atoi(&command)){
			case 988:		// pronti per giocare
				newMapDataAcquisition();
				initItem();
				break;

			case 987:		// time over
				mvprintw(22, 50, "Tempo Scaduto - Pareggio");
				refresh();
				sleep(5);
				exitGame = 1;
				break;

			case 986:		// blue won
				mvprintw(22, 50, "L'esercito BLU ha vinto!");
				refresh();
				sleep(5);
				exitGame = 1;
				break;

			case 985:		// red won
				mvprintw(22, 50, "L'esercito ROSSO ha vinto!");
				refresh();
				sleep(5);
				exitGame = 1;
				break;
		}

		// pulizia dello schermo.
		mvprintw(21, 50, "                                                              ");
		mvprintw(18, 61, "          ");
		mvprintw(18, 116, "           ");
		refresh();

	} // while
	endwin();		// chiude l'utilizzo della libreria
}

/* panoramica regole, inserimento spawn e fazione. */
void _lobby(){
	char choise, command;

	// start curses mode
	initscr();
	noecho();
	start_color();
	curs_set(0);
	keypad(stdscr, true);

	init_pair(RK, COLOR_RED, COLOR_BLACK);
	init_pair(GK, COLOR_GREEN, COLOR_BLACK);

	attron(COLOR_PAIR(RK));
	mvprintw(1, 40, "Benvenuto in %s <%s>, soldato %s", GAMENAME, RELESE, userInfo.username);
	attroff(COLOR_PAIR(RK));

	attron(COLOR_PAIR(GK));
	mvprintw(3, 20, "Panoramica di gioco");
	attroff(COLOR_PAIR(GK));

	mvprintw(4, 20, "Il gioco consiste in una simulazione di guerra tra due eserciti.");
	mvprintw(5, 20, "Il giocatore verrà 'proiettato' in un campo di battaglia all'interno del quale sono presenti: ");
	mvprintw(6, 20, "ostacoli, mine, medikit e due bandiere.");
	mvprintw(7, 20, "Ogni giocatore potrà spostarsi di una posizione alla volta utilizzando le frecce direzionali: ");
	mvprintw(8, 20, "sopra, sotto, destra e sinistra.");

	attron(COLOR_PAIR(GK));
	mvprintw(10, 20, "Scopo del gioco");
	attroff(COLOR_PAIR(GK));

	mvprintw(11, 20, "Lo scopo del gioco consiste nel trovare e catturare la bandiera avversaria. ");
	
	attron(COLOR_PAIR(GK));
	mvprintw(13, 20, "Regole del gioco");
	attroff(COLOR_PAIR(GK));

	mvprintw(14, 20, "Lo scadere del tempo fa terminare la partita con un PAREGGIO.");
	mvprintw(15, 20, "Ogni giocatore inizia la partita con 5000 punti ferita, spendibili in: ");
	mvprintw(16, 20, "	-1 punto salute per ogni spostamento effettuato");
	mvprintw(17, 20, "	-600 punti salute se si calpesta una mina");
	mvprintw(18, 20, "	+300 punti salute se si prende un medikit");
	refresh();

	char h;
	read(sock_client, &h, 4);
	if (atoi(&h) == 555){
		mvprintw(21, 20, "<<Server pronto alla ricezione dati>>");
		refresh();
		sleep(2);
		do {
			attron(COLOR_PAIR(GK));
			mvprintw(21, 20, "Prima di combattere, scegli fazione e una posizione!");
			refresh();
			attroff(COLOR_PAIR(GK));
			mvprintw(22, 20, "Premi b per l'esercito blue e r per quello rosso..");
			refresh();
			choise = getch();
			switch(choise){
				case 'b':
					mvprintw(23, 24, "Hai scelto la fazione BLU.     ");
					userInfo.faction = choise;
					refresh();
					break;

				case 'r':
					mvprintw(23, 24, "Hai scelto la fazione ROSSA.   ");
					userInfo.faction = choise;
					refresh();
					break;
			}
			mvprintw(24, 24, "Premi INVIO per continuare..");
			refresh();
		} while (choise != '\n');

		// meccanismo di controllo dello spawn
		int confirmsPos = 0;
		while (confirmsPos != 1){
			mvprintw(26, 20, "Dove vuoi che spawnare? - premi INVIO per confermare.");
			mvprintw(27, 20, "Riga = ");
			refresh();
			char k;
			char rowPos[3];
			int i = 0,
				column = 27,
				max2Digits = 0,
				confirmNum = 0;
			while (confirmNum != 1){
				while ((k=getch()) != '\n' ){
					if ((k < '0') || (k > '9')){
						mvprintw(27, 31, "Attenzione! consentito soltanto input numerico! Riprova!");
						refresh();
						sleep(2);
						mvprintw(27, 27, "                                                                      ");
						refresh();
						column = 27;
						max2Digits = 0;
						rowPos[0] = '\0';
						i = 0;
						continue;
					}
					if (max2Digits >= 2){
						mvprintw(27, 31, "Attenzione! consentita scelta di massimo 2 cifre");
						refresh();
						sleep(2);
						mvprintw(27, 27, "                                                                      ");
						refresh();
						max2Digits = 0;
						column = 27;
						rowPos[0] = '\0';
						i = 0;
						continue;
					}
					rowPos[i] = k;
					mvprintw(27, column, "%c", rowPos[i]);
					refresh();
					i++;
					column++;
					max2Digits++;
				} // while getch
				rowPos[i] = '\0';
				if (atoi(rowPos) < 30){
					startRowPos = atoi(rowPos);
					confirmNum = 1;
				} else{
					mvprintw(27, 31, "Attenzione! il campo di battaglia è limitato! max colonne e righe = 29");
					refresh();
					sleep(2);
					mvprintw(27, 27, "                                                                         ");
					refresh();
					rowPos[0] = '\0';
					max2Digits = 0;
					column = 27;
					i = 0;
					continue;
				}
			} // while confirmNum

			mvprintw(28, 20, "Colonna =  ");
			refresh();
			char colPos[3];
			i = 0;
			column = 29;
			max2Digits = 0;
			confirmNum = 0;
			while (confirmNum != 1){
				while ((k=getch()) != '\n'){
					if ((k < '0') || (k > '9')){
						mvprintw(28, 31, "Attenzione! consentito soltanto input numerico! Riprova!");
						refresh();
						sleep(2);
						mvprintw(28, 29, "                                                                      ");
						refresh();
						column = 27;
						max2Digits = 0;
						colPos[0] = '\0';
						i = 0;
						continue;
					}
					if (max2Digits >= 2){
						mvprintw(28, 31, "Attenzione! consentita scelta di massimo 2 cifre");
						refresh();
						sleep(2);
						mvprintw(28, 29, "                                                                      ");
						refresh();
						max2Digits = 0;
						column = 27;
						colPos[0] = '\0';
						i = 0;
						continue;
					}
					colPos[i] = k;
					mvprintw(28, column, "%c", colPos[i]);
					refresh();
					i++;
					column++;
					max2Digits++;
				} // while getch
				if (atoi(colPos) < 30){
					startColPos = atoi(colPos);	
					confirmNum = 1;
				} else{
					mvprintw(28, 32, "Attenzione! il campo di battaglia è limitato! max colonne e righe = 29");
					refresh();
					sleep(2);
					mvprintw(28, 29, "                                                                              ");
					refresh();
					colPos[0] = '\0';
					max2Digits = 0;
					column = 29;
					i = 0;
					continue;
				}
			} // while confirmNum

			// creazione di una stringa con tutte le informazioni dell'utente
			// <riga,column,nickname,fazione>
			char allUserInfo[200];
			int globalMap = sprintf(allUserInfo, "%d,%d;%s,%c;", startRowPos, startColPos, userInfo.username, userInfo.faction);
			write(sock_client, &allUserInfo, globalMap);

			// in attesa della risposta del server
			read(sock_client, &command, 3);
			if (atoi(&command) == 977 )		// posizione libera da "ostacoli", confermato inserimento
				confirmsPos = 1;
			else if (atoi(&command) == 978){	// posizione non libera da "ostacoli", richiesta reinvio
				confirmsPos = 0;
				confirmNum = 0;
				mvprintw(31, 20, "                                                                      ");
				mvprintw(32, 20, "                                                                      ");
				mvprintw(29, 20, "Ooops! sei stato lento.. un altro giocatore si è gia posizionato in quella casella! Scegline un'altra!");
				refresh();
			}
		} // while
	} // if prova

	clear();
	endwin();
}

/* menu di gioco */
int firstMenu(){
	char gread;
	int i = 1;	// posizione del cursore di selezione

	do {
		/* stampa del menu */
		system("clear");
		printf("Benvenuto in "GREEN_COLOR_TEXT"%s\n"RESET_COLOR_TEXT, GAMENAME);
		printf("\n\t%c Login %c \n", i == 1 ? 62 : 0, i == 1 ? 60 : 0);
		printf("\n\t%c Registrati %c \n", i == 2 ? 62 : 0, i == 2 ? 60 : 0);
		printf("\n\t%c Exit %c \n", i == 3 ? 62 : 0, i == 3 ? 60 : 0);
		printf("\n\nPremi %c %c per selezionare", 119, 115);
		printf("\nPremi INVIO per confermare");

		/* spostamento del puntatore */
		gread = getchLinux();

		if ((gread == 119)){		// spostamento del cursore in alto
			i--;
			if (i < 1)
				i = 3;	// spostamento circolare del cursore
		} else if ((gread == 115)){	// spostamento del cursore in basso
			i++;
			if (i > 3)
				i = 1;	// spostamento circolare del cursore
		}
	} while (gread != '\n');		// uscita dal ciclo

	system("clear");
	return i;		// scelta effettuata
}

/* menu di partita */
int secondMenu(){
	char gread;
	int i = 1;    // posizione del cursore di selezione

	// stampa del menu
	do {
		system("clear");
		printf("Benvenuto in "RED_COLOR_TEXT"%s <%s>\n"RESET_COLOR_TEXT, GAMENAME, RELESE);
		printf("\n\t%c Gioca nuova partita %c \n", i == 1 ? 62 : 0, i == 1 ? 60 : 0);
		printf("\n\t%c Logout %c \n", i == 2 ? 62 : 0, i == 2 ? 60 : 0);
		printf("\n\nPremi %c %c per selezionare", 119, 115);
		printf("\nPremi INVIO per confermare");

		// spostamento del puntatore
		gread = getchLinux();

		if ((gread == 119)){		// spostamento del cursore verso l'alto
			i--;
			if (i < 1)
				i = 2;	// spostamento circolare del puntatore
		} else if ((gread == 115)){	// spostamento del cursore verso il basso
			i++;
			if (i > 2)
				i = 1;	// spostamento circolare del puntatore
		}
	} while (gread != '\n');		// uscita dal ciclo

	system("clear");
	return i;		// scelta effettuata
}

/* cattura del comando <CTRL+C> per il client */
void myHandler_SIGINT(){
	endwin();
	system("clear");
	printf("<CTRL+C> catturato. Chiusura forzata dell'applicazione.\n");
	// avvisa il server della disconnessione
	shutdown(sock_client, 2);
	exit(EXIT_SUCCESS);
}

/* cattura del comando <CTRL+C> per il server */
void myHandler_SIGPIPE(){
	endwin();
	system("clear");
	printf("Errore nella comunicazione con %s. Connessione interrotta!\n", GAMENAME);
	shutdown(sock_client, 2);
	exit(EXIT_SUCCESS);
}

/* metodo di esecuzione del programma */
void printHelp(){
	system("clear");
	printf("Applicazione: "RED_COLOR_TEXT"Esecuzione incorretta.\n"RESET_COLOR_TEXT);
	printf("Applicazione: Per eseguire il client è necessario fornire la seguente specifica\n");
	printf("\t"GREEN_COLOR_TEXT"./nome_eseguibile -p numero_porta -ip indirizzo_ip\n"RESET_COLOR_TEXT);
}
