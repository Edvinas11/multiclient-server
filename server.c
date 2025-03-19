#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <ctype.h>
#include <pthread.h>

// #define PORT 20000
#define BUFFER_SIZE 1024
#define MAX_USERS 10

// Shared arrays
char* users[MAX_USERS];
int sockets[MAX_USERS];
int user_count = 0;

// Mutex to protect shared array
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

void shift_left_after_delete(int delete_index) {
    free(users[delete_index]);
    users[delete_index] = NULL;

    for(int i = delete_index; i < user_count - 1; i++) {
        users[i] = users[i + 1];
        sockets[i] = sockets[i + 1];
    }

    users[user_count - 1] = NULL;
    return;
}

void format_buffer_after_read(char buffer[], int* size) {
    for (int i = 0; i < *size; i++) {
        if (buffer[i] == 13 || buffer[i] == 10) {
            buffer[i] = '\0';
            *size = i;
        }
    }
}

// Function to handle communication with a client
void *handle_client(void *client_socket_ptr) {
    int client_socket = *((int *)client_socket_ptr);
    char buffer[BUFFER_SIZE] = {0};
    char username[BUFFER_SIZE];
    int bytes_received, values_read;;
    char* name_protocol = "ATSIUSKVARDA\n";
    char* name_protocol_success = "VARDASOK\n";
    int error_first_loop = 0;
    char msg[BUFFER_SIZE * 3];

    while(1) {
        error_first_loop = 0;
        memset(buffer, 0, sizeof(buffer));

        send(client_socket, name_protocol, strlen(name_protocol), 0);

        values_read = read(client_socket, buffer, sizeof(buffer) - 1); // subtract 1 for the null terminator
        if (values_read == 0) {
            perror("Client read failed");
            close(client_socket);
            error_first_loop = 1;
            break;
        }
        if(values_read < 0) {
            perror("Client read failed");
            continue;
        }

        format_buffer_after_read(buffer, &values_read);

        pthread_mutex_lock(&mutex);

        int name_exists = 0;
        for(int i = 0; i < user_count; i++) {
            // printf("Comparing '%s' to '%s'\n", buffer, users[i]);
            if (strcmp(buffer, users[i]) == 0) {
                name_exists = 1;
            }
        }

        pthread_mutex_unlock(&mutex);

        if(name_exists == 0) {
            pthread_mutex_lock(&mutex);

            if (user_count + 1 <= MAX_USERS) {
                users[user_count] = (char *)malloc(sizeof(buffer));
                strcpy(users[user_count], buffer);
                sockets[user_count] = client_socket;
                user_count++;
                strcpy(username, buffer);
                send(client_socket, name_protocol_success, strlen(name_protocol_success), 0);
            }
            else {
                printf("Reached max user size. User not added.");
                error_first_loop = 0;
            }

            pthread_mutex_unlock(&mutex);
            break;
        }
    }

    printf("Client connected, name - %s\n", username);


    if(error_first_loop == 0) {
        while(1) {
            memset(buffer, 0, sizeof(buffer));

            values_read = read(client_socket, buffer, sizeof(buffer) - 1); // subtract 1 for the null terminator
            if (values_read == 0) {
                perror("Client read failed");
                close(client_socket);
                break;
            }
            if(values_read < 0) {
                perror("Client read failed");
                continue;
            }

            format_buffer_after_read(buffer, &values_read);

            printf("User(%s) sent: '%s'\n", username, buffer);

            if(strcmp(buffer, "exit") == 0) {
                break;
            }

            pthread_mutex_lock(&mutex);

            printf("Sending to everyone.\n");
            for(int i = 0; i < user_count; i++) {
                memset(msg, 0, sizeof(msg));
                msg[0] = '\0';

                strcat(msg, "PRANESIMAS ");
                strcat(msg, username);
                strcat(msg, ": ");
                strncat(msg, buffer, (BUFFER_SIZE * 3) - strlen(msg) - 1);  // Ensure no overflow
                strcat(msg, "\n");

                send(sockets[i], msg, strlen(msg), 0);
            }

            pthread_mutex_unlock(&mutex);
        }
    }

    // Close the client socket and free memory when done
    if(error_first_loop == 0) {
        printf("User(%s) disconnected.\n", username);

        pthread_mutex_lock(&mutex);
        for(int i = 0; i < user_count; i++) {
            if(strcmp(username, users[i]) == 0) {
                shift_left_after_delete(i);
                user_count--;
            }
        }
        pthread_mutex_unlock(&mutex);
    }

    close(client_socket);
    free(client_socket_ptr);
    return NULL;
}


int main(int argc, char const *argv[]) {
    int server_fd4, server_fd6;
    int* client_socket;
    struct sockaddr_in address_ipv4;
    struct sockaddr_in6 address_ipv6;
    int opt = 1;
    socklen_t addrlen4 = sizeof(address_ipv4);
    socklen_t addrlen6 = sizeof(address_ipv6);
    char buffer[1024] = {0};
    int givenPort;
    pthread_t thread_id;

    printf("Enter valid port number: ");
    scanf("%d", &givenPort);


    // Creating socket
    if ((server_fd4 = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Setting option to forcefully, without wait for the OS to release, bind socket to IP 
    if (setsockopt(server_fd4, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        perror("Setsockopt function failed");
        close(server_fd4);
        exit(EXIT_FAILURE);
    }

    // Setting up server's address
    address_ipv4.sin_family = AF_INET;
    address_ipv4.sin_addr.s_addr = inet_addr("127.0.0.1");
    address_ipv4.sin_port = htons(givenPort);

    // Forcefully attaching socket to the provided port
    if (bind(server_fd4, (struct sockaddr *)&address_ipv4, sizeof(address_ipv4)) == -1) {
        perror("Bind failed");
        close(server_fd4);
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd4, 5) == -1) {
        perror("Listen failed");
        close(server_fd4);
        exit(EXIT_FAILURE);
    }

    // Creating socket
    if ((server_fd6 = socket(AF_INET6, SOCK_STREAM, 0)) == -1) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Setting option to forcefully, without wait for the OS to release, bind socket to IP 
    if (setsockopt(server_fd6, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        perror("Setsockopt function failed");
        close(server_fd6);
        exit(EXIT_FAILURE);
    }

    // Disable dual-stack mode for IPv6 socket
    // int ipv6_only = 1;
    // if (setsockopt(server_fd6, IPPROTO_IPV6, IPV6_V6ONLY, &ipv6_only, sizeof(ipv6_only)) == -1) {
    //     perror("setsockopt IPV6_V6ONLY failed");
    //     close(server_fd6);
    //     exit(EXIT_FAILURE);
    // }

    // Setting up server's address
    address_ipv6.sin6_family = AF_INET6;
    // address_ipv6.sin6_addr = in6addr_any;
    inet_pton(AF_INET6, "::1", &(address_ipv6.sin6_addr));
    address_ipv6.sin6_port = htons(givenPort);

    // Forcefully attaching socket to the provided port
    if (bind(server_fd6, (struct sockaddr *)&address_ipv6, sizeof(address_ipv6)) == -1) {
        perror("Bind failed");
        close(server_fd6);
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd6, 5) == -1) {
        perror("Listen failed");
        close(server_fd6);
        exit(EXIT_FAILURE);
    }

    // Set up fd_set for select()
    fd_set readfds;
    int max_fd = server_fd4 > server_fd6 ? server_fd4 : server_fd6;

    printf("Waiting for connections...\n");

    // Accept and handle client connections
    while (1) {
        FD_ZERO(&readfds);
        FD_SET(server_fd4, &readfds); // Add IPv4 socket to fd_set
        FD_SET(server_fd6, &readfds); // Add IPv6 socket to fd_set

        client_socket = malloc(sizeof(int));  // Allocate memory for the client socket
        if (client_socket == NULL) {
            perror("Memory allocation for client socket failed");
            continue;
        }

        struct sockaddr_storage client_addr;
        memset(&client_addr, 0, sizeof(client_addr));

        // Use select to wait for incoming connections on either socket
        int activity = select(max_fd + 1, &readfds, NULL, NULL, NULL);

        if (activity == -1) {
            perror("Select error");
            exit(EXIT_FAILURE);
        }

        if (FD_ISSET(server_fd4, &readfds)) {
            *client_socket = accept(server_fd4, (struct sockaddr *)&client_addr, &addrlen4);
            if (*client_socket == -1) {
                perror("Acception of client failed");
                continue; // Continue to listen for the next connection
            }
        }

        if (FD_ISSET(server_fd6, &readfds)) {
            *client_socket = accept(server_fd6, (struct sockaddr *)&client_addr, &addrlen6);
            if (*client_socket == -1) {
                perror("Acception of client failed");
                continue; // Continue to listen for the next connection
            }
        }

        // Create a new thread to handle the client
        if (pthread_create(&thread_id, NULL, handle_client, (void *)client_socket) != 0) {
            perror("Thread creation failed");
            close(*client_socket);
            free(client_socket);
        }

        // Detach the thread to avoid memory leaks
        pthread_detach(thread_id);
    }

    // Closing the listening(server) socket
    close(server_fd4);
    close(server_fd6);

    return 0;
}