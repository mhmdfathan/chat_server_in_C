#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>

#define BUFFER_SIZE 1024

int main() {
    int sock;
    struct sockaddr_in server_addr;
    char message[BUFFER_SIZE], buffer[BUFFER_SIZE];
    fd_set readfds;
    char client_id[50]; // To store the client identifier

    // Prompt for client identifier
    printf("Enter your client ID: ");
    fgets(client_id, sizeof(client_id), stdin);
    client_id[strcspn(client_id, "\n")] = '\0'; // Remove newline character

    // Create socket
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        perror("Socket creation failed");
        return 1;
    }

    // Define server address
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(8080);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    // Connect to server
    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("Connection failed");
        return 1;
    }

    // Send client ID to server
    send(sock, client_id, strlen(client_id), 0);

    printf("Connected to the server at 127.0.0.1:8080. You can now start chatting.\nYou [%s]: ", client_id);
    fflush(stdout);

    while (1) {
        FD_ZERO(&readfds);
        FD_SET(0, &readfds);    // Monitor stdin (user input)
        FD_SET(sock, &readfds); // Monitor server socket

        int activity = select(sock + 1, &readfds, NULL, NULL, NULL);

        if (activity < 0) {
            perror("Select error");
            continue;
        }

        // Check for user input (keyboard input)
        if (FD_ISSET(0, &readfds)) {
            fgets(message, BUFFER_SIZE, stdin);
            message[strcspn(message, "\n")] = '\0'; // Remove newline character

            // If user types "GET_CLIENTS", request the list of connected clients
            if (strcmp(message, "GET_CLIENTS") == 0) {
                send(sock, message, strlen(message), 0);
            } else if (strlen(message) > 0) { // Only send if the message is not empty
                send(sock, message, strlen(message), 0);
            } 

            printf("You [%s]: ", client_id); // Print client prompt after sending (or after checking for empty message)
            fflush(stdout); 
        }

        // Check for messages from the server
        if (FD_ISSET(sock, &readfds)) {
            int valread = read(sock, buffer, BUFFER_SIZE);
            if (valread == 0) {
                printf("Server disconnected. Exiting...\n");
                close(sock);
                break;
            } else {
                buffer[valread] = '\0'; 
                
                // Trim trailing newline characters from the received message
                int len = strlen(buffer);
                while (len > 0 && (buffer[len - 1] == '\n' || buffer[len - 1] == '\r')) {
                    buffer[len - 1] = '\0';
                    len--;
                }

                // Check if the trimmed message starts with the client's ID
                if (strncmp(buffer, client_id, strlen(client_id)) == 0) { 
                    printf("\nServer: %s", buffer); 
                } else if (strlen(buffer) > 0) { // Otherwise, print "Server:" and the message, then the prompt
                    printf("\nServer: %s", buffer);
                    printf("\nYou [%s]: ", client_id); 
                } 

                fflush(stdout);
            }
        }  // This closing brace was missing
    } 

    return 0;
}
