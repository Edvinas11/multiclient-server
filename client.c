#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#define BUFF_SIZE 1024
#define MAX_VARDO_ILGIS 128

int sock;

void *skaitymo_gija(void *arg) {
    char buff[BUFF_SIZE];
    while (1) {
        int n = read(sock, buff, BUFF_SIZE - 1);
        if (n <= 0) break;
        buff[n] = '\0';
        if (strncmp(buff, "pranesimas", 10) == 0) {
            printf("%s", buff + 10);
        } else {
            printf("serveris ---> klientas : %s", buff);
        }
    }
    exit(0);
    return NULL;
}

int main() {
    char serverio_adresas[128];
    int portas;
    char input[128];

    printf("iveskite serverio ip adresa: ");
    if (fgets(serverio_adresas, sizeof(serverio_adresas), stdin) == NULL) exit(EXIT_FAILURE);
    serverio_adresas[strcspn(serverio_adresas, "\n")] = '\0';

    printf("iveskite serverio porta: ");
    if (fgets(input, sizeof(input), stdin) == NULL) exit(EXIT_FAILURE);
    portas = atoi(input);

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("sukurti soketą nepavyko");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in serverio_adresas_struct;
    memset(&serverio_adresas_struct, 0, sizeof(serverio_adresas_struct));
    serverio_adresas_struct.sin_family = AF_INET;
    serverio_adresas_struct.sin_port = htons(portas);
    if (inet_pton(AF_INET, serverio_adresas, &serverio_adresas_struct.sin_addr) <= 0) {
        perror("neteisingas serverio adresas");
        exit(EXIT_FAILURE);
    }

    if (connect(sock, (struct sockaddr *)&serverio_adresas_struct, sizeof(serverio_adresas_struct)) < 0) {
        perror("prisijungti nepavyko");
        exit(EXIT_FAILURE);
    }

    char vardas[MAX_VARDO_ILGIS];
    char buff[BUFF_SIZE];

    // protokolo dalis: laukiame "atsiuskvarda" ir siunčiame vardą, kol gausime "vardasok"
    while (1) {
        int n = read(sock, buff, BUFF_SIZE - 1);
        if (n <= 0) break;
        buff[n] = '\0';
        if (strncmp(buff, "atsiuskvarda", 12) == 0) {
            printf("iveskite varda: ");
            if (fgets(vardas, sizeof(vardas), stdin) == NULL) exit(EXIT_FAILURE);
            vardas[strcspn(vardas, "\n")] = '\0';
            write(sock, vardas, strlen(vardas));
            write(sock, "\n", 1);
        } else if (strncmp(buff, "vardasok", 8) == 0) {
            break;
        }
    }

    // paleidžiame giją pranešimų skaitymui iš serverio
    pthread_t gij;
    pthread_create(&gij, NULL, skaitymo_gija, NULL);

    // pagrindinėje gijoje skaitome vartotojo įvestis ir siunčiame serveriui
    while (fgets(buff, sizeof(buff), stdin) != NULL) {
        buff[strcspn(buff, "\n")] = '\0';
        write(sock, buff, strlen(buff));
        write(sock, "\n", 1);
    }

    close(sock);
    return 0;
}
