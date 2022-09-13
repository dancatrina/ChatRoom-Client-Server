/*
    SERVER - Camera online de chat - Server
    Credit : Catrina-Ilie Dan
    Versiune : 0.1b
*/
//////////////////////////////////////

/* Librarii cross-platform indiferent de sistemul de operare (WINDOWS / LINUX )  */
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

/* Macro specifice verificarii sistemului de compilare */
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)

/* Librarii specifica sistemului de operare Windows */
    #include <winsock2.h>
    #pragma comment(lib,"Ws2_32.lib")
#elif __unix__

/* Librarii specifice sistemului unix */
    #include <unistd.h>
    #include <sys/socket.h>
    #include <sys/types.h>
    #include <arpa/inet.h>
    #include <netinet/in.h>
    #include <signal.h>
    #include <pthread.h>

#else
    error "Program incopatibil cu sisteul de operare"
#endif

/*Lungimea maxima a unui Mesaj*/
#define MS_LEN 2048

/*Numarul maxim de clienti pe care serverul il accepta*/
#define MAX_CLIENTS 4

/*Portul serverului*/
#define PORT 8109

/*
 adresa -> varabila de tip [struct sockaddr_in] cu rolul de a stoca datele de conexiune;
 fileDescriptor -> variabila de tip [int] cu rolul de a stoca descriptorul conexiunii;
 id -> variabila de tip [int] cu rolul de a stoca id-ul clientului care s-a conectat
 nume -> variabila de tip [char*] reprezentand un sir de caractere cu rolul de a stoca numele clientului

 INFO : Structura are rolul de a grupa datele clientului care s-a conectat la server;
*/
struct Client {
    struct sockaddr_in adresa;
    int fileDescriptor;              
    int id;                 
    char nume[32];           
};

/*Initializare*/

/*variabila cu rolul de a stoca nr de clienti conectati al server. Folosita pentru sincronizarea threadurilor*/
static _Atomic unsigned int nrClient = {0};

/*Variabila cu scopul de a le inregistra clientilor noi un ID unic*/
static int globalID = {1};

/*Declararea unui Array de pointeri pentru Clienti*/
struct Client * arrClient[MAX_CLIENTS];

/*Initializarea unui guard pentru a asigura lucrul sincron al threadurilor*/
pthread_mutex_t mutexClient = PTHREAD_MUTEX_INITIALIZER;


/*
 client -> variabila intrare de tip [struct Client*] cu rolul de adauga un client in coada [arrClient]
 INFO : Aceasta functie are rolul de a inregistra un client nou in array;
*/
void adaugaClient(struct Client*);

/*
 idClient -> variabila  de intrare de tip [int] reprezentand id-ul clientului
 INFO: Aceasta functie are rolul de a sterge un client din array dupa id-ul precizat
*/
void removeClient(int);

/*
 s -> variabila de intrare de tip [char*] reprezentand mesajul;
 uid -> variabila de intrare de tip [uid] reprezentand id-ul clientului
 INFO: Aceasta functie are rolul de a transmite mesaje clientilor dar nu si celui care trimite mesajul
*/
void sendMesajAll(char *, int);

/*
 s -> variabila de intrare de tip [char*] reprezentand mesajul;
 INFO: Aceasta functie are rolul de a transmite un mesaj clientilor;
*/
void sendMesajAllClients(char *);

/*
 s -> variabila de intrare de tip [char*] reprezentand mesajul;
 INFO: Aceasta functie are rolul prelucra mesajul;
*/
void newLine(char *);

/*
 arg -> variabila de tip [void*] are rolul de a stoca clientul
 INFO : Aceasta functie are rolul de a manageria clientul;
*/
void *managerClienti(void *);


int main(int argc, char *argv[]){
    //Declaram descriptorul pentru Server si Client;
    int fileDescriptorS = 0, fileDescriptorCli = 0;

    //Declaram structurile specifice conexiunii;
    struct sockaddr_in serverAdress;
    struct sockaddr_in clientAdress;

    //Initializam threadul
    pthread_t threadId;

    //Creem socketul specific serverului
    fileDescriptorS = socket(AF_INET, SOCK_STREAM, 0);

    //Initializam familia IPv4
    serverAdress.sin_family = AF_INET;

    //Setam adresa IP a serverului;
    serverAdress.sin_addr.s_addr = htonl(INADDR_ANY);

    //Setam portul serverului
    serverAdress.sin_port = htons(PORT);

    //Ignoram semnalul folosit pt multihread;
    signal(SIGPIPE, SIG_IGN);

    //Asignam adresa specificata de fileDescriptor
    if (bind(fileDescriptorS, (struct sockaddr*)&serverAdress, sizeof(serverAdress)) < 0) {
        perror("Bindarea socketului a esuat");
        exit(0);
    }

    // VAcceptam noile conexiuni
    if (listen(fileDescriptorS, 10) < 0) {
        perror("Ascultarea unei noi conexiuni a esuat");
        exit(0);
    }

    printf("SERVERUL A PORNIT CU SUCCES PE PORTUL [%d] - Catrina-Ilie Dan Grupa 1081\n", PORT);

    //Acceptam clientii
    while (1) {
        //Asignam lungimea socketului; Utilizat pentru acceptarea clientilor
        socklen_t lenClient = sizeof(clientAdress);

        //Acceptam conexiunea facuta de client la server
        fileDescriptorCli = accept(fileDescriptorS, (struct sockaddr*)&clientAdress, &lenClient);

        //Verificam daca a atins numarul maxim de clienti
        if ((nrClient + 1) == MAX_CLIENTS) {
            printf("SERVER: Numarul maxim de clienti a fost atins\n");
            printf("SERVER: STATUS : REFUZAT ");
            printf("\n");
            close(fileDescriptorCli);
            continue;
        }

        //Creem un client nou
        struct Client* clientNou = (struct Client *)malloc(sizeof(struct Client));

        //Asignam adresa clientului;
        clientNou->adresa = clientAdress;

        //Asignam descriptorul prin care clientul s-a conectat la retea
        clientNou->fileDescriptor = fileDescriptorCli;

        //Asignam id-ul clientului;
        clientNou->id = globalID++;

        printf("%s",clientNou->nume);

        //Asignam numele clientului
        read(fileDescriptorCli,clientNou->nume,32);

        //Adaugam clientul in array;
        adaugaClient(clientNou);

        //Creem thread-ul, vom asigna functia de baza [managerClienti] care se va ocuap cu managerierea clientilor si vom trimite ca parametru clientNou;
        pthread_create(&threadId, NULL, &managerClienti, (void*)clientNou);

        //Optimizam threadul;
        sleep(1);
    }

    exit(0);
}


//CORP FUNCTII


/* Add client to queue */
void adaugaClient(struct Client * client){
    //Blocam executia celorlalte threaduri;
    pthread_mutex_lock(&mutexClient);
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (!arrClient[i]) {
            arrClient[i] = client;
            break;
        }
    }
    //Deblocam executia  celorlalte threaduri asupra functiei dupa executarea blocului de memorie de mai sus
    pthread_mutex_unlock(&mutexClient);
}


void removeClient(int idClient){
    //Blocam threadul
    pthread_mutex_lock(&mutexClient);
    //Stergem clientul cu id-ul = [idClient]
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (arrClient[i]) {
            if (arrClient[i]->id == idClient) {
                arrClient[i] = NULL;
                break;
            }
        }
    }
    //Deblocam threadul;
    pthread_mutex_unlock(&mutexClient);
}

void sendMesajAll(char *s, int uid){
    pthread_mutex_lock(&mutexClient);
    //Trimitem un mesaj clientilor mai putin cel care trimite
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (arrClient[i]) {
            if (arrClient[i]->id != uid) {
                //trimite mesajul;
                if (write(arrClient[i]->fileDescriptor, s, strlen(s)) < 0) {
                    perror("Write to descriptor failed");
                    break;
                }
            }
        }
    }
    //Deblocam threadul;
    pthread_mutex_unlock(&mutexClient);
}


void sendMesajAllClients(char *s){
    //Blocam thread
    pthread_mutex_lock(&mutexClient);
    //Trimitem mesaj tuturor clientilor;
    for (int i = 0; i <MAX_CLIENTS; ++i){
        if (arrClient[i]) {
            if (write(arrClient[i]->fileDescriptor, s, strlen(s)) < 0) {
                perror("Descriptorul de scriere a esuat");
                break;
            }
        }
    }
    //Deblocam threadul;
    pthread_mutex_unlock(&mutexClient);
}

//Prelucram mesajul
void newLine(char *s){
    while (*s != '\0') {
        if (*s == '\r' || *s == '\n') {
            *s = '\0';
        }
        s++;
    }
}


void *managerClienti(void *arg){
    //Initializam variabilele;
    char bufferOUT[MS_LEN];
    char bufferIN[MS_LEN / 2];
    int readLen;

    //Incrementam numarul de clienti
    nrClient++;

    //Preluam referinta clientului
    struct Client* client = (struct Client*)arg;

    //Afisam datele de conectare ale clientului
    printf("Clientul [%s] cu IP: [%d.%d.%d.%d]",
         client->nume,
         client->adresa.sin_addr.s_addr & 0xff,
        (client->adresa.sin_addr.s_addr & 0xff00) >> 8,
        (client->adresa.sin_addr.s_addr & 0xff0000) >> 16,
        (client->adresa.sin_addr.s_addr & 0xff000000) >> 24);

    printf(" cu ID [%d] s-a conectat pe server\n", client->id);

    //Anuntam clientii aflati in camera de exista noului client;
    sprintf(bufferOUT, "%s s-a alaturat camerei.\n", client->nume);

    //Trimitem mesaj clientilor;
    sendMesajAllClients(bufferOUT);

    //Preluam mesajul clientilor
    while ((readLen = read(client->fileDescriptor, bufferIN, sizeof(bufferIN) - 1)) > 0) {
        bufferIN[readLen] = '\0';
        bufferOUT[0] = '\0';
        newLine(bufferIN);

        //Ignoram mesajele goale;
        if (!strlen(bufferIN)) {
            continue;
        }
        //Copiem in bufferOutMesajul + format;
            snprintf(bufferOUT, sizeof(bufferOUT), "[%s]:%s\n", client->nume, bufferIN);
        
        //Trimitem mesaj clientilor cu exceptia cui trimite
            sendMesajAll(bufferOUT, client->id);
        }
    
    //Notificam clientii ca a parasit camera;
    sprintf(bufferOUT, "%s a parasit camera\n", client->nume);
    sendMesajAllClients(bufferOUT);
    close(client->fileDescriptor);

    //Eliminam clientul din array
    removeClient(client->id);
    printf("Clientul %s cu ID: ", client->nume);
    printf("%d a iesit din camera ", client->id);
    //Eliberam spatiul din memorie
    free(client);
    printf("[DEALOCAT DIN MEMORIE CU SUCCES]\n");
    //Decrementam valoarea atomica;
    nrClient--;
    //Distrugem threadul;
    pthread_detach(pthread_self());

    return NULL;
}
