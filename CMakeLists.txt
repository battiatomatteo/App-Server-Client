

cmake_minimum_required(VERSION 3.10)
project(Progetto_s_o)

#Trova OpenSSL
find_package(OpenSSL REQUIRED)

include_directories(
        ${PROJECT_SOURCE_DIR}/Server
        ${OPENSSL_INCLUDE_DIR}
)

#Crea l'eseguibile
add_executable(client 
        Client/client.c
)
add_executable(server 
        Server/server.c
        Server/sha256_utils.c
)

#Collega OpenSSL
target_link_libraries(server OpenSSL::SSL OpenSSL::Crypto)


