#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include "sha256_utils.h"

// Funzione per calcolare l'hash SHA-256 di un file
// Restituisce 0 in caso di successo, -1 in caso di errore
int digest_file(const char *filename, uint8_t *hash) {
    SHA256_CTX ctx;
    SHA256_Init(&ctx);

    char buffer[32];
    int file = open(filename, O_RDONLY);
    if (file == -1) {
        perror("File non trovato");
        return -1;        
    }

    ssize_t bR;
    do {
        bR = read(file, buffer, 32);
        if (bR > 0) {
            SHA256_Update(&ctx, (uint8_t *)buffer, bR);
        } else if (bR < 0) {
            perror("Errore lettura file");
            return -1;
        }
    } while (bR > 0);

    SHA256_Final(hash, &ctx);
    close(file);
    return 0;
}
