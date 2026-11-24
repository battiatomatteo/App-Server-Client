# App-Server-Client

## Descrizione generale

Il progetto implementa un sistema client-server in ambiente Linux che consente a più client di richiedere il calcolo dell'hash **SHA-256** di file presenti nel filesystem.

La comunicazione avviene tramite FIFO (named pipe), mentre il server gestisce le richieste concorrenti utilizzando thread POSIX (`pthread`) e una cache per evitare alcuni ricalcoli.

### Obiettivi principali

- Calcolo dell'hash SHA-256 di un file su richiesta di un client.
- Gestione concorrente delle richieste tramite thread.
- FIFO dedicata a ogni client per ricevere la risposta.
- Priorità delle richieste in base alla dimensione del file.
- Cache per memorizzare gli hash già calcolati.

---

## `server.c` - Componente server

### Funzionalità principali

- Riceve richieste tramite una FIFO globale (`/tmp/file_hash_fifo`).
- Accoda le richieste in ordine crescente di dimensione del file.
- Gestisce fino a 5 thread worker contemporaneamente.
- Utilizza una cache con capacità massima di 100 elementi.
- Restituisce il risultato ai client attraverso le rispettive FIFO personali.

### Strutture dati principali

- `CacheEntry`: contiene il percorso del file e il relativo hash SHA-256.
- `RequestNode`: rappresenta una richiesta nella coda, ordinata per dimensione del file.
- `ClientNode`: contiene i riferimenti alle FIFO dei client in attesa della risposta.

### Funzioni principali

- `search_cache()`: verifica se l'hash di un file è già presente nella cache.
- `insert_cache()`: inserisce un nuovo risultato nella cache.
- `enqueue_request()`: inserisce una richiesta nella coda e gestisce eventuali richieste duplicate ancora in attesa.
- `worker_thread()`: elabora le richieste, calcola gli hash e invia le risposte ai client.

---

## `client.c` - Componente client

### Funzionalità principali

- Verifica l'esistenza del file richiesto.
- Crea una FIFO personale identificata tramite il PID del processo.
- Invia al server il percorso del file e quello della propria FIFO.
- Attende la risposta del server.
- Visualizza l'hash SHA-256 oppure un messaggio di errore.

Il client comunica attraverso la FIFO globale e riceve la risposta tramite la propria FIFO personale.

---

## `sha256_utils.c` - Utility per SHA-256

Contiene la funzione:

```c
int digest_file(const char *filename, uint8_t *hash);
```

La funzione calcola l'hash SHA-256 di un file utilizzando le API crittografiche di OpenSSL.

Il file viene aperto in sola lettura e analizzato progressivamente attraverso blocchi di dati.

La funzione restituisce:
- `0` in caso di successo.
- `-1` in caso di errore.

---

## `CMakeLists.txt` - Configurazione della compilazione

Il progetto utilizza CMake per configurare la compilazione dei due eseguibili:

- `client`: compilato a partire da `Client/client.c`.
- `server`: compilato a partire da `Server/server.c` e `Server/sha256_utils.c`.

Le dipendenze principali sono OpenSSL e pthread.

---

## Requisiti

- Sistema operativo Linux.
- Compilatore C compatibile con lo standard utilizzato dal progetto.
- CMake.
- GNU Make.
- OpenSSL con librerie e header di sviluppo.
- Librerie POSIX per la gestione dei thread.

## Compilazione e avvio

Dopo aver clonato il repository:

```bash
git clone https://github.com/battiatomatteo/App-Server-Client.git
cd App-Server-Client
```

### Configurazione e compilazione

```bash
cmake -S . -B build
cmake --build build
```

### Avvio del server

Aprire un terminale ed eseguire:

```bash
./build/server
```

### Avvio di un client

Aprire un secondo terminale ed eseguire:

```bash
./build/client <percorso_file>
```

Sostituire `<percorso_file>` con il percorso del file di cui si desidera calcolare l'hash SHA-256.

---

## Difficoltà riscontrate e possibili miglioramenti

Durante lo sviluppo sono state affrontate diverse problematiche relative a:

- Gestione concorrente delle richieste provenienti da più client.
- Organizzazione della coda delle richieste.
- Sincronizzazione tra thread e accesso alle strutture condivise.
- Comunicazione e gestione delle risposte tramite FIFO.
- Configurazione dell'ambiente di compilazione con CMake.

Tra i possibili sviluppi futuri rientrano il miglioramento della sincronizzazione della cache, della gestione delle richieste in attesa e della validazione dei messaggi ricevuti.
