#ifndef FUN_H
#define FUN_H

/* info sui dati dei giocatori */
typedef struct DatiPlayer{
	char username[10];
	char password[10];
	char faction;
	int  healthPoint;
} User_t;

/* info utenti online (dal server) */
typedef struct OnlinePlayer_fromServer{
	char username[10];
	struct OnlinePlayer_fromServer *succ;
} OnPlayerServer_t;

struct sockaddr_in server;

/* verifica se la lista è vuota */
int isempty(OnPlayerServer_t *head);

/* inserimento di un nuovo giocatore online */
OnPlayerServer_t *addUser(OnPlayerServer_t *head, char utente[]);

/* funzione per creare una lettura bloccante */
/* getch() ha, apparentemente, una coda di input */
int readKey();

/* funzione che permette di interagire (graficamente) col terminale */
char getchLinux();
/* funzione di inizializzazione dei vari item */
void initItem();

/* funzione che permette di disegnare la mappa */
void drawsMap();

/* funzione che alloca dinamicamente la mappa */
unsigned char **allocateMap();

/* funzione per la registrazione e/o login dell'utente sul server */
int userManagement(char richiesta[]);

/* funzione di acquisizione dati della mappa */
void *mapDataAcquisition();
/* funzione per l'acquisizione dei nuovi dati della mappa */
void newMapDataAcquisition();

/* funzione per la lettura dal server della stringa <giocatori,fazione> */
void *onlinePlayerAcquisition();

/* funzione di gioco */
void runGame();

/* panoramica regole, inserimento spawn e fazione. */
void _lobby();

/* menu di gioco */
int firstMenu();
/* menu di partita */
int secondMenu();

/* cattura del comando <CTRL+C> per il client */
void myHandler_SIGINT();
/* cattura del comando <CTRL+C> per il server */
void myHandler_SIGPIPE();

/* metodo di esecuzione del programma */
void printHelp();

#endif 
