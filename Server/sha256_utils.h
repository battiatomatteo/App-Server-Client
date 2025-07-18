#ifndef SHA256_UTILS_H
#define SHA256_UTILS_H

#include <stdint.h>
#include <openssl/sha.h>

#define SHA256_HASH_SIZE 32

/**
 * @brief Calcola l'hash SHA-256 di un file specificato dal percorso.
 *
 * @param filename Il percorso del file da processare.
 * @param hash Buffer di 32 byte dove verrà memorizzato l'hash binario.
 * @return 0 in caso di successo, -1 in caso di errore (es. file non trovato/leggibile).
 */
int digest_file(const char *filename, uint8_t *hash);


#endif // SHA256_UTILS_H