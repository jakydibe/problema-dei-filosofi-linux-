#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <semaphore.h>
#include <signal.h>
#include <pthread.h>
#include <time.h>
#include <sys/mman.h>
#include <string.h>

int nPhilosopher; //numero di filosofi
int nForks;       //numero di forchette
sem_t** forks;    //array globale che dopo mallochiamo

int endStarvLoop = 1;  //flag booleana che si attiva in caso di starvation
int endStallLoop= 1;   //flag booleana che si attiva n caso di stallo
int *fasting;          // array globale in shared memory che tiene conto da quanto tempo ogni filosofo non mangia
int *terminate_philosophers; //flag booleana che si attiva in caso di qualsiasi segnale di terminazione e servira' a far chiudere i procesis filosofi
int *coda;      //Coda di filosofi che aspetta una forchetta destra

pthread_t *terminator;   //array di thread utilizzati per controllare condizioni di terminazione
pthread_t timer;        //thread che si occupa di rilevazione STARVATION
pthread_t stallo;       //thread che si occupa di rilevazione STALLO

pid_t *philosophers;    //array globale mallocato dei processi filosofi
pid_t pid_parent;       //variabile globale che tiene il processo parent



//funzione che mangia (semplicemente fa sleep) per un tempo casuale compreso tra 1 e 4 secondi
void eat(int i){

    int r = 1 + rand() % 4;
    fasting[i]= time(NULL);  //'resetto' il tempo di diguno al UNIX timestamp corrente

    char eat_bar[5] = "|/-\\"; //progress_bar
    for(int j = 0; j<r; j++){
        printf("filosofo[%d] sta mangiando [%c]\n",i,eat_bar[j%4]);
        sleep(1);       //mangio
    }
}

//blocca il semaforo prendendo la forchetta sinistra con sem_wait(forchetta)
void take_forkSx(sem_t* fork){
    printf("aspetto fork\n");
    sem_wait(fork);     //sem_wait() si mette in attesa del semaforo forchettae appena e' libero lo blocca 
}
//blocca il semaforo prendendo la forchetta destra con sem_wait(forchetta), incrementa o decrementa la variabile condivisa coda per rilevare stallo 
void take_forkDx(sem_t* fork){
    printf("aspetto fork\n");
    *coda+=1;       //incremento la variabile coda che significa che il processo e' in attesa della forchetta dx
    
    sem_wait(fork);
    *coda -= 1;     //decremento la variabile coda se ho preso la forchetta dx(non sono piu' in attesa)
}

//metti a posto la forchetta e quindi sblocca il semaforo forchetta con sem_post
void put_fork(sem_t* fork){
    sem_post(fork); //poso la forchetta ebbasta
}

//funzione inutilizzata che sarebbe semplicemente una sleep dopo aver finito di mangiare
void think(){
    int r = 1 +rand() % 4;
    sleep(r);
}

//funzione di callback dei thread terminator che si occupa di rilevare i segnali di terminazione e chiudere tutti i processi filosofi
void* close_process(void * vargp){
    while(1){
        if (*terminate_philosophers == 1){  //utilizzo la variabile condivisa tra processi
            int i = *(int *)vargp;
            pthread_join(terminator[i],NULL); //aspetto che il processo  termini
            printf("processo filosofo[%d] chiuso [+]\n",i);
            exit(1);            //chiudo il processo che ha chiamato il thread di questa funzione
        }
        sleep(0.2); //sleep inserite per evitare di mettere la cpu sotto troppo stress a causa di cicli infiniti
    }
}

//funzione philosopher che pero' ha la soluzione per lo stallo (ovvero che l' ultimo filosofo prende prima la forchetta destra che la sinistra)
void philosopher_solve_stallo(int i){

    pthread_create(&terminator[i], NULL, close_process, &i);    //creo il thread stallo che rileva lo stallo

    while(1){
        if(i < nPhilosopher-1){ //controllo se sono l' ultim filosofo
            printf("filosofo[%d] prova a prendere le fork\n",i);
            take_forkSx(forks[(i + nForks) % nForks]);
            printf("filosofo[%d] fork Sx presa\n",i);
            take_forkDx(forks[(i + nForks+1) % nForks]);
            printf("filosofo[%d] fork Dx presa\n",i);
            printf("filosofo[%d] ha iniziato a mangiare\n",i);

            eat(i);

            put_fork(forks[(i + nForks) % nForks]);
            printf("filosofo[%d] fork Sx posata\n",i);
            put_fork(forks[(i + nForks+1) % nForks]);
            printf("filosofo[%d] fork Dx posata\n",i);
            printf("filosofo[%d] ha smesso a mangiare\n",i);
        }
        else{
            printf("filosofo[%d] prova a prendere le forchette\n",i);

            take_forkSx(forks[(i + nForks+1) % nForks]);
            printf("filosofo[%d] forchetta Dx presa\n",i);

            take_forkDx(forks[(i + nForks) % nForks]);
            printf("filosofo[%d] forchetta Sx presa\n",i);
            printf("filosofo[%d] ha iniziato a mangiare\n",i);

            eat(i);


            put_fork(forks[(i + nForks+1) % nForks]);
            printf("filosofo[%d] forchetta Dx posata\n",i);

            put_fork(forks[(i + nForks) % nForks]);
            printf("filosofo[%d] forchetta Sx posata\n",i);
            printf("filosofo[%d] ha smesso a mangiare\n",i);
        }

        //semplicemente printo le forchette e i valori
        for (int j = 0; j< nForks; j++){
            int value = 0;
            sem_getvalue(forks[j],&value);
            printf("forchetta [%d]: |%d|\n",j,value);
        }
    }
    exit(1);
}

//funzione philosopher che modella il comportamento di ogni filosofo ovvero:
//0) crea un thread (in totale ce ne sara' uno per filosofo) che serve a rilevare i segnali di terminazione
//1) prova a prendere le forchette (prima sx e poi dx)
//2) mangia, metti a posto le forchette
void philosopher(int i){
    pthread_create(&terminator[i], NULL, close_process, &i);    //creo il thread stallo che rileva lo stallo

    while(1){

        printf("filosofo[%d] prova a prendere le forchette\n",i);
        take_forkSx(forks[(i + nForks) % nForks]);
        printf("filosofo[%d] forchetta Sx presa\n",i);
        take_forkDx(forks[(i + nForks+1) % nForks]);
        printf("filosofo[%d] forchetta Dx presa\n",i);
        printf("filosofo[%d] ha iniziato a mangiare\n",i);

        eat(i);

        put_fork(forks[(i + nForks) % nForks]);
        printf("filosofo[%d] forchetta Sx posata\n",i);
        put_fork(forks[(i + nForks+1) % nForks]);
        printf("filosofo[%d] forchetta Dx posata\n",i);
        printf("filosofo[%d] ha smesso a mangiare\n",i);

        //semplicemente printo le forchette e i valori
        for (int j = 0; j< nForks; j++){
            int value = 0;
            sem_getvalue(forks[j],&value);
            printf("forchetta[%d]: |%d|\n",j,value);
        }
    }

}

//funzione che si occupa di fare una terminazione pulita in ogni caso di starvation/stallo/SIGINT con Ctrl+c

void termina(){
    *terminate_philosophers = 1; // modifico la flag che va a terminare tutti i processi filosofi tramite i thread terminator

    fflush(stdout);  //pulire il buffer di output

    // unlinka e chiudi tutti i semafori forchetta 
    for (int i = 0; i < nForks; i++) {
        char nome[32];
        sprintf(nome, "/forchetta%d", i);  
        sem_close(forks[i]); 
        sem_unlink(nome);    
    }
    printf("forchette unlinkate [+]\n");

    //libera la memoria mallocata

    free(philosophers);
    free(forks);
    free(terminator);          
    printf("memoria liberata [+]\n");

    //unmappa la memoria mmappata
    munmap(fasting, nPhilosopher * sizeof(int));
    munmap(terminate_philosophers,sizeof(int));
    munmap(coda, sizeof(int));

    printf("memoria unmappata [+]\n");

    printf("ALLA PROSSIMA.....");
    exit(1);
}
//funzione che gestisce il SIGINT da Ctrl+c
void sigInthandler(int sig_num){
    int pid = (int) getpid();
    if(sig_num==SIGINT && pid == pid_parent){ //controlla se e' stata chiamata dal processo padre ----> e' stato premuto Ctrl+c
        endStallLoop=0;
        endStarvLoop=0;
        printf("\n\n*************|\n");
        printf("SIGINTHANDLER|\n");
        printf("*************|\n");

    }
    termina();

}


//funzione di callback del thread timer che checka per STARVATION
//che si occupa di controllare se c'e' qualche filosofo che non mangia da piu' di 8 secondi
void * checkTime(void * vargp){
    while(endStarvLoop){ //finche la flag booleana di starvation e' attiva
        time_t sec = time(NULL);//sec = tempo attuale
        for(int i=0;i<nPhilosopher;i++){
            if((sec - fasting[i])>=8){          //se la differenza tra il tempo attuale e l' ultima volta in cui ogni filosofo ha mangiato e' maggiore di 8

                for(int j = 0; j<nPhilosopher; j++){    //vai in starvation 
                    printf("folosofo[%d] non mangia da %ld secondi\n",i,sec - fasting[j]);
                }
                printf("\n***|STARVATION|***::{filosofo[%d]}\n",i);
                endStarvLoop=0;
                endStallLoop=0;
                termina();    
            }

        }
        sleep(0.2); //sleep inserite per evitare di mettere la cpu sotto troppo stress a causa di cicli infiniti
    }
}

//funzione di callback del thread che si occupa di verificare se siamo in situazione di stallo
void* rilevaStallo(void * vargp){
    while(endStallLoop){ //esegui all' infinito queste azioni
        if(*coda==nPhilosopher){  //controlla se la coda (ovvero il numero di filosofi che sono in attesa della forchetta dx) e' uguale al numero di filosofi
            printf("\n\n***|STALLO|***\n");
            endStarvLoop=0;
            endStallLoop=0;
            termina();
        }
        sleep(0.2); //sleep inserite per evitare di mettere la cpu sotto troppo stress a causa di cicli infiniti
    }
}

//main
int main(int argc, char* argv[]) {
    

    struct sigaction sa;
    memset(&sa, '\0', sizeof(struct sigaction));
    sa.sa_handler = sigInthandler;  //imposto l'handler dei segnali
    sigaction(SIGINT, &sa ,NULL);   //imposto a quali segnali deve reagire l'handler
    srand(time(NULL));              //inizializzazione seed random
    time_t seconds = time(NULL);    //tempo iniziale (UNIX timestamp)
    

    pid_parent = getpid();
    //FLAG di input
    int starvFlag = 0;
    int stallFlag = 0;
    int enableStallFlag = 0;
    if(argc > 1){   //classico controllo di input da linea di comando
        nPhilosopher= atoi(argv[1]);
        nForks = nPhilosopher;
        
        stallFlag = atoi(argv[2]);
        enableStallFlag = atoi(argv[3]);
        starvFlag = atoi(argv[4]);

    }else{ //se nessun input utilizzo i valor standard di 5 filosofi, 5 forchette
        nPhilosopher=5;
        nForks= nPhilosopher;
    }

    //istanzio array di grandezza variabile(nPhilosopher) con la malloc
    philosophers = (int*) malloc(sizeof(pid_t)*nPhilosopher); 
    forks =  (sem_t**) malloc(sizeof(sem_t)*nForks);        //array di puntatori
    terminator = (pthread_t*) malloc(sizeof(pthread_t)* nPhilosopher);

//Mappo due array; 
//1) Fasting serve a tenere conto di quanto tempo ha digiunato ciascun filosofo
//2) terminate_philosophers e' una flag condivisa che e' letta da dei thread che si occupano di chiudere i processi filosofi
//Li mappo in quanto deve essere memoria accessibile da tutti i processi, percio' devo mappare.
    fasting = mmap(NULL, nPhilosopher * sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);

    terminate_philosophers = mmap(NULL,sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    coda = mmap(NULL,sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    *coda= 0;
    for(int i=0;i<nPhilosopher;i++){ //inizializzo i tempi di digiuno al tempo iniziale
        fasting[i]=seconds;
    }

    //gestisco input in linea di comando
    if (starvFlag == 1){
        pthread_create(&timer, NULL, checkTime, NULL);    //creo il thread timer che si occupa di checkare i tempi di starvation di tutti i filosofi
    }
    
    //inizializzo le forchette come semafori, le inizializzo a 1 perche' devono essere libere inizialmente
    for (int i = 0; i<nForks; i++){

        char nome[32]; // array sufficientemente grande
        sprintf(nome, "/forchetta%d", i);        
        forks[i] = sem_open(nome, O_CREAT, S_IRWXU, 1);

    } 
    if (stallFlag == 1){
        pthread_create(&stallo, NULL, rilevaStallo, NULL);    //creo il thread stallo che rileva lo stallo
    }


    for(int i=0;i<nPhilosopher;i++){
        philosophers[i]=fork();//creo tutti i processi figli
        if (philosophers[i] < 0) {
        // Forking failed
            fprintf(stderr, "Forking failed\n");
            return 1;
        } else if (philosophers[i] == 0) {
            // Child process
            printf("filosofo[%d]\n", i);
            //se sono riuscito a creare i processi allora chiamo la funzione philosopher che descrive il comportamento ciclico dei filosofi
            if(enableStallFlag == 1){//se la flag di risoluzione dello stallo e' attiva allora chiama la philosopher
                philosopher_solve_stallo(i);
            }
            else{
                philosopher(i);
            }

            return 0;
        }

    }
    for(int i=0;i<nPhilosopher;i++){
        wait(NULL);     //Il processo parent aspetta che i processi child terminino
    }
    
    pthread_join(stallo, NULL);
    pthread_join(timer, NULL);
    printf("ARRIVEDERCI");
    return 0;
}