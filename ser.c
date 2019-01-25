#include <stdio.h>		/* printf, perror, getch, fflush, sprintf, fprintf, fopen, fclose, fputs, ftell */
#include <stdlib.h>		/* rand, srand, atoi, malloc, free, exit */
#include <errno.h>		/* perror */
#include <sys/types.h>	/* socket, connect, bind, listen */
#include <sys/socket.h>	/* socket, bind, listen, inet_aton, inet_ntoa, inet_addr, connect */
#include <arpa/inet.h>	/* htons, htonl, ntohs, inet_aton, inet_ntoa, inet_addr */
#include <unistd.h>		/* close, write, read, exit, sleep, tcgetattr, getch */
#include <netinet/in.h>	/* inet_aton, inet_ntoa, inet_addr */
#include <string.h>		/* strcpy, strncpy, strlen, strcmp, strcat, strtok */
#include <time.h>		/* time, ctime */
#include <signal.h>		/* signal */
#include <pthread.h>	/* pthread_create, pthread_exit, pthread_join, pthread_mutex_lock, pthread_mutex_unlock, pthread_cond_signal */

#define GAMENAME			"CLASH UNIVERSITY"
#define GREEN_COLOR_TEXT	"\x1b[32m"
#define RED_COLOR_TEXT		"\x1b[31m"
#define RESET_COLOR_TEXT	"\x1b[0m"

/* info degli utenti registrati */
typedef struct Registered_Players{
	char userCred[1024];
	struct Registered_Players *next;
} RegPlayers_t;

/* info degli utenti online */
typedef struct Online_Players{
	char userCred[1024];
	struct Online_Players *next;
} OnPlayers_t;

/* info di connessioni dai vari client connessi */
typedef struct Connection_from{
	int connection;
	int startRowPos;
	int startColPos;
	char health[4];
	char color;
	char nickname[10];
	char *ip;
	int porta;
	struct Connection_from *next;
} Connection_t;

FILE *logFile,	// file di log
	*regFile;	// registro
time_t t;

int connIn, socketServerSide;
struct sockaddr_in client;
socklen_t s = sizeof(struct sockaddr_in);

/* Variabili globali */
/* Variabili di "Game" per mappa, giocatori, bandiere, muri, ecc */
unsigned char **globalMap;
int globalRow, globalCol;
char playerR = 'R';
char playerB = 'B';
int maxPlayers = 0;
int ready = 0;
int timeOver = 0;
int winGlobal = 0,
	winBlu = 0,
	winRed = 0;

/* parametri passati a argv */
int porta = 0,
	tempoLogin = 0,
	tempoGlobalGame = 0;

RegPlayers_t *list_UserREG = NULL;
OnPlayers_t *list_UserON = NULL;
Connection_t *list_ActiveConnections = NULL;

/* mutex */
static pthread_mutex_t mtx_credentials_fileREG = PTHREAD_MUTEX_INITIALIZER;	// scrittura sul file REG e inserimento delle credenziali
static pthread_mutex_t mtx_prontoPerGiocare = PTHREAD_MUTEX_INITIALIZER;		// per la verifica dello stato dei giocatori (se pronti a giocare o meno)
static pthread_cond_t cond_prontoPerGiocare = PTHREAD_COND_INITIALIZER;		// controllo sull'integrità dell'informazione dei giocatori pronti
static pthread_mutex_t mtx_list_connections = PTHREAD_MUTEX_INITIALIZER;		// gestione della lista delle connessioni attive
static pthread_mutex_t mtx_list_online = PTHREAD_MUTEX_INITIALIZER;			// gestione della lista degli utenti online
static pthread_mutex_t mtx_fileLOG = PTHREAD_MUTEX_INITIALIZER;				// scrittura sul file LOG
static pthread_mutex_t mtx_gameEngine = PTHREAD_MUTEX_INITIALIZER;			// gestione del gameEngine

/* funzione che serve ad avviare il server */
int startServer(int porta){
	int sock;
	struct sockaddr_in server;

	system("clear");

	// creazione della socket
	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1){
		printf("Error on socket function!\n");
		exit(EXIT_FAILURE);
	}

	// associazione dei parametri per la connessione
	server.sin_family = AF_INET;
	server.sin_port = htons(porta);
	server.sin_addr.s_addr = htonl(INADDR_ANY);

	// associazione dei parametri creati alla socket
	if (bind(sock, (struct sockaddr *)&server, sizeof(struct sockaddr_in)) == -1){
		printf("Error on bind function!\n");
		exit(EXIT_FAILURE);
	}
	
	// creazione della lista di accettazione
	listen(sock, 5);

	return sock;
}

/* funzione che serve a chiudere le socket */
void shutdownServer(){
	// SHUT_WR = ulteriori trasmissioni saranno disabilitate
	// SHUT_RWDR = ulteriori trasmissioni in ricezione e trasmissione saranno disabilitate
	while (list_ActiveConnections != NULL){
		shutdown(list_ActiveConnections->connection, SHUT_RDWR);
		list_ActiveConnections = list_ActiveConnections->next;
	}
	
	// LOGIN action - offline
	time(&t);	// prende il tempo in secondi
	pthread_mutex_lock(&mtx_fileLOG);
	logFile = fopen("LOG.txt", "a");
	fprintf(logFile, "Server <%s>: offline @ %s", GAMENAME, ctime(&t));
	printf("Server <%s>: " RED_COLOR_TEXT"offline "RESET_COLOR_TEXT "@ %s", GAMENAME, ctime(&t));
	fclose(logFile);
	pthread_mutex_unlock(&mtx_fileLOG);
	exit(EXIT_SUCCESS);
}

/* funzione per chiudere tutte le connessioni */
Connection_t *freeConnections(Connection_t *head){
	Connection_t *node = head,
		*temp;

	while (node != NULL){
		temp = node;
		node = node->next;
		free(temp);
	}
	head = NULL;
	return NULL;
}

/* funzione per fare il logout di tutti gli utenti */
OnPlayers_t *freeOnlineUsers(OnPlayers_t *head){
	OnPlayers_t *node = head,
		*temp;

	while (node != NULL){
		temp = node;
		node = node->next;
		free(temp);
	}
	head = NULL;
	return NULL;
}

/* funzione per verificare se il registro utenti è vuoto */
int isemptyReg(RegPlayers_t *head){
	if (head == NULL)
		return 1;	// vuota
	else
		return -1;	// non vuota
}

/* funzione per verificare se la lista delle connessioni è vuota */
int isemptyCon(Connection_t *head){
	if (head == NULL)
		return 1;	// vuota
	else
		return -1;	// non vuota
}

/* funzione perverificare se la lista dei players online è vuota */
int isemptyOn(OnPlayers_t *head){
	if (head == NULL)
		return 1;	// vuota
	else
		return -1;	// non vuota
}

/* funzione par la ricerca di un utente nel registo */
int searchUsersREG(RegPlayers_t *head, char user[]){
	RegPlayers_t *temp = head;

	while (temp != NULL && strcmp(temp->userCred, user) != 0)
		temp = temp->next;
	if (temp == NULL)
		return -1;	// insuccesso
	else
		return 1;	// successo
}

/* funzione per la ricerca di utenti online */
int searchUsersON(OnPlayers_t *head, char user[]){
	OnPlayers_t *temp = head;

	while (temp != NULL && strcmp(temp->userCred, user) != 0)
		temp = temp->next;
	if (temp == NULL)
		return -1;	// insuccesso
	else
		return 1;	// successo
}

/* funzione per l'inserimento di un nuovo utente online nella lista */
OnPlayers_t *addUserOnline(OnPlayers_t *head, char user[]){
	OnPlayers_t *temp = (OnPlayers_t*)malloc(sizeof(OnPlayers_t));

	strcpy(temp->userCred, user);
	temp->next = NULL;
	if (isemptyOn(head) == 1)
		head = temp;
	else{
	 	temp->next = head;
		head = temp;
	}
	return head;
}

/* funzione per l'inserimento di un nuovo utente nel registro */
RegPlayers_t *addUser(RegPlayers_t *head, char user[]){
	RegPlayers_t *temp = (RegPlayers_t*)malloc(sizeof(RegPlayers_t));

	strcpy(temp->userCred, user);
	temp->next = NULL;
	if (isemptyReg(head) == 1)
		head = temp;
	else{
	 	temp->next = head;
		head = temp;
	}
	return head;
}

/* funzione per cancellare una connessione dalla lista */
Connection_t *deleteConnection(Connection_t *head, int connection){
	if (head->connection == connection){
		Connection_t *nextUser_temp = head->next;
		free(head);
		return nextUser_temp;
	}
	head->next = deleteConnection(head->next, connection);
	return head;
}

/* funzione per inserire una nuova connessione nella lista */
Connection_t *addConnection(Connection_t *head, int connection){
	Connection_t *temp = (Connection_t*)malloc(sizeof(Connection_t));

	temp->connection = connection;
	strcpy(temp->health, "5000");
	temp->porta = ntohs(client.sin_port);
	temp->ip = inet_ntoa(client.sin_addr);
	temp->next = NULL;
	if (isemptyCon(head) == 1)
		head = temp;
	else{
	 	temp->next = head;
		head = temp;
	}
	return head;
}

/* funzione per cancellare un utente online dalla lista */
OnPlayers_t *deleteUserOnline(OnPlayers_t *head, char user[]){
	if (strcmp(head->userCred, user) == 0 ){
		OnPlayers_t *nextUser_temp = head->next;
		free(head);
		return nextUser_temp;
	}
	head->next = deleteUserOnline(head->next, user);
	return head;
}

/* funzione per l'inizializzazione della mappa di gioco */
unsigned char **generateMap(){
	int i, j;
	unsigned char **map = (unsigned char**)malloc(globalRow * sizeof(unsigned char*));

	// allocazione mappa
	for(i=0; i<globalRow; i++){
		map[i] = (unsigned char*)malloc(globalCol * sizeof(unsigned char));
		for(j=0; j<globalCol; j++){
			map[i][j] = ' ';
		}
	}
	return map;
}

/* funzione per l'inizializzazione dei vari item */
unsigned char **initItem(int mine, int wall, int medikit){
	int i;
	int rowPos, colPos;
	int blueFlagRowPos = 0,
		blueFlagColPos = 0;		// variabili di posizione bandiera blue
	int redFlagRowPos = 0,
		redFlagColPos = 0;		// variabili di posizione bandiera rossa

	// posizionamento delle mine
	for(i=0; i<mine; i++){
		// scelta casuale di una posizione (la posizione deve essere libera)
		rowPos = rand() % globalRow;
		colPos = rand() % globalCol;
		while ((globalMap[rowPos][colPos] != ' ')){
			rowPos = rand() % globalRow;
			colPos = rand() % globalCol;
		}
		globalMap[rowPos][colPos] = '1';	// mina piazzata
	}
	
	// posizionamento dei muri
	for(i=0; i<wall; i++){
		// scelta casuale di una posizione (la posizione deve essere libera)
		rowPos = rand() % globalRow;
		colPos = rand() % globalCol;
		while ((globalMap[rowPos][colPos] != ' ')){
			rowPos = rand() % globalRow;
			colPos = rand() % globalCol;
		}
		globalMap[rowPos][colPos] = '0';	// muro piazzato
	}
	
	// posizionamento dei medikit
	for(i=0; i<medikit; i++){
		// scelta casuale di una posizione (la posizione deve essere libera)
		rowPos = rand() % globalRow;
		colPos = rand() % globalCol;
		while ((globalMap[rowPos][colPos] != ' ')){
			rowPos = rand() % globalRow;
			colPos = rand() % globalCol;
		}
		globalMap[rowPos][colPos] = '2';	// medikit piazzato
	}

	// posizionamento delle bandiere
	// le bandiere si devono trovare nella rispettiva metà del campo di battaglia
	while ((blueFlagColPos = rand()%globalCol) >= globalCol/2){}
	while ((redFlagColPos = rand()%globalCol) <= globalCol/2){}

	// scelta casuale di una posizione (la posizione deve essere libera)
	blueFlagRowPos = rand() % globalRow;
	redFlagRowPos = rand() % globalRow;
	while ((globalMap[blueFlagRowPos][blueFlagColPos] != ' ') && (globalMap[redFlagRowPos][redFlagColPos] != ' ')){
		while ((blueFlagColPos = rand()%globalCol) >= globalCol/2){}
		while ((redFlagColPos = rand()%globalCol) <= globalCol/2){}
		blueFlagRowPos = rand() % globalRow;
		redFlagRowPos = rand() % globalRow;
	}
	globalMap[blueFlagRowPos][blueFlagColPos] = 'b';	// bandiera blu piazzata
	globalMap[redFlagRowPos][redFlagColPos] = 'r';		// bandiera rossa piazzata

	return globalMap;
}

/* funzione per l'acquisizione delle informazioni iniziali dei giocatori */
void *getFristDetailsPlayers(void *clientConnection){
	Connection_t *conn = (Connection_t*)clientConnection;

	//int startRowPos, cstartColPos;
	char startPos[10], app1[2], app2[2], nickname[20], color[1];
	int confermatoPosizione = 0;

	pthread_mutex_lock(&mtx_gameEngine);
	while (confermatoPosizione != 1){
		read(conn->connection, &startPos, 25);

		// divisione del messaggio ricevuto: row,column;
		int index = 0;
		while (startPos[index] != ','){
			app1[index] = startPos[index];
			index++;
		}
		index++;
		int ind = 0;
		while (startPos[index] != ';'){
			app2[ind] = startPos[index];
			ind++;
			index++;
		}
		ind = 0;
		index++;

		// divisione del messaggio ricevuto: nickname,color;
		while (startPos[index] != ','){
			nickname[ind] = startPos[index];
			ind++;
			index++;
		}
		ind = 0;
		index++;
		while (startPos[index] != ';'){
			color[ind] = startPos[index];
			ind++;
			index++;
		}

		strcpy(&conn->color, color);
		strcpy(conn->nickname, nickname);
		conn->startRowPos = atoi(app1);
		conn->startColPos = atoi(app2);

		if ((globalMap[conn->startRowPos][conn->startColPos]) == ' '){
			write(conn->connection, "977", 3);		// posizione libera da "ostacoli", confermato inserimento
			confermatoPosizione = 1;
		} else{
			write(conn->connection, "978", 3);		// posizione non libera da "ostacoli", richiesta reinvio
			confermatoPosizione = 0;
			continue;
		}
		if (conn->color == 'r'){
			globalMap[conn->startRowPos][conn->startColPos] = playerR;
		} else if (conn->color == 'b'){
			globalMap[conn->startRowPos][conn->startColPos] = playerB;
		}
	} // while
	startPos[0] = '\0';
	app1[0] = '\0';
	app2[0] = '\0';
	nickname[0] = '\0';
	color[0] = '\0';
	pthread_mutex_unlock(&mtx_gameEngine);

	pthread_exit(NULL);
}

/* funzione per l'invio ai client dei parametri della mappa */
void sendMap(Connection_t *con){
	while (con != NULL){
		char dim[] = {"30"};	// dimensione della mappa
		write(con->connection, &dim, 2);	// invio mappa
		write(con->connection, "5000", 4);	// invio vita

		int i, j;
		for(i=0; i<globalRow; i++){
			for(j=0; j<globalCol; j++){
				write(con->connection, &globalMap[i][j], sizeof(char));	// invio items e muri
			}
		}
		con = con->next;
	}
}

/* funzione per l'aggiornamento della mappa inviata ai client */
void sendNewMap(Connection_t *con){
	while (con != NULL){
		write(con->connection, con->health, 4);	// invio vita

		int i, j;
		for(i=0; i<globalRow; i++){
			for(j=0; j<globalCol; j++){
				write(con->connection, &globalMap[i][j], sizeof(char));	// invio items e muri
			}
		}
		con = con->next;
	}
	con = list_ActiveConnections;
}

/* funzione per la gestione della connessione */
void *inConnManager(void *clientConnection){
	Connection_t* conn = (Connection_t*)clientConnection;

	char command;
	char userInfo[1024];
	int nread;

	do{
		read(conn->connection, &command, 3);
		switch(atoi(&command)){
			case 200:	// richiesta registrazione
				// LOGIN action - registrazione
				pthread_mutex_lock(&mtx_fileLOG);
				logFile = fopen("LOG.txt", "a");
				fprintf(logFile, "Server <LOG>: Signup request (%s : %d) @ %s", conn->ip, conn->porta, ctime(&t));
				fclose(logFile);
				pthread_mutex_unlock(&mtx_fileLOG);

				// registrazione del nuovo utente in un file nel formato
				// NomeUtente,PAsword;
				pthread_mutex_lock(&mtx_credentials_fileREG);
				regFile = fopen("REG.txt", "a+");
				nread = read(conn->connection, &userInfo, 1024);
				userInfo[nread] = '\0';
				fputs(userInfo, regFile);
				fclose(regFile);				
				list_UserREG = addUser(list_UserREG, userInfo);	// inserimento nella lista degli utenti registrati
				pthread_mutex_unlock(&mtx_credentials_fileREG);

				write(conn->connection, "SUCC\0", 5);	// risposta del server al client di avvenuta registrazione
				break;

			case 201:	// richiesta login
				// LOGIN action - login
				pthread_mutex_lock(&mtx_fileLOG);
				time(&t);
				logFile = fopen("LOG.txt", "a");
				fprintf(logFile, "Server <LOG>: Login request (%s : %d) @ %s", conn->ip, conn->porta, ctime(&t));
				fclose(logFile);
				pthread_mutex_unlock(&mtx_fileLOG);

				read(conn->connection, &userInfo, 1024);
				if ((searchUsersREG(list_UserREG, userInfo)) == 1){
					if ((searchUsersON(list_UserON, userInfo) == -1) ){
						pthread_mutex_lock(&mtx_list_online);
						list_UserON = addUserOnline(list_UserON, userInfo);	// inserimento dell'utente loggato in una lista di utenti online
						pthread_mutex_unlock(&mtx_list_online);

						write(conn->connection, "665", 3);		//utente appena loggato e salvato nella lista
					} else
						write(conn->connection, "666", 3);		// utente già online
				} else
					write(conn->connection, "667", 3);			// utente non registrato

				break;

			case 202:	// client pronto per giocare
				// LOGIN action - pronto per giocare
				pthread_mutex_lock(&mtx_fileLOG);
				time(&t);
				logFile = fopen("LOG.txt", "a");
				fprintf(logFile, "Server <LOG>: ready to play (%s : %d) @ %s", conn->ip, conn->porta, ctime(&t));
				fclose(logFile);
				pthread_mutex_unlock(&mtx_fileLOG);
				pthread_mutex_lock(&mtx_prontoPerGiocare);
				ready++;
				if (ready == maxPlayers)
					pthread_cond_signal(&cond_prontoPerGiocare);
				pthread_mutex_unlock(&mtx_prontoPerGiocare);

				break;

			case 203:	// logout
				// LOGIN action - logout
				pthread_mutex_lock(&mtx_fileLOG);
				time(&t);
				logFile = fopen("LOG.txt", "a");
				fprintf(logFile, "Server <LOG>: Client logout (%s : %d) @ %s", conn->ip, conn->porta, ctime(&t));
				fclose(logFile);
				pthread_mutex_unlock(&mtx_fileLOG);

				// cancellazione dell'utente dalla lista online
				pthread_mutex_lock(&mtx_list_online);
				list_UserON = deleteUserOnline(list_UserON, userInfo);
				pthread_mutex_unlock(&mtx_list_online);

				break;

			case 000:	// exit
				// LOGIN action - exit
				pthread_mutex_lock(&mtx_fileLOG);
				time(&t);
				logFile = fopen("LOG.txt", "a");
				fprintf(logFile, "Server <LOG>: Client logout (%s : %d) @ %s", conn->ip, conn->porta, ctime(&t));
				fclose(logFile);
				pthread_mutex_unlock(&mtx_fileLOG);

				// cancellazione dalla lista del nodo_connessione e
				// cancellazione dell'utente dalla lista online
				pthread_mutex_lock(&mtx_list_connections);
				list_ActiveConnections = deleteConnection(list_ActiveConnections, conn->connection);
				pthread_mutex_unlock(&mtx_list_connections);

				if (list_UserON != NULL){
					pthread_mutex_lock(&mtx_list_online);
					list_UserON = deleteUserOnline(list_UserON, userInfo);
					pthread_mutex_unlock(&mtx_list_online);
				}
				maxPlayers--;

				// se le connessioni sono 0, si riaprono le iscrizioni
				if (list_ActiveConnections == NULL){
					pthread_mutex_lock(&mtx_prontoPerGiocare);
					ready = maxPlayers;	// condizione di uscita dalla condition variable
					pthread_cond_signal(&cond_prontoPerGiocare);
					pthread_mutex_unlock(&mtx_prontoPerGiocare);
				}

				pthread_exit(NULL);
				break;
		}
	} while ((atoi(&command)) != 202 );

	pthread_exit(NULL);
}

/* funzione per visualizzare i giocatori online */
void sendAllPlayerInGame(Connection_t *conn){
	char command[200];
	Connection_t *global = list_ActiveConnections;
	char playerInfo[200];
	int partial = 0,
		total = 0;

	command[0] = '\0';
	playerInfo[0] = '\0';

	// creazione di una stringa dalla lista
	// color,nickname;
	while (global != NULL){
		partial = sprintf(command, "%c,%s;", global->color, global->nickname);
		//partial = snprintf(command, sum, "%c,%s;" ,global->color, global->nickname);
		strcat(playerInfo, command);
		total+=partial;
		global = global->next;
		command[0] = '\0';
	}
	playerInfo[total] = '\0';
	//write(STDOUT_FILENO, &playerInfo, total);

	while (conn != NULL){
		write(conn->connection, &playerInfo, strlen(playerInfo));
		conn = conn->next;
	}
}

/* funzione per il tempo di login */
void *startTimeLogin(){
	sleep(tempoLogin);
	pthread_exit(NULL);
}

/* funzione per l'accettazione dei giocatori */
void *playersAcceptance(){
	while (1){
		connIn = accept(socketServerSide, (struct sockaddr*)&client, &s);
		time(&t);
		// LOGIN action - nuova connessione
		pthread_mutex_lock(&mtx_fileLOG);
		logFile = fopen("LOG.txt", "a");
		fprintf(logFile, "Server <LOG>: New connection (%s : %d) @ %s", inet_ntoa(client.sin_addr), ntohs(client.sin_port), ctime(&t));
		fclose(logFile);
		pthread_mutex_unlock(&mtx_fileLOG);

		pthread_mutex_lock(&mtx_list_connections);
		maxPlayers++;
		list_ActiveConnections = addConnection(list_ActiveConnections, connIn);
		pthread_mutex_unlock(&mtx_list_connections);

		pthread_t inConnManager_th;
		pthread_create(&inConnManager_th, NULL, inConnManager, list_ActiveConnections);
	}
}

/* funzione per far iniziare il tempo di gioco */
void *startGlobalGameTime(){
	int i;
	for(i=0; i<=tempoGlobalGame; i++){
		sleep(1);
	}
	timeOver = 1;

	pthread_exit(NULL);
}

/* funzione per la gestione degli spostamenti, della vita... */
void *gameEngine(void *clientConnection){
	Connection_t *conn = (Connection_t*)clientConnection;

	int minedFootsteps = 0,
		hitWall = 0,
		usedMedikit = 0;
	int row = conn->startRowPos;
	int column = conn->startColPos;
	char command;

	pthread_mutex_lock(&mtx_gameEngine);
	read(conn->connection, &command, 1);
	if (atoi(conn->health) > 0){
		if ((globalMap[row][column]) == 'R' ){
			switch((atoi(&command))){
				case 1:	// sopra
					row = row - 1;
					if (row < 0){		// check del bordo
						hitWall = 1;
						row = 0;
						globalMap[row][column] = playerR;
						break;
					} else if ((globalMap[row][column]) == '0'){		// check del muro
						hitWall = 1;
						row++;
						globalMap[row][column] = playerR;
						break;
					} else if ((globalMap[row][column]) == '1'){		// check della mina
						minedFootsteps = 1;
						globalMap[row+1][column] = ' ';
						globalMap[row][column] = playerR;
						break;
					} else if ((globalMap[row][column]) == '2'){		// check del medikit
						usedMedikit = 1;
						globalMap[row+1][column] = ' ';
						globalMap[row][column] = playerR;
						break;
					} else if (((globalMap[row][column]) == 'b')){	// check bandiera
						globalMap[row+1][column] = ' ';
						globalMap[row][column] = playerR;
						winRed = 1;
						break;
					} else if (((globalMap[row][column]) == 'r')){	// check bandiera alleata diventa muro
						hitWall = 1;
						row++;
						globalMap[row][column] = playerR;
						break;
					} else if (((globalMap[row][column]) == 'R')){	// check alleato
						hitWall = 1;
						row++;
						globalMap[row][column] = playerR;
						break;
					} else if (((globalMap[row][column]) == 'B')){	// check contatto nemico
						hitWall = 1;
						row++;
						globalMap[row][column] = playerR;
						break;
					}
					globalMap[row+1][column] = ' ';
					globalMap[row][column] = 'R';
					break;

				case 2:	// sotto
					row = row + 1;
					if (row >= globalRow){
						hitWall = 1;
						row = globalRow - 1;
						globalMap[row][column] = playerR;
						break;
					} else if ((globalMap[row][column]) == '0'){
						hitWall = 1;
						row--;
						globalMap[row][column] = playerR;
						break;
					} else if ((globalMap[row][column]) == '1'){
						minedFootsteps = 1;
						globalMap[row-1][column] = ' ';
						globalMap[row][column] = playerR;
						break;
					} else if ((globalMap[row][column]) == '2'){
						usedMedikit = 1;
						globalMap[row-1][column] = ' ';
						globalMap[row][column] = playerR;
						break;
					} else if (((globalMap[row][column]) == 'b')){
						globalMap[row-1][column] = ' ';
						globalMap[row][column] = playerR;
						winRed = 1;
						break;
					} else if (((globalMap[row][column]) == 'r')){
						hitWall = 1;
						row--;
						globalMap[row][column] = playerR;
						break;
					} else if (((globalMap[row][column]) == 'R')){
						hitWall = 1;
						row--;
						globalMap[row][column] = playerR;
						break;
					} else if (((globalMap[row][column]) == 'B')){
						hitWall = 1;
						row--;
						globalMap[row][column] = playerR;
						break;
					}
					globalMap[row-1][column] = ' ';
					globalMap[row][column] = 'R';
					break;

				case 3:	// sinistra
					column = column - 1;
					if (column < 0){
						hitWall = 1;
						column = 0;
						globalMap[row][column] = playerR;
						break;
					} else if ((globalMap[row][column]) == '0'){
						hitWall = 1;
						column++; globalMap[row][column] = playerR;
						break;
					} else if ((globalMap[row][column]) == '1'){
						minedFootsteps = 1;
						globalMap[row][column+1] = ' ';
						globalMap[row][column] = playerR;
						break;
					} else if ((globalMap[row][column]) == '2'){
						usedMedikit = 1;
						globalMap[row][column+1] = ' ';
						globalMap[row][column] = playerR;
						break;
					} else if (((globalMap[row][column]) == 'b')){
						globalMap[row][column+1] = ' ';
						globalMap[row][column] = playerR;
						winRed = 1;
						break;
					} else if (((globalMap[row][column]) == 'r')){
						hitWall = 1;
						column++;
						globalMap[row][column] = playerR;
						break;
					} else if (((globalMap[row][column]) == 'R')){
						hitWall = 1;
						column++;
						globalMap[row][column] = playerR;
						break;
					} else if (((globalMap[row][column]) == 'B')){
						hitWall = 1;
						column++;
						globalMap[row][column] = playerR;
						break;
					}
					globalMap[row][column+1] = ' ';
					globalMap[row][column] = 'R';
					break;

				case 4:	// destra
					column = column + 1;
					if (column >= globalRow){
						hitWall = 1;
						column = globalCol - 1;
						globalMap[row][column] = playerR;
						break;
					} else if ((globalMap[row][column]) == '0'){
						hitWall = 1;
						column--;
						globalMap[row][column] = playerR;
						break;
					} else if ((globalMap[row][column]) == '1'){
						minedFootsteps = 1;
						globalMap[row][column-1] = ' ';
						globalMap[row][column] = playerR;
						break;
					} else if ((globalMap[row][column]) == '2'){
						usedMedikit = 1;
						globalMap[row][column-1] = ' ';
						globalMap[row][column] = playerR;
						break;
					} else if (((globalMap[row][column]) == 'b')){
						globalMap[row][column-1] = ' ';
						globalMap[row][column] = playerR;
						winRed = 1;
						break;
					} else if (((globalMap[row][column]) == 'r')){
						hitWall = 1;
						column--;
						globalMap[row][column] = playerR;
						break;
					} else if (((globalMap[row][column]) == 'R')){
						hitWall = 1;
						column--;
						globalMap[row][column] = playerR;
						break;
					} else if (((globalMap[row][column]) == 'B')){
						hitWall = 1;
						column--;
						globalMap[row][column] = playerR;
						break;
					}
					globalMap[row][column-1] = ' ';
					globalMap[row][column] = 'R';
					break;
			} // switch
		} else if ((globalMap[row][column]) == 'B'){
			switch((atoi(&command))){
				case 0:	// player morto
					// LOGIN action - giocatore morto
					time(&t);
					pthread_mutex_lock(&mtx_fileLOG);
					logFile = fopen("LOG.txt", "a");
					fprintf(logFile, "Server <LOG>: Dead player (%s : %d) @ %s", conn->ip, conn->porta, ctime(&t));
					fclose(logFile);
					pthread_mutex_unlock(&mtx_fileLOG);
					pthread_exit(NULL);

				case 1: // sopra
					row = row - 1;
					if (row < 0){		// check del bordo
						hitWall = 1;
						row = 0;
						globalMap[row][column] = playerB;
						break;
					} else if ((globalMap[row][column]) == '0'){		// check del muro
						hitWall = 1;
						row++;
						globalMap[row][column] = playerB;
						break;
					} else if ((globalMap[row][column]) == '1'){		// check della mina
						minedFootsteps = 1;
						globalMap[row+1][column] = ' ';
						globalMap[row][column] = playerB;
						break;
					} else if ((globalMap[row][column]) == '2'){		// check del medikit
						usedMedikit = 1;
						globalMap[row+1][column] = ' ';
						globalMap[row][column] = playerB;
						break;
					} else if (((globalMap[row][column]) == 'r')){	// check bandiera
						globalMap[row+1][column] = ' ';
						globalMap[row][column] = playerB;
						winBlu = 1;
						break;
					} else if (((globalMap[row][column]) == 'b')){	// check bandiera alleata diventa muro
						hitWall = 1;
						row++;
						globalMap[row][column] = playerB;
						break;
					} else if (((globalMap[row][column]) == 'B')){	// check alleato
						hitWall = 1;
						row++;
						globalMap[row][column] = playerB;
						break;
					} else if (((globalMap[row][column]) == 'R')){	// check contatto nemico
						hitWall = 1;
						row++;
						globalMap[row][column] = playerB;
						break;
					}
					globalMap[row+1][column] = ' ';
					globalMap[row][column] = 'B';
					break;

				case 2:	// sotto
					row = row + 1;
					if (row >= globalRow){
						hitWall = 1;
						row = globalRow - 1;
						globalMap[row][column] = playerB;
						break;
					} else if ((globalMap[row][column]) == '0'){
						hitWall = 1;
						row--;
						globalMap[row][column] = playerB;
						break;
					} else if ((globalMap[row][column]) == '1'){
						minedFootsteps = 1;
						globalMap[row-1][column] = ' ';
						globalMap[row][column] = playerB;
						break;
					} else if ((globalMap[row][column]) == '2'){
						usedMedikit = 1;
						globalMap[row-1][column] = ' ';
						globalMap[row][column] = playerB;
						break;
					} else if (((globalMap[row][column]) == 'r')){
						globalMap[row-1][column] = ' ';
						globalMap[row][column] = playerB;
						winBlu = 1;
						break;
					} else if (((globalMap[row][column]) == 'b')){
						hitWall = 1;
						row--;
						globalMap[row][column] = playerB;
						break;
					} else if (((globalMap[row][column]) == 'B')){
						hitWall = 1;
						row--;
						globalMap[row][column] = playerB;
						break;
					} else if (((globalMap[row][column]) == 'R')){
						hitWall = 1;
						row--;
						globalMap[row][column] = playerB;
						break;
					}
					globalMap[row-1][column] = ' ';
					globalMap[row][column] = 'B';
					break;

				case 3:	// sinistra
					column = column - 1;
					if (column < 0){
						hitWall = 1;
						column = 0;
						globalMap[row][column] = playerB;
						break;
					} else if ((globalMap[row][column]) == '0'){
						hitWall = 1;
						column++;
						globalMap[row][column] = playerB;
						break;
					} else if ((globalMap[row][column]) == '1'){
						minedFootsteps = 1;
						globalMap[row][column+1] = ' ';
						globalMap[row][column] = playerB;
						break;
					} else if ((globalMap[row][column]) == '2'){
						usedMedikit = 1;
						globalMap[row][column+1] = ' ';
						globalMap[row][column] = playerB;
						break;
					} else if (((globalMap[row][column]) == 'r')){
						globalMap[row][column+1] = ' ';
						globalMap[row][column] = playerB;
						winBlu = 1;
						break;
					} else if (((globalMap[row][column]) == 'b')){
						hitWall = 1;
						column++;
						globalMap[row][column] = playerB;
						break;
					} else if (((globalMap[row][column]) == 'B')){
						hitWall = 1;
						column++;
						globalMap[row][column] = playerB;
						break;
					} else if (((globalMap[row][column]) == 'R')){
						hitWall = 1;
						column++;
						globalMap[row][column] = playerB;
						break;
					}
					globalMap[row][column+1] = ' ';
					globalMap[row][column] = 'B';
					break;

				case 4:	// destra
					column = column + 1;
					if (column >= globalRow){
						hitWall = 1;
						column = globalCol - 1;
						globalMap[row][column] = playerB;
						break;
					} else if ((globalMap[row][column]) == '0'){
						hitWall = 1;
						column--;
						globalMap[row][column] = playerB;
						break;
					} else if ((globalMap[row][column]) == '1'){
						minedFootsteps = 1;
						globalMap[row][column-1] = ' ';
						globalMap[row][column] = playerB;
						break;
					} else if ((globalMap[row][column]) == '2'){
						usedMedikit = 1;
						globalMap[row][column-1] = ' ';
						globalMap[row][column] = playerB;
						break;
					} else if (((globalMap[row][column]) == 'r')){
						globalMap[row][column-1] = ' ';
						globalMap[row][column] = playerB;
						winBlu = 1;
						break;
					} else if (((globalMap[row][column]) == 'b')){
						hitWall = 1;
						column--;
						globalMap[row][column] = playerB;
						break;
					} else if (((globalMap[row][column]) == 'B')){
						hitWall = 1;
						column--;
						globalMap[row][column] = playerB;
						break;
					} else if (((globalMap[row][column]) == 'R')){
						hitWall = 1;
						column--;
						globalMap[row][column] = playerB;
						break;
					}
					globalMap[row][column-1] = ' ';
					globalMap[row][column] ='B';
					break;
			}//switch
		} else{
			row = conn->startRowPos;
			column = conn->startColPos;
		}

	} else{	//fine if su healt
		row = conn->startRowPos;
		column = conn->startColPos;
	}

	int provLife = atoi(conn->health);

	if (hitWall != 1){
		// esplosione mina
		if (minedFootsteps == 1){
			if ((provLife - 600) < 0)
				provLife = 0;
			else if ((provLife - 600) >= 0)
				provLife-=600;
			minedFootsteps = 0;
		}

		// utilizzo del medikit
		if (usedMedikit == 1){
			if ((provLife + 300) >= 5000)
				provLife = 5001;
			else
				provLife+=300;
			usedMedikit = 0;
		}

		// passo
		if ((provLife) > 0)
			provLife = provLife - 1;

		// setting della vittoria globale
		if (winRed==1)
			winGlobal = 1;
		else if (winBlu == 1)
			winGlobal = 1;
	} else
		hitWall = 0;

	char vitaInChar[4];
	sprintf(vitaInChar, "%d", provLife);
	strcpy(conn->health, vitaInChar);
	conn->startRowPos = row;
	conn->startColPos = column;
	pthread_mutex_unlock(&mtx_gameEngine);

	pthread_exit(NULL);
}

/* funzione per la gestione del gioco e dei thread */
void *gameManager(){
	Connection_t *list_Users_prov = list_ActiveConnections;
	void *status;
	int i;

	do{
		// creazione del thread per gestire/controllare il movimento dei giocatori
		pthread_t gameEngine_th[maxPlayers];
		for(i=0; i<maxPlayers; i++){
			pthread_create(&gameEngine_th[i], NULL, gameEngine, list_Users_prov);
			list_Users_prov = list_Users_prov->next;
		}
		for(i=0; i<maxPlayers; i++){
			pthread_join(gameEngine_th[i], &status);
		}

		// aggiornamento della mappa dei giocatori
		// (soltanto quando tutti hanno effettuato la loro mossa, non prima)
		list_Users_prov = list_ActiveConnections;
		while (list_Users_prov != NULL){
			write(list_Users_prov->connection, "988", 3);
			list_Users_prov = list_Users_prov->next;
		}
		sendNewMap(list_ActiveConnections);

		list_Users_prov = list_ActiveConnections;
	} while ((timeOver != 1) && (winGlobal != 1));

	// fine della fase di gioco
	// timeover oppure bandiera presa
	list_Users_prov = list_ActiveConnections;
	if (timeOver == 1){		// notifica pareggio
		while (list_Users_prov != NULL){
			write(list_Users_prov->connection, "987", 3);
			list_Users_prov = list_Users_prov->next;
		}
		// LOGIN action - pareggio
		time(&t);
		pthread_mutex_lock(&mtx_fileLOG);
		logFile = fopen("LOG.txt", "a");
		fprintf(logFile, "Server <%s>: Time Over - Draw @ %s", GAMENAME, ctime(&t));
		fclose(logFile);
		pthread_mutex_unlock(&mtx_fileLOG);
	}
	if (winGlobal == 1){	// notifica vittoria
		// vittoria squadra blu
		if (winBlu == 1){
			while (list_Users_prov != NULL){
				write(list_Users_prov->connection, "986", 3);
				list_Users_prov = list_Users_prov->next;
			}
			// LOGIN action - vittoria blu
			time(&t);
			pthread_mutex_lock(&mtx_fileLOG);
			logFile = fopen("LOG.txt", "a");
			fprintf(logFile, "Server <%s>: Blue Won @ %s", GAMENAME, ctime(&t));
			fclose(logFile);
			pthread_mutex_unlock(&mtx_fileLOG);
		// vittoria squadra rossa
		} else if (winRed == 1){
			while (list_Users_prov != NULL){
				write(list_Users_prov->connection, "985", 3);
				list_Users_prov = list_Users_prov->next;
			}
			// LOGIN action - vittoria rossi
			time(&t);
			pthread_mutex_lock(&mtx_fileLOG);
			logFile = fopen("LOG.txt", "a");
			fprintf(logFile, "Server <%s>: Red Won @ %s", GAMENAME, ctime(&t));
			fclose(logFile);
			pthread_mutex_unlock(&mtx_fileLOG);
		}
	}
	// LOGIN action - fine game
	time(&t);
	pthread_mutex_lock(&mtx_fileLOG);
	logFile = fopen("LOG.txt", "a");
	fprintf(logFile, "Server <%s>: Game Over @ %s", GAMENAME, ctime(&t));
	fclose(logFile);
	pthread_mutex_unlock(&mtx_fileLOG);

	// fine della gestione di gioco
	pthread_exit(NULL);
}

/* funzione help */
void printHelp(){
	system("clear");
	printf("Server: " RED_COLOR_TEXT"Esecuzione incorretta.\n"RESET_COLOR_TEXT);
	printf("Server: eseguire il server con le seguenti specifiche:\n");
	printf("\t"GREEN_COLOR_TEXT"./nome_eseguibile -p porta -tc tempo_connessione -tg tempo_gioco\n"RESET_COLOR_TEXT);
}

/* inserimento nella lista di registro tutti gli utenti del file REG.txt */
RegPlayers_t* readRegisteredPlayers(){
	RegPlayers_t *myList = NULL;

	pthread_mutex_lock(&mtx_credentials_fileREG);
	FILE *f = fopen("REG.txt", "rb");
	if (f == NULL){
		fclose(fopen("REG.txt", "w"));
		f = fopen("REG.txt", "rb");
	}
	fseek(f, 0, SEEK_END);
	long fsize = ftell(f);
	fseek(f, 0, SEEK_SET);
	char *string = malloc(fsize + 1);
	fread(string, fsize, 1, f);
	string[fsize] = 0;
	fclose(f);
	char* token = strtok(string, ";");
	while (token != NULL){
		int s = strlen(token);
		char *credenziali = malloc(s+2);
		strcpy(credenziali, token);
		credenziali[s] = ';';
		credenziali[s+1] = 0;
		myList = addUser(myList, credenziali);
		token = strtok(NULL, ";");
	}
	pthread_mutex_unlock(&mtx_credentials_fileREG);
	return myList;
}

/* funzione main */
int main(int argc, char** argv){
	srand(time(NULL));
	globalRow = 30;
	globalCol = 30;

	// SIGINT - Interruzione del processo.
	signal(SIGINT, shutdownServer);
	
	// SIGPIPE - Se un processo che dovrebbe leggere da una pipe
	// termina inaspettatamente, questo segnale viene inviato al
	// programma che dovrebbe scrivere sulla pipe in questione.
	signal(SIGPIPE, SIG_IGN);

	// argv[1] = porta
	// argv[3] = tempo di Login
	// argv[5] = tempo di gioco
	if (argc != 7){
		printHelp();
		exit(EXIT_FAILURE);
	} else{
		if (strcmp((argv[1]), "-p") == 0)
			porta = atoi(argv[2]);
		else{
			printHelp();
			exit(EXIT_FAILURE);
		}
		if (strcmp((argv[3]), "-tc") == 0)
			tempoLogin = atoi(argv[4]);
		else{
			printHelp();
			exit(EXIT_FAILURE);
		}
		if (strcmp((argv[5]), "-tg") == 0)
			tempoGlobalGame = atoi(argv[6]);
		else{
			printHelp();
			exit(EXIT_FAILURE);
		}
	}

	// start server
	if ((socketServerSide = startServer(porta)) == -1)
	   printf("Server <%s>: " RED_COLOR_TEXT"offline\n"RESET_COLOR_TEXT, GAMENAME);
	else{
		// LOGING action - server online
		time(&t);
		pthread_mutex_lock(&mtx_fileLOG);
		logFile = fopen("LOG.txt", "a");
		fprintf(logFile, "Server <%s>: online @ %s", GAMENAME, ctime(&t));
		printf("Server <%s>: " GREEN_COLOR_TEXT"online "RESET_COLOR_TEXT "@ %s", GAMENAME, ctime(&t));
		fclose(logFile);
		pthread_mutex_unlock(&mtx_fileLOG);
	}
	// creazione della mappa di gioco;
	globalMap = generateMap(globalRow, globalCol);
	// caricamento della lista con gli utenti registrati nella partite precedenti
	list_UserREG = readRegisteredPlayers();

	// LOGIN action - inizio iscrizioni
	time(&t);
	pthread_mutex_lock(&mtx_fileLOG);
	logFile = fopen("LOG.txt", "a");
	fprintf(logFile, "Server <%s>: Open inscriptions @ %s", GAMENAME, ctime(&t));
	fclose(logFile);
	pthread_mutex_unlock(&mtx_fileLOG);

	while (1){
		pthread_t startTimeLogin_th;
		pthread_create(&startTimeLogin_th, NULL, startTimeLogin, NULL);
		pthread_t acceptancePlayers_th;
		pthread_create(&acceptancePlayers_th, NULL, playersAcceptance, NULL);
		pthread_join(startTimeLogin_th, NULL);
		pthread_cancel(acceptancePlayers_th);

		// riapertura delle iscrizioni in caso di nessun giocatore
		if (list_ActiveConnections == NULL){
			//printf(RED_COLOR_TEXT"Server <%s>"RESET_COLOR_TEXT": Nessuna iscrizione\n"RESET_COLOR_TEXT, GAMENAME);
			continue;
		}

		// tempo di iscizione scaduto con almeno un giocatore
		// LOGIN action - iscrizioni chiuse
		time(&t);
		pthread_mutex_lock(&mtx_fileLOG);
		logFile = fopen("LOG.txt", "a");
		fprintf(logFile, "Server <%s>: Closed inscriptions @ %s", GAMENAME, ctime(&t));
		fclose(logFile);
		pthread_mutex_unlock(&mtx_fileLOG);

		// momento di attesa che client siano pronti dando il comando "ready"
		pthread_mutex_lock(&mtx_prontoPerGiocare);
		while (ready != maxPlayers){
			pthread_cond_wait(&cond_prontoPerGiocare, &mtx_prontoPerGiocare);
		}
		pthread_mutex_unlock(&mtx_prontoPerGiocare);

		// riapertura delle iscrizioni nel caso in cui l'unico utente
		// iscritto si disconnetta
		if (list_ActiveConnections == NULL){
			printf("Server <%s>: " GREEN_COLOR_TEXT"Nessuna iscrizione, ricontrollo.\n"RESET_COLOR_TEXT, GAMENAME);
			continue;
		}

		// fase di preparazione
		Connection_t *temp = list_ActiveConnections;
		// creazione del thread per lìacquisizione dettagli del giocatore
		// row,column;Nome,Fazione;
		pthread_t getInfoPlayers_th[maxPlayers];

		int i;
		for(i=0; i<maxPlayers; i++){
			pthread_create(&getInfoPlayers_th[i], NULL, getFristDetailsPlayers, temp);
			temp = temp->next;
		}

		// inserimento e invio dei dati del client
		temp = list_ActiveConnections;
		while (temp != NULL){
			write(temp->connection, "555", 3);
			temp = temp->next;
		}

		for(i=0; i<maxPlayers; i++)
			pthread_join(getInfoPlayers_th[i], NULL);

		// setting degli item
		globalMap = initItem(10, 10, 10);

		// invio della lista playerOnline
		sendAllPlayerInGame(list_ActiveConnections);
		// "stretta di mano" per l'avvenuta transazione
		Connection_t *head = list_ActiveConnections;
		while (head != NULL){
			char risposta[3];
			read(head->connection, &risposta, 3);
			if ((atoi(risposta) == 204))
				write(head->connection, "777", 3);
			head = head->next;
		}

		// invio della mappa iniziale
		sendMap(list_ActiveConnections);

		// inizio del timer di gioco
		pthread_t startGlobalGameTime_th;
		pthread_create(&startGlobalGameTime_th, NULL, startGlobalGameTime, NULL);

		// LOGIN action - inizio game
		time(&t);
		pthread_mutex_lock(&mtx_fileLOG);
		logFile = fopen("LOG.txt", "a");
		fprintf(logFile, "Server <%s>: Start game @ %s", GAMENAME, ctime(&t));
		fclose(logFile);
		pthread_mutex_unlock(&mtx_fileLOG);

		// fase di game
		pthread_t gameManager_th;
		pthread_create(&gameManager_th, NULL, gameManager, NULL);
		pthread_join(gameManager_th, NULL);

		// reset per nuova partita
		ready =0 ;
		maxPlayers = 0;
		list_UserON = freeOnlineUsers(list_UserON);
		list_ActiveConnections = freeConnections(list_ActiveConnections);
		free(globalMap);
		globalMap = generateMap();
		tempoLogin = atoi(argv[4]);
		tempoGlobalGame = atoi(argv[6]);
		timeOver = 0;
		winGlobal = 0;
		winBlu = 0;
		winRed =0;

		// LOGIN action - inizio accettazione
		time(&t);
		pthread_mutex_lock(&mtx_fileLOG);
		logFile = fopen("LOG.txt", "a");
		fprintf(logFile, "Server <%s>: Open inscriptions @ %s", GAMENAME, ctime(&t));
		fclose(logFile);
		pthread_mutex_unlock(&mtx_fileLOG);
	}

	return 0;
}
