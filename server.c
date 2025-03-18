#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#define MAX_VARDO_ILGIS 128
#define BUFF_SIZE 1024

// struktūra, apibūdinanti klientą
typedef struct klientas {
    int sock;
    char vardas[MAX_VARDO_ILGIS];
    FILE *fp; // srautas rašymui
    struct klientas *next;
} klientas_t;

klientas_t *klientai = NULL;
pthread_mutex_t klientu_mutex = PTHREAD_MUTEX_INITIALIZER;

// funkcija, siunčianti pranešimą visiems klientams
void transliuoti_pranesima(const char *pranesimas) {
    pthread_mutex_lock(&klientu_mutex);
    klientas_t *tmp = klientai;
    while (tmp) {
        fprintf(tmp->fp, "%s", pranesimas);
        fflush(tmp->fp);
        tmp = tmp->next;
    }
    pthread_mutex_unlock(&klientu_mutex);
}

// gijų funkcija, skirta apdoroti kiekvieną prisijungusį klientą
void *apdoroti_klienta(void *arg) {
    int kliento_sock = *(int *)arg;
    free(arg);
    char buff[BUFF_SIZE];
    char vardas[MAX_VARDO_ILGIS];
    FILE *fp = fdopen(kliento_sock, "r+");
    if (!fp) {
        close(kliento_sock);
        return NULL;
    }

    // siunčiame pradinį pranešimą, kad klientas atsiųstų vardą
    fprintf(fp, "atsiuskvarda\n");
    fflush(fp);

    int vardas_uzregistruotas = 0;
    while (!vardas_uzregistruotas) {
        if (fgets(vardas, sizeof(vardas), fp) == NULL) {
            fclose(fp);
            return NULL;
        }
        vardas[strcspn(vardas, "\n")] = '\0'; // pašaliname naujos eilutės simbolį

        // tikriname, ar toks vardas jau yra prisijungusiųjų sąraše
        pthread_mutex_lock(&klientu_mutex);
        klientas_t *tmp = klientai;
        int pasikartojimas = 0;
        while (tmp) {
            if (strcmp(tmp->vardas, vardas) == 0) {
                pasikartojimas = 1;
                break;
            }
            tmp = tmp->next;
        }
        if (!pasikartojimas) {
            vardas_uzregistruotas = 1;
        }
        pthread_mutex_unlock(&klientu_mutex);

        if (!vardas_uzregistruotas) {
            fprintf(fp, "vardasuzimtas\n");
            fflush(fp);
        }
    }

    // sukuriame naują kliento struktūrą ir įtraukiame į sąrašą
    klientas_t *naujas = malloc(sizeof(klientas_t));
    if (!naujas) {
        fclose(fp);
        return NULL;
    }
    naujas->sock = kliento_sock;
    strncpy(naujas->vardas, vardas, MAX_VARDO_ILGIS);
    naujas->vardas[MAX_VARDO_ILGIS - 1] = '\0';
    naujas->fp = fp;
    pthread_mutex_lock(&klientu_mutex);
    naujas->next = klientai;
    klientai = naujas;
    pthread_mutex_unlock(&klientu_mutex);

    // patvirtiname klientui, kad vardas priimtas
    fprintf(fp, "vardasok\n");
    fflush(fp);

    // tol, kol klientas siunčia pranešimus – serveris jų perduoda visiems
    while (fgets(buff, sizeof(buff), fp) != NULL) {
        buff[strcspn(buff, "\n")] = '\0';
        char pranesimas[BUFF_SIZE + MAX_VARDO_ILGIS];
        snprintf(pranesimas, sizeof(pranesimas), "pranesimas%s: %s\n", naujas->vardas, buff);
        transliuoti_pranesima(pranesimas);
    }

    // klientas atsijungė – pašaliname jį iš sąrašo
    pthread_mutex_lock(&klientu_mutex);
    if (klientai == naujas) {
        klientai = naujas->next;
    } else {
        klientas_t *prev = klientai;
        while (prev && prev->next != naujas) {
            prev = prev->next;
        }
        if (prev) {
            prev->next = naujas->next;
        }
    }
    pthread_mutex_unlock(&klientu_mutex);
    fclose(fp);
    free(naujas);
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "naudojimas: %s portas\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int port = atoi(argv[1]);
    int serverio_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (serverio_sock < 0) {
        perror("sukurti soketą nepavyko");
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    setsockopt(serverio_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in serverio_adresas;
    memset(&serverio_adresas, 0, sizeof(serverio_adresas));
    serverio_adresas.sin_family = AF_INET;
    serverio_adresas.sin_addr.s_addr = INADDR_ANY;
    serverio_adresas.sin_port = htons(port);

    if (bind(serverio_sock, (struct sockaddr *)&serverio_adresas, sizeof(serverio_adresas)) < 0) {
        perror("pririšti nepavyko");
        exit(EXIT_FAILURE);
    }

    if (listen(serverio_sock, 10) < 0) {
        perror("klausyti nepavyko");
        exit(EXIT_FAILURE);
    }

    printf("serveris veikia...\n");

    while (1) {
        struct sockaddr_in kliento_adresas;
        socklen_t adreso_ilgis = sizeof(kliento_adresas);
        int *kliento_sock = malloc(sizeof(int));
        if (!kliento_sock) {
            continue;
        }
        *kliento_sock = accept(serverio_sock, (struct sockaddr *)&kliento_adresas, &adreso_ilgis);
        if (*kliento_sock < 0) {
            perror("priimti nepavyko");
            free(kliento_sock);
            continue;
        }
        pthread_t gij;
        pthread_create(&gij, NULL, apdoroti_klienta, kliento_sock);
        pthread_detach(gij);
    }

    close(serverio_sock);
    return 0;
}
