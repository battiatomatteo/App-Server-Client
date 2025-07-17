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

void digest_file(const char *filename, uint8_t *hash) {
    SHA256_CTX ctx;
    SHA256_Init(&ctx);

    char buffer[32];
    int file = open(filename, O_RDONLY);
    if (file == -1) {
        perror("File non trovato");
        exit(1);
    }

    ssize_t bR;
    do {
        bR = read(file, buffer, 32);
        if (bR > 0) {
            SHA256_Update(&ctx, (uint8_t *)buffer, bR);
        } else if (bR < 0) {
            perror("Errore lettura file");
            exit(1);
        }
    } while (bR > 0);

    SHA256_Final(hash, &ctx);
    close(file);
}

int search_cache(const char *filepath, uint8_t *hash_out) {
    for (int i = 0; i < CACHE_SIZE; i++) {
        if (cache[i].valido && strcmp(cache[i].path, filepath) == 0) {
            memcpy(hash_out, cache[i].hash, SHA256_DIGEST_LENGTH);
            return 1;
        }
    }
    return 0;
}

void insert_cache(const char *filepath, const uint8_t *hash) {
    for (int i = 0; i < CACHE_SIZE; i++) {
        if (!cache[i].valido) {
            strcpy(cache[i].path, filepath);
            memcpy(cache[i].hash, hash, SHA256_DIGEST_LENGTH);
            cache[i].valido = 1;
            return;
        }
    }
}

void enqueue_request(const char *filepath, const char *client_fifo) {
    struct stat st;
    off_t dimensione = stat(filepath, &st) == 0 ? st.st_size : 0;

    RequestNode *nodoCorrente = coda_richiesta, *nodoPrec = NULL;
    while (nodoCorrente && strcmp(nodoCorrente->filepath, filepath) != 0) {
        nodoPrec = nodoCorrente;
        nodoCorrente = nodoCorrente->next;
    }

    if (nodoCorrente) {
        // File già in coda, aggiungi client
        ClientNode *c = malloc(sizeof(ClientNode));
        strcpy(c->fifo_path, client_fifo);
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

void *worker_thread(void *arg) {
    pthread_mutex_lock(&mutex);
    if (!coda_richiesta) {
        pthread_mutex_unlock(&mutex);
        return NULL;
    }

    RequestNode *req = coda_richiesta;
    coda_richiesta = req->next;
    pthread_mutex_unlock(&mutex);

    uint8_t hash[SHA256_DIGEST_LENGTH];
    int success = 1;

    if (!search_cache(req->filepath, hash)) {
        if (access(req->filepath, R_OK) != 0) {
            success = 0;
        } else {
            digest_file(req->filepath, hash);
            pthread_mutex_lock(&mutex);
            insert_cache(req->filepath, hash);
            pthread_mutex_unlock(&mutex);
        }
    }

    char hash_str[MAX_BUFFER] = {0};
    if (success) {
        snprintf(hash_str, MAX_BUFFER, "SHA256(%s) = ", req->filepath);
        for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
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

        ssize_t len = read(fifo, buffer, sizeof(buffer) - 1);
        close(fifo);
        if (len <= 0) continue;

        buffer[len] = '\0';

        char *filepath = strtok(buffer, "|");
        char *client_fifo = strtok(NULL, "|");

        pthread_mutex_lock(&mutex);
        enqueue_request(filepath, client_fifo);
        if (active_threads < MAX_PTHREADS) {
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


/*
✔️ FIFO condivisa tra client e server

✔️ Hash SHA-256 con OpenSSL (digest_file)

✔️ Thread pool con massimo MAX_PTHREADS

✔️ Caching delle coppie percorso-hash

✔️ Gestione concorrente con pthread_mutex
*/