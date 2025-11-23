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
#include "protocol.h"

void clearwinsock() {
#if defined WIN32
    WSACleanup();
#endif
}

void errorhandler(char *errorMessage) {
	printf("%s", errorMessage);
}

void print_usage(char *prog_name) {
    printf("Uso: %s -r \"type city\"\n", prog_name);
}

int main(int argc, char *argv[]) {

    char *server_ip = "127.0.0.1";
    int server_port = SERVER_PORT;
    char request_str[128] = {0};

    // Parsing argomenti
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-s") == 0 && i+1 < argc) {
            server_ip = argv[++i];
        } else if (strcmp(argv[i], "-p") == 0 && i+1 < argc) {
            server_port = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-r") == 0 && i+1 < argc) {
            strncpy(request_str, argv[++i], sizeof(request_str)-1);
        }
    }

    if (request_str[0] == '\0') {
        print_usage(argv[0]);
        return -1;
    }

#if defined WIN32
    WSADATA wsa_data;
    int result = WSAStartup(MAKEWORD(2,2), &wsa_data);
    if (result != 0) {
    	printf("Errore in WSAStartup()\n");
        return 0;
    }
#endif

    // Creazione socket
    int my_socket;
    my_socket = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (my_socket < 0) {
        errorhandler("creazione della socket fallita.\n");
        closesocket(my_socket);
        clearwinsock();
        return -1;
    }

    // Configurazione indirizzo server
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);
    server_addr.sin_addr.s_addr = inet_addr(server_ip);

    // Connessione al server
    if (connect(my_socket, (struct sockaddr*) &server_addr, sizeof(server_addr)) < 0) {
        errorhandler("Connessione fallita.\n");
        closesocket(my_socket);
        clearwinsock();
        return -1;
    }

    // Preparazione richiesta
    weather_request_t req;
    req.type = request_str[0];
    strncpy(req.city, request_str+2, sizeof(req.city)-1);
    req.city[sizeof(req.city)-1] = '\0';

    // Invio richiesta
    if (send(my_socket, &req, sizeof(req), 0) != sizeof(req)) {
        errorhandler("send() fallito.\n");
        closesocket(my_socket);
        clearwinsock();
        return -1;
    }

    // Ricezione risposta
    weather_response_t res;
    int bytes_received = recv(my_socket, &res, sizeof(res), 0);
    if (bytes_received <= 0) {
        errorhandler("recv() fallito o connessione chiusa prematuramente\n");
        closesocket(my_socket);
        clearwinsock();
        return -1;
    }

    // Stampa risultato
    printf("Ricevuto risultato dal server ip %s. ", server_ip);
    if (res.status == 0) {
        switch(res.type) {
            case 't': printf("%s: Temperatura = %.1f°C\n", req.city, res.value);
            break;
            case 'h': printf("%s: Umidità = %.1f%%\n", req.city, res.value);
            break;
            case 'w': printf("%s: Vento = %.1f km/h\n", req.city, res.value);
            break;
            case 'p': printf("%s: Pressione = %.1f hPa\n", req.city, res.value);
            break;
        }
    } else if (res.status == 1) {
        printf("Città non disponibile\n");
    } else if (res.status == 2) {
        printf("Richiesta non valida\n");
    }

    // Chiusura socket
    closesocket(my_socket);

    printf("Client terminato.\n");

    clearwinsock();
    return 0;
}
