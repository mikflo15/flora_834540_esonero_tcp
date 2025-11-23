/*
 * main.c
 *
 * TCP Client - Meteo
 *
 * Portabile su Windows, Linux e macOS
 */

#if defined WIN32
#include <winsock.h>
#else
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#define closesocket close
#endif

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>
#include "protocol.h"

#if defined WIN32
typedef int socklen_t;
#endif

// Controlla che il tipo richiesto sia valido
int valida_tipo(char t) {
    return (t == 't' || t == 'h' || t == 'w' || t == 'p');
}

// Confronta due città ignorando maiuscolo/minuscolo
int confronta_citta(const char *c1, const char *c2) {
    while (*c1 && *c2) {
        if (tolower((unsigned char)*c1) != tolower((unsigned char)*c2))
            return 0;
        c1++;
        c2++;
    }
    return (*c1 == '\0' && *c2 == '\0');
}

// Verifica se la città è supportata
int citta_supportata(const char *city) {
    const char *lista[] = {
        "bari","roma","milano","napoli","torino",
        "palermo","genova","bologna","firenze","venezia"
    };
    int n = sizeof(lista) / sizeof(lista[0]);

    for (int i = 0; i < n; i++) {
        if (confronta_citta(city, lista[i]))
            return 1;
    }
    return 0;
}

// ---------------------------
// Funzioni meteo
// ---------------------------

static void init_random() {
    static int initialized = 0;
    if (!initialized) {
        srand((unsigned int)time(NULL));
        initialized = 1;
    }
}

float get_temperature(void) {
    init_random();
    return -10.0f + (rand() % 5000) / 100.0f;
}

float get_humidity(void) {
    init_random();
    return 20.0f + (rand() % 8000) / 100.0f;
}

float get_wind(void) {
    init_random();
    return (rand() % 10000) / 100.0f;
}

float get_pressure(void) {
    init_random();
    return 950.0f + (rand() % 10000) / 100.0f;
}

// ---------------------------
// Funzioni di sistema
// ---------------------------

void clearwinsock() {
#if defined WIN32
    WSACleanup();
#endif
}

void errorhandler(char *errorMessage) {
    printf("%s", errorMessage);
}


// ---------------------------
// MAIN SERVER
// ---------------------------

int main(int argc, char *argv[]) {

#if defined WIN32
    WSADATA wsa_data;
    int result = WSAStartup(MAKEWORD(2,2), &wsa_data);
    if (result != 0) {
        printf("Error in WSAStartup()\n");
        return 0;
    }
#endif

    unsigned short server_port = SERVER_PORT;

    // Parsing parametri riga di comando
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-p") == 0 && i + 1 < argc) {
            server_port = (unsigned short)atoi(argv[i + 1]);
            i++;
        }
    }

    int my_socket = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (my_socket < 0) {
        errorhandler("creazione della socket fallita.\n");
        clearwinsock();
        return -1;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(my_socket, (struct sockaddr*) &server_addr, sizeof(server_addr)) < 0) {
        errorhandler("bind() fallito.\n");
        closesocket(my_socket);
        clearwinsock();
        return -1;
    }

    if (listen(my_socket, QUEUE_SIZE) < 0) {
        errorhandler("listen() fallito.\n");
        closesocket(my_socket);
        clearwinsock();
        return -1;
    }

    printf("Server in ascolto sulla porta %d...\n", server_port);

    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);

    // ---------------------------
    // CICLO PRINCIPALE DEL SERVER
    // ---------------------------

    while (1) {
        int client_socket = accept(my_socket, (struct sockaddr*)&client_addr, &client_len);
        if (client_socket < 0) {
            errorhandler("accept() fallita.\n");
            continue;
        }

        weather_request_t req;
        int bytes_received = recv(client_socket, &req, sizeof(req), 0);
        if (bytes_received <= 0) {
            closesocket(client_socket);
            continue;
        }

        printf("Richiesta '%c %s' dal client ip %s\n",
               req.type, req.city, inet_ntoa(client_addr.sin_addr));

        weather_response_t res;
        res.status = STATUS_INVALID_REQ;
        res.type = req.type;
        res.value = 0.0f;

        // Validazione tipo
        if (!valida_tipo(req.type)) {
            res.status = STATUS_INVALID_REQ;
        }
        // Validazione città
        else if (!citta_supportata(req.city)) {
            res.status = STATUS_CITY_NOT_FOUND;
            res.type = '\0';
        }
        else {
            res.status = STATUS_SUCCESS;

            switch (req.type) {
                case 't': res.value = get_temperature();
                break;
                case 'h': res.value = get_humidity();
                break;
                case 'w': res.value = get_wind();
                break;
                case 'p': res.value = get_pressure();
                break;
            }
        }

        send(client_socket, &res, sizeof(res), 0);
        closesocket(client_socket);
    }

    closesocket(my_socket);
    clearwinsock();
    return 0;
}
