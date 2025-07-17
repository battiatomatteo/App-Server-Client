# Client-Server SHA256 Application

Progetto universitario di un'applicazione client-server in C per il calcolo di hash SHA-256 di file.

## Descrizione del Progetto

Il progetto implementa un sistema client-server dove:
- Il **client** invia richieste per calcolare l'hash SHA-256 di un file
- Il **server** elabora le richieste in modo concorrente e restituisce l'hash calcolato
- La comunicazione avviene tramite FIFO (Named Pipes)

### Architettura del Sistema

```
Client 1 ──┐
Client 2 ──┼──→ FIFO ──→ Server ──→ Thread Pool ──→ SHA256 Calculation
Client N ──┘                    ├──→ Cache System
                               └──→ Request Scheduling
```

## Funzionalità Implementate

### Server (`server.c`)
- ✅ **Gestione FIFO**: Riceve richieste e invia risposte tramite FIFO
- ✅ **Thread Pool**: Massimo 5 thread concorrenti (configurabile)
- ✅ **Ordinamento richieste**: Schedula per dimensione file (dal più piccolo al più grande)
- ✅ **Sistema di caching**: Cache LRU per 50 coppie percorso-hash
- ✅ **Gestione richieste duplicate**: Un solo calcolo per file identici richiesti simultaneamente
- ✅ **Monitor e sincronizzazione**: Pthread mutex e semafori per thread safety
- ✅ **Gestione errori**: Controllo file non esistenti e errori di I/O

### Client (`client.c`)
- ✅ **Invio richieste**: Invia percorso file al server tramite FIFO
- ✅ **Ricezione risposte**: Riceve hash calcolato dal server
- ✅ **Timeout**: Timeout di 30 secondi per le risposte
- ✅ **Validazione input**: Controllo esistenza file prima dell'invio
- ✅ **Gestione errori**: Messaggi di errore chiari per l'utente

## Requisiti di Sistema

### Dipendenze
- **OpenSSL**: Per il calcolo SHA-256
- **PThreads**: Per la concorrenza
- **GCC**: Compilatore C con supporto C99+
- **CMake**: Sistema di build (versione 3.20+)

### Installazione Dipendenze (Ubuntu/Debian)
```bash
sudo apt update
sudo apt install libssl-dev build-essential cmake
```

### Installazione Dipendenze (CentOS/RHEL)
```bash
sudo yum install openssl-devel gcc cmake
```

### Installazione Dipendenze (macOS)
```bash
brew install openssl cmake
```

## Compilazione

### Metodo 1: Con CMake (Raccomandato)
```bash
# Crea directory di build
mkdir build
cd build

# Configura il progetto
cmake ..

# Compila
make

# I binari saranno in build/
```

### Metodo 2: Compilazione Manuale
```bash
# Server
gcc -o server src/server.c -lssl -lcrypto -lpthread -std=c99

# Client  
gcc -o client src/client.c -lpthread -std=c99

# Programmi di test originali
gcc -o sha256_string src/sha256_string.c -lssl -lcrypto
gcc -o sha256_file src/sha256_file.c -lssl -lcrypto
```

## Utilizzo

### 1. Avviare il Server
```bash
./server
```

Output atteso:
```
Server SHA256 avviato. FIFO: /tmp/sha256_fifo
Numero massimo thread: 5
```

###