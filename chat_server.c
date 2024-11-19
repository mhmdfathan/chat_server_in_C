#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <errno.h>

#define MAX_CLIENTS 3
#define BUFFER_SIZE 1024
#define PORT 8080

// Structure to store client information
typedef struct {
    int socket;
    char id[50];
} Client;

Client clients[MAX_CLIENTS];

int main() {
    int server_fd, new_socket, activity;
    int max_sd, sd, valread;
    struct sockaddr_in address;
    fd_set readfds;
    char buffer[BUFFER_SIZE];
    char message[BUFFER_SIZE + 50]; // For formatted messages
    int opt = 1;
    int addrlen = sizeof(address);

    // Initialize client structures
    for (int i = 0; i < MAX_CLIENTS; i++) {
        clients[i].socket = 0;
    }

    // Create server socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket failed");
        exit(EXIT_FAILURE);
    }

    // Set socket options
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("Setsockopt failed");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = inet_addr("127.0.0.1");
    address.sin_port = htons(PORT);

    // Bind the socket
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    // Listen for connections
    if (listen(server_fd, 3) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    printf("Listening on port %d\n", PORT);

    // Server loop
    while (1) {
        FD_ZERO(&readfds);
        FD_SET(server_fd, &readfds);
        max_sd = server_fd;

        // Add client sockets to the read set
        for (int i = 0; i < MAX_CLIENTS; i++) {
            sd = clients[i].socket;
            if (sd > 0) FD_SET(sd, &readfds);
            if (sd > max_sd) max_sd = sd;
        }

        // Monitor sockets
        activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);
        if ((activity < 0) && (errno != EINTR)) {
            perror("Select error");
            exit(EXIT_FAILURE);
        }

        // Handle new connections
        if (FD_ISSET(server_fd, &readfds)) {
            if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0) {
                perror("Accept failed");
                exit(EXIT_FAILURE);
            }

            printf("New connection, socket fd is %d, ip is : %s, port : %d\n",
                   new_socket, inet_ntoa(address.sin_addr), ntohs(address.sin_port));

            // Receive client ID
            char client_id[50];
            valread = read(new_socket, client_id, sizeof(client_id));
            client_id[valread] = '\0';

            // Notify all clients of the new connection
            snprintf(message, sizeof(message), "Server: New client [%s] connected from %s:%d\n", 
                     client_id, inet_ntoa(address.sin_addr), ntohs(address.sin_port));
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (clients[i].socket > 0) {
                    send(clients[i].socket, message, strlen(message), 0);
                }
            }

            // Add new socket to array
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (clients[i].socket == 0) {
                    clients[i].socket = new_socket;
                    strcpy(clients[i].id, client_id); 
                    printf("Adding to list of sockets as %d\n", i);
                    break;
                }
            }
        }

        // Handle client messages
        for (int i = 0; i < MAX_CLIENTS; i++) {
            sd = clients[i].socket;
            if (FD_ISSET(sd, &readfds)) {
                if ((valread = read(sd, buffer, BUFFER_SIZE)) == 0) {
                    getpeername(sd, (struct sockaddr *)&address, (socklen_t *)&addrlen);
                    printf("Client [%s] disconnected, ip %s, port %d\n", 
                           clients[i].id, inet_ntoa(address.sin_addr), ntohs(address.sin_port));

                    // Notify all clients of disconnection
                    snprintf(message, sizeof(message), "Server: Client [%s] from %s:%d has disconnected\n",
                             clients[i].id, inet_ntoa(address.sin_addr), ntohs(address.sin_port));
                    for (int j = 0; j < MAX_CLIENTS; j++) {
                        if (clients[j].socket > 0) {
                            send(clients[j].socket, message, strlen(message), 0);
                        }
                    }

                    // Close and reset client socket
                    close(sd);
                    clients[i].socket = 0;
                } else {
                    buffer[valread] = '\0';

                    if (strcmp(buffer, "GET_CLIENTS") == 0) {
                        char client_list[BUFFER_SIZE] = "Connected clients: ";
                        for (int j = 0; j < MAX_CLIENTS; j++) {
                            if (clients[j].socket > 0) {
                                strcat(client_list, clients[j].id);
                                strcat(client_list, ", ");
                            }
                        }
                        // Remove trailing comma and space
                        if (strlen(client_list) > 18) { 
                            client_list[strlen(client_list) - 2] = '\0';
                        }
                        send(sd, client_list, strlen(client_list), 0);
                    } else {
                        // Broadcast the message to all other clients, but not to the sender
                        snprintf(message, sizeof(message), "[%s]: %s\n", clients[i].id, buffer);
                        for (int j = 0; j < MAX_CLIENTS; j++) {
                            if (clients[j].socket > 0 && j != i) {
                                send(clients[j].socket, message, strlen(message), 0);
                            }
                        }
                    }
                }
            }
        }
    }

    return 0;
}
