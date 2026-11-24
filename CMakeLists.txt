
cmake_minimum_required(VERSION 3.10)

project(Progetto_s_o)

# Trova le dipendenze necessarie
find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)

# Directory degli header
include_directories(
    ${PROJECT_SOURCE_DIR}/Server
    ${OPENSSL_INCLUDE_DIR}
)

# Crea gli eseguibili
add_executable(client
    Client/client.c
)

add_executable(server
    Server/server.c
    Server/sha256_utils.c
)

# Collega le librerie necessarie al server
target_link_libraries(server
    PRIVATE
    OpenSSL::SSL
    OpenSSL::Crypto
    Threads::Threads
)
