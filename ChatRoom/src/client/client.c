/*
    SERVER - Camera online de chat - Client
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

/* Librarii specifice sistemului unix */
#elif __unix__     
    #include <unistd.h> 
    #include <sys/socket.h>
    #include <sys/types.h>
    #include <arpa/inet.h>
    #include <netinet/in.h>
    #include <signal.h>
    #include <pthread.h>

#else
    /*Mesaj de eroare*/
    error "Program incopatibil cu sisteul de operare"
#endif

/*
 MAX_CAR_CLIENT -> numele clientului , reprezinta numarul maxim de caractere pe care il poate detine; 
*/
#define MAX_CAR_CLIENT 32

/*
 MAX_MSG_LEN -> lungimea maxima a mesajului pe care clientul poate sa o trimita la server
*/
#define MAX_MSG_LEN 2048


/*
 Structura Client:
    -> adresaIp -> sir de caractere reprezentand adresa ip;
    -> nume -> sir de caractere reprezentand numele clientului;
    -> socketFileDescriptor -> variabila folosita pentru deschiderea socketului, valoarea introasa este un numar reprezentand descriptorul fisierului
                               in caz contrar -1 (crearea socketului nu a fost posibila);
    -> port -> numar intreg, reprezentand portul in care se face conexiunea
*/
struct Client {
    char* adresaIp;
    char* nume;

    int socketFileDescriptor;
    int port;
};


//////////////*INITIALIZARI *////////////////

/*
 cl -> variabila de tipul [struct Client] cu rolul de a stoca informatiile clientului
*/
struct Client* cl = NULL;

/*
 flagIesire -> variabila de tip [sig_atomic_t], folosita pentru a trimite un semnal asincron threadurilor atunci cand actiunea a fost intrerupta;
*/
volatile sig_atomic_t flagIesire = 0;


//////////////* Functii *////////////////

/*
 arg -> variabila de intrare de tip [char*] reprezinta ip si portul serverului
 numeClient -> variabila de intrare de tip [char*] reprezinta numele clientului;
 return -> functia returneaza [struct Client]; 

 INFO: Functia de a crea clientul
*/
struct Client* createClient(char*,char*);

/*
 msg -> variabila de intrare de tip [char*], reprezinta mesajul
 lungime -> variabila de intrare de tip [int], reprezinta lungimea mesajului

 INFO: Aceasta functie are rolul de a prelucra dimensiunea mesajului
*/
void prelucreazaMesaj(char*,int );

/*
INFO : Functia are rolul de a transmite mesajul serverului
*/
void sendMesaj();

/*
INFO: Functia are rolul de a primii mesajul transmis de server
*/
void reciveMesaj();

/*
flag -> variabila de intrare de tip [int] cu scopul de a modifica statusul variabilei globale [exitFlag];

INFO : Functia are rolul de a seta flagul coresunzator variabilei globale [exitFlag] in momentul opririi executiei;
*/
void exitFlag(int);


int main(int argc, char* argv[]){
    
    //Verificam daca numarul de argumente introdus de utilizator este corect in caz contrar aruncam un mesaj de informare si oprim executia
    if(argc != 3){
        printf("Comanda trebuie sa contina 2 parametrii: <IP:PORT> <NUME> \n");
        exit(-1);
    }

    //Initializam clientul;
    cl = createClient(argv[1],argv[2]);

    //Verificam daca clientul a fost construit cu succes iar in caz contrar aruncam un mesaj de informare si oprim executia
    if(cl == NULL){
        printf("EROARE: verificati IP,PORT si numele introdus \n");
        exit(-1);
    }
    
    //Inregistram functia exitFlag ca semnal. In cazul in care utilizatorul nostru v-a intrerupe rularea programului va trimite semnalul
    //Folosita in oprirea threadurilor;
    signal(SIGINT, exitFlag);


    //Declaram structura de tip [sockraddr_in] cu rolul de a stabilii conexiunea la server;
    struct sockaddr_in adresaServer;

    //Creem un endpoint pentru comunicare, valoarea aceasta este stocata in client (fileDescriptor); AF_INET - IPv4 Prot, STOCK_STREAM -  stream byte
    cl->socketFileDescriptor = socket(AF_INET,SOCK_STREAM,0);
    
    //Asignam familia de transport a adreselor. In cazul nostru IPv4
    adresaServer.sin_family = AF_INET;

    //Stocam adresa IP  a serverului; inet_addr - are rolul de a transforma dintr-un sir de caractere in in_addr_t
    adresaServer.sin_addr.s_addr = inet_addr(cl->adresaIp);

    //Asignam portul;
    adresaServer.sin_port = htons(cl->port);

    //Se obtine conexiunea la server; In caz contrar se arunca un mesaj de eroare si se opreste executia;
    int verConexiune = connect(cl->socketFileDescriptor,(struct sockaddr*)&adresaServer,sizeof(adresaServer));
    if(verConexiune == -1){
        printf("Eroare la conexiunea serverului. \n");
        exit(0);
    }

    //Trimitem numele clientului catre server;
    send(cl->socketFileDescriptor,cl->nume,strlen(cl->nume),0);

    //Initializam un thread nou pentru a trimite mesaje;
    pthread_t sendMesajThread;

    //Creem threadul si asignam functia  [sendMesaj()] pe care dorim sa o ruleze; In caz contrar se arunca un mesaj de informare si se opreste executia
     if(pthread_create(&sendMesajThread, NULL, (void *)sendMesaj, NULL) != 0){
		printf("Eroare in crearea threadului\n");
        exit(0);
	}

    //Initializam un thread nou pentru a receptiona mesajele
    pthread_t reciveMesajThread;

    //Creem threadul si asignam functia [reciveMesaj()] pe care dorim sa o ruleze; In caz contrar se arunca un mesaj de informare si se opreste executia
  if(pthread_create(&reciveMesajThread, NULL, (void *)reciveMesaj, NULL) != 0){
		printf("Eroare in crearea threadului\n");
        exit(0);
	}

    //Creem un loop pentru a tine clientul pe server; In cazul in care acesta opreste executia se va iesi din while;
    while (1){
	    if(flagIesire){
		break;
    }
	}

    //Inchidem conexiunea la server
    close(cl->socketFileDescriptor);

    //Dezalocam adresa IP;
    free(cl->adresaIp);

    //Dezalocam clientul;
    free(cl);

    //Incheiem executia programului;
    exit(0);

    
}


struct Client* createClient(char* arg,char* numeClient){
    //Alocam un client nou;
    struct Client* conexiuneNoua = (struct Client*)malloc(sizeof(struct Client));

    //Verificam daca sirul de caractere [arg] contine ":" in caz contrar returnam null;
    char* checkV = strchr(arg,':');
    if(checkV == NULL){
        free(conexiuneNoua);
        return NULL;
    }

    //Prelucram sirul de caractere ( extragem IP)in caz contrar returnam null;
    char* auxPointer = strtok(arg,":");
    if(auxPointer == NULL && strlen(auxPointer) <7){
        free(conexiuneNoua);
        return NULL;
    }else{
        conexiuneNoua->adresaIp = (char*)malloc(strlen(auxPointer));
        strcpy(conexiuneNoua->adresaIp,auxPointer);
    }
    //Extragem portul
    auxPointer = strtok(NULL,"\n");

    //Verificam daca portul este invalid; In caz contrar inregistram portul;
    if(auxPointer == NULL){
        free(conexiuneNoua->adresaIp);
        free(conexiuneNoua);
        return NULL;

    }else{ 
        conexiuneNoua->port = atoi(auxPointer);
    }

    //Prelucram numele clientului si il asignam variabilei corespunzatoare;
    size_t lenNumeClient = strlen(numeClient);
    if(lenNumeClient >= MAX_CAR_CLIENT && lenNumeClient != 0){
        free(conexiuneNoua->adresaIp);
        free(conexiuneNoua);
        return NULL;
    }else{
        conexiuneNoua->nume = (char*)malloc(strlen(numeClient));
        strcpy(conexiuneNoua->nume,numeClient);
    }

    //Initializam descritporul
    conexiuneNoua->socketFileDescriptor =  0;


    //Returnam structura noua;
    return conexiuneNoua;
}


void sendMesaj(){
    //Initializam un buffer si un mesaj
    char mesaj[MAX_MSG_LEN] = {};
    char buffer[MAX_MSG_LEN + 1] = {};

    while(1){
        //Preluam de la tastatura mesajul;
        fgets(mesaj,MAX_MSG_LEN,stdin);

        //Prelucram mesajul;
        prelucreazaMesaj(mesaj,MAX_MSG_LEN);

        //Vom transpune un sir de caractere formatat in buffer
        sprintf(buffer, "%s",mesaj);

        //Vom trimite la server bufferul; in cazul nostru mesajul pe care dorim sa il transmitem
        send(cl->socketFileDescriptor,buffer,strlen(buffer),0);
        
        //Golim mesajul
        bzero(mesaj,MAX_MSG_LEN);

        //Golim bufferul
        bzero(buffer,MAX_MSG_LEN +1);
    }
}

void reciveMesaj() {

    //Initializam un sir de caractere unde vom stoca mesajul serverului;
	char mesaj[MAX_MSG_LEN] = {};
  while (1) {

    //Verificam si stocam mesajul transmis de server
		int primesteMesaj = recv(cl->socketFileDescriptor, mesaj, MAX_MSG_LEN, 0);
    if (primesteMesaj > 0) {
      printf("%s", mesaj);
    } else if (primesteMesaj == 0) {
			break;
    }
    //Stergem mesajul;
		memset(mesaj, 0, sizeof(mesaj));
  }
}


void prelucreazaMesaj(char* msj,int lungimeMesaj){
    //Prelucram dimensiunea mesajului;
    for(int i = 0;i < lungimeMesaj; i++){
        if(msj[i] =='\n'){
            msj[i] = '\0';
            break;
        }
    }

}

void exitFlag(int semnal){
    //Setam flagul de stopare
    flagIesire = 1;
}