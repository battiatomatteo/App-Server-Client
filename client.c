// client.c
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include <time.h>

#define FIFO_PATH "/tmp/file_hash_fifo"
#define MAX_BUFFER 1024

// Crea una FIFO personale per ricevere la risposta
void genera_coda_risposte(char *coda_risposte) {
    snprintf(coda_risposte, MAX_BUFFER, "/tmp/client_fifo_%d", getpid());
    mkfifo(coda_risposte, 0666);
}

/*
argc quanti parametri passo 
argv i parametri in se che passo 
*/
int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Uso: %s <percorso_file>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    const char *filepath = argv[1];

    // Verifica se il file esiste
    if (access(filepath, F_OK) != 0) {
        perror("Il file non esiste");
        exit(EXIT_FAILURE);
    }

    // Crea FIFO di risposta
    char coda_risposte[MAX_BUFFER];
    genera_coda_risposte(coda_risposte);

    // Prepara messaggio da inviare al server
    char messaggio[MAX_BUFFER];
    snprintf(messaggio, MAX_BUFFER, "%s|%s", filepath, coda_risposte);

    // Scrivi nella FIFO principale
    int fifo_w = open(FIFO_PATH, O_WRONLY);
    if (fifo_w == -1) {
        perror("Errore apertura FIFO per scrittura");
        unlink(coda_risposte);
        exit(EXIT_FAILURE);
    }

    write(fifo_w, messaggio, strlen(messaggio));
    close(fifo_w);

    // Leggi la risposta dalla propria FIFO
    int fifo_r = open(coda_risposte, O_RDONLY);
    if (fifo_r == -1) {
        perror("Errore apertura FIFO personale per lettura");
        unlink(coda_risposte);
        exit(EXIT_FAILURE);
    }

    char risposta[MAX_BUFFER];
    ssize_t len = read(fifo_r, risposta, sizeof(risposta) - 1);
    if (len > 0) {
        risposta[len] = '\0';
        printf("Risposta dal server:\n%s", risposta);
    } else {
        perror("Errore lettura risposta");
    }

    close(fifo_r);
    unlink(coda_risposte);
    return 0;
}
