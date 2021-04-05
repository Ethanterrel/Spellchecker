
#define max_queue_size 100
#define N_THREADS 6
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>

void worker_threads();
void *workerThread(void *arg);
void *logThread(void *arg);
void putlength(char *word);

void put_c(int socket);
int init_dictionary();
int search(char *word, int i);
int getchar();

char* getlength();

int numWords;
int queue_c[max_queue_size];
int writeto;
int queuesize;



pthread_mutex_t mutex_l;
pthread_mutex_t mutex_c;
pthread_cond_t fill_l;
pthread_cond_t empty_c;
pthread_cond_t fill_c;

int main(int argc, char *argv[]) {

    int socket_desc, new_socket, c;
    struct sockaddr_in server, client;
    char *message;
    socket_desc = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_desc == -1){
        puts("Error in socket");
        exit(1);
    }                                  //this is for 127.0.0.1
    server.sin_family = AF_INET;  server.sin_addr.s_addr = INADDR_ANY;  server.sin_port = htons(portNumber);
    int bind_result = bind(socket_desc, (struct sockaddr*)&server, sizeof(server));
    if (bind_result < 0){
        puts("failed to Connect.");
        exit(1);
    }
    listen(socket_desc, 3);
    puts("Waiting for connections...");
    worker_threads();

    numWords = init_dictionary();


}

void worker_threads(){
    pthread_t worker_threads[N_THREADS];
    // create worker threads
    for (size_t i = 0; i < N_THREADS; ++i){
        if (pthread_create(&worker_threads[i],NULL,workerThread, NULL) != 0){
            printf("Failed to create thread\n");
            exit(1);
        }
    }
    pthread_t log_threads[N_THREADS];
    // create logger threads
    for (size_t i = 0; i < N_THREADS; ++i){
        if (pthread_create(&log_threads[i],NULL,logThread,NULL) != 0){
            exit(1);
        }
    }
}

void *logThread(void *arg) {
    // log file
    FILE *logFile = fopen("log.txt", "w");
    fclose(logFile);
    logFile = fopen("log.txt", "a");

    while (1){
        char *word = getlength();
        fprintf(logFile, "%s", word);
        fflush(logFile);
        free(word);
    }

}




void *workerThread(void *arg){

    while (1) {
        char *response;
        char *word = (char *)malloc(sizeof(char)*32);
        int sd = getchar();
        while ( read(sd, word, 32) > 0){

            // search for the word
            if (search(word, numWords)){
                strcat(word, "Word spelled right");
                puts(word);
            } else {
                strcat(word, " not found\n");
                puts(word);
            }
            write(sd, word, strlen(word) + 1);
            putlength(word);
            word = (char *)malloc(sizeof(char)*32); // allocate buffer
        }
        close(sd);
    }
}



 //add socket descriptor
void put_c(int socket) {

    pthread_mutex_lock(&mutex_c);

    while (queuesize == max_queue_size){ pthread_cond_wait(&empty_c, &mutex_c);
    }

    queue_c[writeto] = socket;
    writeto = (writeto + 1) % max_queue_size; queuesize++;

    pthread_cond_signal(&fill_c); pthread_mutex_unlock(&mutex_c);

}


char* getlog() {

    pthread_mutex_lock(&mutex_l);

    while (queuesize == 0) {
        pthread_cond_wait(&fill_l, &mutex_l);
    }
}