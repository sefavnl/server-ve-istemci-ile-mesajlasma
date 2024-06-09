#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <windows.h>

#define PORT 8080
#define MAX_CLIENTS 100
#define BUFFER_SIZE 1024

typedef struct {
    SOCKET sockfd;
    char username[50];
    int online;
    char mood[50];
} client_t;

client_t clients[MAX_CLIENTS];
HANDLE clients_mutex = NULL;

DWORD WINAPI handle_client(LPVOID arg);
void send_message_to_all(char *message);
void send_message_to_client(char *username, char *message);
void list_clients(SOCKET sockfd, char *mask);
void login_client(SOCKET sockfd, char *username, char *password, char *mood);
void logout_client(SOCKET sockfd);
void register_client(SOCKET sockfd, char *username, char *password, char *name, char *surname);
void info_client(SOCKET sockfd, char *username);
void load_user_data();
void save_user_data();

int main() {
    WSADATA wsa;
    SOCKET server_fd, new_sock;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);
    HANDLE tid;

    clients_mutex = CreateMutex(NULL, FALSE, NULL);

    load_user_data();

    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        printf("WSAStartup failed. Error Code: %d\n", WSAGetLastError());
        return 1;
    }

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
        printf("Socket creation failed. Error Code: %d\n", WSAGetLastError());
        return 1;
    }

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt)) == SOCKET_ERROR) {
        printf("setsockopt failed. Error Code: %d\n", WSAGetLastError());
        return 1;
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) == SOCKET_ERROR) {
        printf("Bind failed. Error Code: %d\n", WSAGetLastError());
        return 1;
    }

    if (listen(server_fd, 3) == SOCKET_ERROR) {
        printf("Listen failed. Error Code: %d\n", WSAGetLastError());
        return 1;
    }

    printf("Server started on port %d\n", PORT);

    while (1) {
        if ((new_sock = accept(server_fd, (struct sockaddr *)&address, &addrlen)) == INVALID_SOCKET) {
            printf("Accept failed. Error Code: %d\n", WSAGetLastError());
            return 1;
        }

        WaitForSingleObject(clients_mutex, INFINITE);

        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].sockfd == 0) {
                clients[i].sockfd = new_sock;
                clients[i].online = 0;
                strcpy(clients[i].username, ""); // Ensure username is empty at the start
                tid = CreateThread(NULL, 0, handle_client, &clients[i], 0, NULL);
                if (tid == NULL) {
                    printf("CreateThread failed. Error Code: %d\n", GetLastError());
                }
                break;
            }
        }

        ReleaseMutex(clients_mutex);
    }

    closesocket(server_fd);
    WSACleanup();

    return 0;
}

DWORD WINAPI handle_client(LPVOID arg) {
    char buffer[BUFFER_SIZE];
    int nbytes;
    client_t *cli = (client_t *)arg;

    while ((nbytes = recv(cli->sockfd, buffer, sizeof(buffer), 0)) > 0) {
        buffer[nbytes] = '\0';
        printf("Received command: %s\n", buffer);  // Debug output
        char *command = strtok(buffer, " ");

        if (command == NULL) {
            continue; // Ignore empty commands
        }

        if (strcmp(command, "LIST") == 0) {
            char *mask = strtok(NULL, " ");
            list_clients(cli->sockfd, mask);
        } else if (strcmp(command, "LOGIN") == 0) {
            char *username = strtok(NULL, " ");
            char *password = strtok(NULL, " ");
            char *mood = strtok(NULL, " ");
            if (username != NULL && password != NULL) {
                printf("Processing LOGIN command for user: %s\n", username);  // Debug output
                login_client(cli->sockfd, username, password, mood);
            } else {
                printf("LOGIN command requires both username and password\n");
            }
        } else if (strcmp(command, "LOGOUT") == 0) {
            logout_client(cli->sockfd);
        } else if (strcmp(command, "MSG") == 0) {
            char *username = strtok(NULL, " ");
            char *message = strtok(NULL, "\n");
            if (username != NULL && message != NULL) {
                if (strcmp(username, "*") == 0) {
                    send_message_to_all(message);
                } else {
                    send_message_to_client(username, message);
                }
            }
        } else if (strcmp(command, "INFO") == 0) {
            char *username = strtok(NULL, " ");
            if (username != NULL) {
                info_client(cli->sockfd, username);
            }
        } else if (strcmp(command, "REGISTER") == 0) {
            char *username = strtok(NULL, " ");
            char *password = strtok(NULL, " ");
            char *name = strtok(NULL, " ");
            char *surname = strtok(NULL, " ");
            if (username != NULL && password != NULL && name != NULL && surname != NULL) {
                register_client(cli->sockfd, username, password, name, surname);
            }
        }
    }

    closesocket(cli->sockfd);
    cli->sockfd = 0;
    return 0;
}

void send_message_to_all(char *message) {
    WaitForSingleObject(clients_mutex, INFINITE);

    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].sockfd != 0 && clients[i].online) {
            send(clients[i].sockfd, message, strlen(message), 0);
        }
    }

    ReleaseMutex(clients_mutex);
}

void send_message_to_client(char *username, char *message) {
    WaitForSingleObject(clients_mutex, INFINITE);

    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].sockfd != 0 && clients[i].online && strcmp(clients[i].username, username) == 0) {
            send(clients[i].sockfd, message, strlen(message), 0);
            break;
        }
    }

    ReleaseMutex(clients_mutex);
}

void list_clients(SOCKET sockfd, char *mask) {
    char list[BUFFER_SIZE] = "Users:\n";
    WaitForSingleObject(clients_mutex, INFINITE);

    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].sockfd != 0) {
            char client_info[100];
            sprintf(client_info, "%s (%s) - %s\n", clients[i].username, clients[i].online ? "Online" : "Offline", clients[i].mood);
            strcat(list, client_info);
        }
    }

    ReleaseMutex(clients_mutex);
    send(sockfd, list, strlen(list), 0);
}

void login_client(SOCKET sockfd, char *username, char *password, char *mood) {
    WaitForSingleObject(clients_mutex, INFINITE);

    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].sockfd == sockfd) {
            strcpy(clients[i].username, username);
            if (mood != NULL) {
                strcpy(clients[i].mood, mood);
            } else {
                strcpy(clients[i].mood, "N/A");
            }
            clients[i].online = 1;
            printf("User %s logged in\n", username);
            break;
        }
    }

    ReleaseMutex(clients_mutex);
    save_user_data();
}

void logout_client(SOCKET sockfd) {
    WaitForSingleObject(clients_mutex, INFINITE);

    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].sockfd == sockfd) {
            clients[i].online = 0;
            printf("User %s logged out\n", clients[i].username);
            break;
        }
    }

    ReleaseMutex(clients_mutex);
    save_user_data();
}

void register_client(SOCKET sockfd, char *username, char *password, char *name, char *surname) {
    WaitForSingleObject(clients_mutex, INFINITE);

    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].sockfd == 0) {
            strcpy(clients[i].username, username);
            strcpy(clients[i].mood, "N/A");
            clients[i].online = 0;
            // Register the user details (e.g., write to file)
            printf("User %s registered\n", username);
            break;
        }
    }

    ReleaseMutex(clients_mutex);
    save_user_data();
}

void info_client(SOCKET sockfd, char *username) {
    char info[BUFFER_SIZE] = "User Info:\n";
    WaitForSingleObject(clients_mutex, INFINITE);

    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].sockfd != 0 && strcmp(clients[i].username, username) == 0) {
            sprintf(info + strlen(info), "Name: %s\nMood: %s\n", clients[i].username, clients[i].mood);
            break;
        }
    }

    ReleaseMutex(clients_mutex);
    send(sockfd, info, strlen(info), 0);
}

void load_user_data() {
    // Implement loading user data from file
}

void save_user_data() {
    // Implement saving user data to file
}
