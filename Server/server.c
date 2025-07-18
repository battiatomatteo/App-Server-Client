// server.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <fcntl.h>
#include <unistd.h>
#include <openssl/sha.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <stdint.h>
#include <sha256_utils.h>

#define FIFO_PATH "/tmp/file_hash_fifo"
#define MAX_PTHREADS 5
#define MAX_BUFFER 1024
#define CACHE_SIZE 100
#define MAX_CLIENTS 10

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
int active_threads = 0;

// Cache percorso-hash
typedef struct {
    char path[MAX_BUFFER];
    uint8_t hash[SHA256_DIGEST_LENGTH];
    int valido;
} CacheEntry;

CacheEntry cache[CACHE_SIZE];

// Richiesta client
typedef struct ClientNode {
    char fifo_path[MAX_BUFFER];
    struct ClientNode *next;
} ClientNode;

// Struttura per file richiesti
typedef struct RequestNode {
    char filepath[MAX_BUFFER];
    off_t dim_file;
    ClientNode *clients;
    struct RequestNode *next;
} RequestNode;

RequestNode *coda_richiesta = NULL;

// Inizializza la cache
int search_cache(const char *filepath, uint8_t *hash_out) {
    for (int i = 0; i < CACHE_SIZE; i++) {  // Controlla se il file è già in cache
        if (cache[i].valido && strcmp(cache[i].path, filepath) == 0) { 
            memcpy(hash_out, cache[i].hash, SHA256_DIGEST_LENGTH);  // Copia l'hash nella variabile di output
            return 1;
        }
    }
    return 0;
}

// Inserisce un file nella cache
// Se la cache è piena, non fa nulla 
void insert_cache(const char *filepath, const uint8_t *hash) {
    for (int i = 0; i < CACHE_SIZE; i++) { 
        if (!cache[i].valido) { // Trova una posizione libera nella cache
            strcpy(cache[i].path, filepath);
            memcpy(cache[i].hash, hash, SHA256_DIGEST_LENGTH);
            cache[i].valido = 1;
            return;
        }
    }
}

// Aggiunge una richiesta alla coda, ordinata per dimensione del file
// Se il file è già in coda, aggiunge il client alla lista dei client
// Se il file non esiste o non è leggibile, non aggiunge la richiesta
void enqueue_request(const char *filepath, const char *client_fifo) {
    struct stat st;
    // Verifica se il file esiste e ottieni la sua dimensione
    off_t dimensione = stat(filepath, &st) == 0 ? st.st_size : 0;
    // Se il file non esiste o non è leggibile, non aggiungere la richiesta
    RequestNode *nodoCorrente = coda_richiesta, *nodoPrec = NULL;
    // Controlla se il file è già in coda
    while (nodoCorrente && strcmp(nodoCorrente->filepath, filepath) != 0) {
        nodoPrec = nodoCorrente;
        nodoCorrente = nodoCorrente->next;
    }
    // Se il file è già in coda, aggiungi il client
    if (nodoCorrente) {
        // File già in coda, aggiungi client
        ClientNode *c = malloc(sizeof(ClientNode));
        strcpy(c->fifo_path, client_fifo);
        // Aggiungi il client alla lista dei client
        c->next = nodoCorrente->clients;
        nodoCorrente->clients = c;
        return;
    }

    // Nuova richiesta
    RequestNode *nuovoNodo = malloc(sizeof(RequestNode));
    strcpy(nuovoNodo->filepath, filepath);
    nuovoNodo->dim_file = dimensione;
    nuovoNodo->clients = malloc(sizeof(ClientNode));
    strcpy(nuovoNodo->clients->fifo_path, client_fifo);
    nuovoNodo->clients->next = NULL;
    nuovoNodo->next = NULL;

    // Inserimento ordinato per dimensione
    nodoCorrente = coda_richiesta;
    nodoPrec = NULL;
    // Trova la posizione giusta per inserire il nuovo nodo
    while (nodoCorrente && nodoCorrente->dim_file < dimensione) {
        nodoPrec = nodoCorrente;
        nodoCorrente = nodoCorrente->next;
    }
    if (!nodoPrec) {
        nuovoNodo->next = coda_richiesta;
        coda_richiesta = nuovoNodo;
    } else {
        nuovoNodo->next = nodoPrec->next;
        nodoPrec->next = nuovoNodo;
    }
}

// Thread worker che gestisce le richieste
// Si occupa di calcolare l'hash del file e rispondere ai client
// Se il file è già in cache, lo restituisce direttamente
// Se il file non è leggibile, restituisce un errore
// Utilizza mutex per garantire l'accesso sicuro alla coda e alla cache
void *worker_thread(void *arg) {
    pthread_mutex_lock(&mutex);
    if (!coda_richiesta) {  // Se non ci sono richieste, rilascia il mutex e termina
        pthread_mutex_unlock(&mutex);
        return NULL;
    }

    RequestNode *req = coda_richiesta;
    coda_richiesta = req->next;
    pthread_mutex_unlock(&mutex);

    uint8_t hash[SHA256_DIGEST_LENGTH];
    int ok = 1; //

    if (!search_cache(req->filepath, hash)) {  // Se non è in cache
        // Controlla se il file esiste e se è leggibile
        if (access(req->filepath, R_OK) != 0) {
            ok = 0;  // File non leggibile
        } else {
            if(digest_file(req->filepath, hash) < 0){
                ok = 0; // Errore durante il calcolo dell'hash
            }
            else {
                // Aggiungi l'hash alla cache
                pthread_mutex_lock(&mutex);
                insert_cache(req->filepath, hash);
                pthread_mutex_unlock(&mutex);
            }
        }
    }

    char hash_str[MAX_BUFFER] = {0};
    if (ok) {
        snprintf(hash_str, MAX_BUFFER, "SHA256(%s) = ", req->filepath); // Inizializza la stringa con il percorso del file
        for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) { // Converte l'hash in esadecimale
            char byte[3];
            snprintf(byte, sizeof(byte), "%02x", hash[i]);
            strncat(hash_str, byte, 2);
        }
        strncat(hash_str, "\n", 2);
    } else {
        snprintf(hash_str, MAX_BUFFER, "Errore: impossibile leggere il file %s\n", req->filepath);
    }

    // Rispondi a tutti i client
    ClientNode *c = req->clients;
    while (c) {
        int out = open(c->fifo_path, O_WRONLY);
        if (out >= 0) {
            write(out, hash_str, strlen(hash_str));
            close(out);
            printf("[Server] Client ha chiesto %s, consegnato correttamente.\n", req->filepath);
        } else {
            printf("[Server] Client ha chiesto %s, non consegnato.\n", req->filepath);
        }
        ClientNode *tmp = c;
        c = c->next;
        free(tmp);
    }
    free(req);

    pthread_mutex_lock(&mutex);
    active_threads--;
    pthread_mutex_unlock(&mutex);
    return NULL;
}

int main() {
    if (mkfifo(FIFO_PATH, 0666) == -1 && errno != EEXIST) {
        perror("Errore creazione FIFO");
        exit(EXIT_FAILURE);
    }

    printf("[Server] In ascolto su FIFO: %s\n", FIFO_PATH);

    while (1) {
        char buffer[MAX_BUFFER];
        int fifo = open(FIFO_PATH, O_RDONLY);
        if (fifo == -1) {
            perror("Errore apertura FIFO");
            continue;
        }

        ssize_t len = read(fifo, buffer, sizeof(buffer) - 1);  // Leggi dalla FIFO
        close(fifo);
        if (len <= 0) continue;

        buffer[len] = '\0';

        char *filepath = strtok(buffer, "|");
        char *client_fifo = strtok(NULL, "|");

        pthread_mutex_lock(&mutex);
        enqueue_request(filepath, client_fifo);
        if (active_threads < MAX_PTHREADS) {  // Controlla se ci sono thread attivi
            pthread_t tid;
            pthread_create(&tid, NULL, worker_thread, NULL);
            pthread_detach(tid);
            active_threads++;
        }
        pthread_mutex_unlock(&mutex);
    }

    unlink(FIFO_PATH);
    return 0;
}