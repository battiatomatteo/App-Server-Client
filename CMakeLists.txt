

cmake_minimum_required(VERSION 3.10)
project(Progetto_s_o)

#Trova OpenSSL
find_package(OpenSSL REQUIRED)

#Crea l'eseguibile
add_executable(client 
        client.c
)
add_executable(server 
        server.c
        #server/sha256_utils.c
)

#Collega OpenSSL
target_link_libraries(server OpenSSL::SSL OpenSSL::Crypto)


