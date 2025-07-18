## Descrizione generale

Questo progetto implementa un sistema client-server in ambiente Linux che consente a più client di richiedere il calcolo dell'hash **SHA-256** di file presenti nel filesystem. La comunicazione tra client e server avviene tramite **FIFO (named pipe)**. Il server è in grado di gestire richieste concorrenti usando **pthread**, mantenendo una **cache** per evitare ricalcoli inutili.

### Obiettivi principali:

- Calcolo dell’hash SHA-256 di un file su richiesta di un client.
- Gestione concorrente delle richieste tramite thread.
- FIFO dedicata per ogni client per ricevere la risposta.
- Priorità nella coda del server basata sulla dimensione del file.
- Cache per evitare ricalcoli su file già processati.

---

## `server.c` - Componente server

### Funzionalità principali

- Riceve richieste tramite una FIFO globale (`/tmp/file_hash_fifo`) contenenti:
  ```
  <percorso_file>|<fifo_risposta_client>
  ```
- Accoda la richiesta in ordine di dimensione crescente del file.
- Avvia fino a 5 thread worker per gestire i calcoli.
- Usa una **cache** per salvare l'hash dei file già calcolati.
- Invia la risposta ai client aprendo la loro FIFO personale.

### Strutture dati principali

- `CacheEntry`: array statico che contiene fino a 100 voci di cache (`filepath` + `hash`).
- `RequestNode`: nodo della coda richieste, ordinato per dimensione file.
- `ClientNode`: lista di client che richiedono lo stesso file.

### Parti di codice rilevanti

#### Cache
```c
int search_cache(const char *filepath, uint8_t *hash_out);
void insert_cache(const char *filepath, const uint8_t *hash);
```
Permettono di evitare il ricalcolo dell'hash se già presente in cache.

#### Coda delle richieste
```c
void enqueue_request(const char *filepath, const char *client_fifo);
```
Inserisce in modo **ordinato** per dimensione e gestisce richieste duplicate sullo stesso file.

#### Thread worker
```c
void *worker_thread(void *arg);
```
Calcola l’hash (tramite `digest_file()`), accede alla cache in modo sicuro (con `mutex`), e risponde ai client.

#### Ciclo principale
```c
while (1) {
    read(fifo, buffer, ...);
    enqueue_request(...);
    if (active_threads < MAX_PTHREADS) {
        pthread_create(...);
    }
}
```
Gestisce le nuove richieste e lancia thread solo se sotto la soglia di concorrenza.

---

## `client.c` - Componente client

### Funzionalità principali

- Verifica se il file esiste ( nel caso non esiste mostra un messaggio di errore ) .
- Crea una FIFO personale (`/tmp/client_fifo_<pid>`).
- Invia al server un messaggio con:
  ```
  <filepath>|<client_fifo>
  ```
- Attende la risposta dal server nella FIFO personale.
- Stampa l'hash (o messaggio di errore).

### Parti di codice rilevanti

#### Generazione FIFO client
```c
void genera_coda_risposte(char *coda_risposte) {
    snprintf(coda_risposte, MAX_BUFFER, "/tmp/client_fifo_%d", getpid());
    mkfifo(coda_risposte, 0666);
}
```

#### Invio e ricezione
```c
write(fifo_w, messaggio, strlen(messaggio));
...
read(fifo_r, risposta, sizeof(risposta) - 1);
```
Comunica in modo sincrono: invia richiesta e attende la risposta.

---

## `sha256_utils.c` - Utility per hash

### Funzionalità principali

- Contiene una sola funzione:
```c
int digest_file(const char *filename, uint8_t *hash);
```
Calcola l’hash SHA-256 del file usando le API OpenSSL:
- `SHA256_Init`, `SHA256_Update`, `SHA256_Final`.

### Logica
- Apre il file in sola lettura.
- Legge a blocchi di 32 byte.
- Aggiorna il contesto SHA-256 per ogni blocco letto.
- Chiude il file e restituisce l’hash binario.

---

## `CMakeLists.txt` - Build configuration

```cmake
find_package(OpenSSL REQUIRED)
...
add_executable(client Client/client.c)
add_executable(server Server/server.c Server/sha256_utils.c)
target_link_libraries(server OpenSSL::SSL OpenSSL::Crypto)
```

### Note:

- Il progetto richiede **OpenSSL** per il calcolo degli hash.
- I file `client.c` e `server.c` si trovano rispettivamente in `Client/` e `Server/`.
- I file `.c` sono compilati in due eseguibili distinti: `client` e `server`.

---

## Difficoltà riscontrate

- Gestione della coda delle richieste da parte dei vari client.
- Configurazione di CMake. 
- Risposta da parte del server a tutti i client che richiedono lo stesso file.




