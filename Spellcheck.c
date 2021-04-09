#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>


#define max_length 200
#define max_size 10000
#define max_queue_size 99
#define N_THREADS 15


char* format_word(char *word);


int init_dictionary();
int init_variables();
int get_word(char *word);
int getchar();
int get_input(char* user_word);

void spawn_worker_threads();
void *workerThread(void *arg);
void *logThread(void *arg);
void print_dictionary();

int search(char *word, int i);
void put_c(int socket)

void putlength(char *word);
char* getlength();

char *filename;
FILE *words;
char **dictionary;
int numWords;


int queue_c[max_queue_size];
int writePtr_c;
int readPtr_c;
int queue_size_c;


char *queue_l[max_queue_size];
int writePtr_l;
int readPtr_l;
int queue_size_l;


pthread_mutex_t mutex_l;
pthread_mutex_t mutex_c;
pthread_cond_t empty_l;
pthread_cond_t fill_l;
pthread_cond_t empty_c;
pthread_cond_t fill_c;


int main(int argc, char *argv[]) {

    int portNumber;

    if(argc == 1) {
        portNumber = 5555;  filename = "dictionary.txt";
    } else if (argc == 2){ int num = atoi(argv[1]);
        if (num > 0) {
            portNumber = atoi(argv[1]);
            printf("\nport number: %d\n", portNumber); filename = "dictionary.txt";
        } else {
            portNumber = 8888; filename = argv[1];
            printf("\nfilename: %s\n", filename);
        }
    } else if (argc == 3) {
        int num = atoi(argv[1]);
        if (num > 0) {
            portNumber = atoi(argv[1]); filename = argv[2];

            printf("\nport number: %d\n", portNumber);
            printf("\nfilename: %s\n", filename);

        } else {
            portNumber = atoi(argv[2]); filename = argv[1];

            printf("\nport number: %d\n", portNumber);
            printf("\nfilename: %s\n", filename);
        }

    }


    int socket_desc, c; struct sockaddr_in server, client;
    socket_desc = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_desc == -1){
        puts("Error in socket");
        exit(1);
    }

    server.sin_family = AF_INET; server.sin_addr.s_addr = INADDR_ANY; server.sin_port = htons(portNumber); // uses 127.0.0.1

    int bind_result = bind(socket_desc, (struct sockaddr*)&server, sizeof(server));
    if (bind_result < 0){
        puts("failed to connect");
        exit(1);
    }
    puts("Bind is done");
    listen(socket_desc, 3);
    puts("Waiting for connections");


    if(!init_variables()) {
        exit(0);
    }

    spawn_worker_threads();
    numWords = init_dictionary();

    while (1){

        int fd = accept(socket_desc, (struct sockaddr *)  &client, (socklen_t *) &c);
        if (fd < 0) {
            puts("error");
            continue;
        }
        puts("Connection accepted");
        put_c(fd);
    }

    for(size_t i = 0; dictionary[i] != NULL; i++){  free(dictionary[i]);
    }
    free(dictionary);
}

void spawn_worker_threads(){

    pthread_t worker_threads[N_THREADS];
    for (size_t i = 0; i < N_THREADS; ++i){
        if (pthread_create(&worker_threads[i], NULL, workerThread, NULL) != 0){
            printf(" Failed to create thread");
            exit(1);
        }
    }
    pthread_t log_threads[N_THREADS];
    for (size_t i = 0; i < N_THREADS; ++i){
        if (pthread_create(&log_threads[i], NULL, logThread, NULL) != 0){
            printf("Error creating thread");
            exit(1);
        }
    }
}

void *workerThread(void *arg){

    while (1) {
        char *word = (char *)malloc(sizeof(char)*32);
        int sd = get_c();
        while ( read(sd, word, 32) > 0){


            word = format_word(word);


            if (search(word, numWords)){

                strcat(word," ---Word Correct\n");
                puts(word);
            } else {
                strcat(word," ---Not found\n");
                puts(word);
            }
            write(sd, word, strlen(word) + 1); put_l(word);
            word = (char *)malloc(sizeof(char)*32);
        }
        close(sd);
    }
}

void *logThread(void *arg) {
    FILE *logFile = fopen("log.txt", "w"); fclose(logFile); logFile = fopen("log.txt", "a");

    while (1){

        char *word = get_l();
        fprintf(logFile, "%s", word); fflush(logFile); free(word);
    }

    fclose(logFile);
}
char* format_word(char *word) {

    for(size_t i = 0; word[i] != '\0'; i++) {
        if (word[i] == '\n'){ word[i] = '\0';
            return word;
        } else if (word[i] == '\t'){ word[i] = '\0';
            return word;
        } else if (word[i] == '\r'){ word[i] = '\0';
            return word;
        } else if (word[i] == ' '){ word[i] = '\0';
            return word;
        }
    }
    return word;
}

int init_dictionary() {
    words = fopen(filename, "r");

    if (words == NULL) {
        puts("error opening file");
        exit(0);
    }

    char word[max_length];
    dictionary = calloc(max_size, sizeof(char *));

    int i = 0;
    while(get_word(word) != -1) {
        dictionary[i] = (char *) malloc(100);
        if(word[strlen(word) - 1] == '\n') {  word[strlen(word) - 1] = '\0';
        }
        strcpy(dictionary[i], word);
        i++;
    }
    fclose(words);
    return i;

}


int init_variables() {
    if(pthread_cond_init(&empty_l, NULL) != 0) { puts("error");
        return 0;
    }
    if(pthread_cond_init(&fill_l, NULL) != 0) { puts("error");
        return 0;
    }
    if(pthread_cond_init(&empty_c, NULL) != 0){ puts("error");
        return 0;
    }
    if(pthread_cond_init(&fill_c, NULL) != 0) { puts("error");
        return 0;
    }
    if(pthread_mutex_init(&mutex_c, NULL) != 0) { puts("error");
        return 0;
    }
    if(pthread_mutex_init(&mutex_l, NULL) != 0) { puts("error");
        return 0;
    }

    return 1;
}


int get_word(char *word) {

    size_t max = max_length;
    int c = getline(&word, &max, words);
    return c;

}


int getinput(char* user_word) {
    puts("enter a word");
    if (fgets(user_word, max_length, stdin) == NULL) {
        return 0;
    }

    return 1;

}

void printdictionary() {
    for(size_t i = 0; dictionary[i] != NULL; i++) {
        puts(dictionary[i]);
    }
}



int search(char *word, int i) {
    for(size_t j = 0; j < i; j++) {
        if(strcasecmp(dictionary[j], word) == 0) {
            return 1;
        }
    }

    return 0;
}


void put_c(int socket) {

    pthread_mutex_lock(&mutex_c);


    while (queue_size_c == max_queue_size){
        pthread_cond_wait(&empty_c, &mutex_c);
    }

    queue_c[writePtr_c] = socket;
    writePtr_c = (writePtr_c + 1) % max_queue_size; queue_size_c++;

    pthread_cond_signal(&fill_c); pthread_mutex_unlock(&mutex_c);

}


int get_c() {

    pthread_mutex_lock(&mutex_c);


    while (queue_size_c == 0){
        pthread_cond_wait(&fill_c, &mutex_c);
    }

    int socket = queue_c[readPtr_c];
    readPtr_c = (readPtr_c + 1) % max_queue_size; queue_size_c--;

    pthread_cond_signal(&empty_c); pthread_mutex_unlock(&mutex_c); return socket;

}


void putlength(char *word) {

    pthread_mutex_lock(&mutex_l);

    while (queue_size_l == max_queue_size){
        pthread_cond_wait(&empty_l, &mutex_l);
    }

    queue_l[writePtr_l] = word;
    writePtr_l = (writePtr_l + 1) % max_queue_size;queue_size_l++;


    pthread_cond_signal(&fill_l); pthread_mutex_unlock(&mutex_l);

}


char* getlength() {

    pthread_mutex_lock(&mutex_l);


    while (queue_size_l == 0){
        pthread_cond_wait(&fill_l, &mutex_l);
    }

    char *word = queue_l[readPtr_l];
    readPtr_l = (readPtr_l + 1) % max_queue_size; queue_size_l--;

    pthread_cond_signal(&empty_l); pthread_mutex_unlock(&mutex_l); return word;

}



