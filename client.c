#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <windows.h>

#define PORT 8080
#define BUFFER_SIZE 1024

DWORD WINAPI receive_messages(LPVOID sockfd);

int main() {
    WSADATA wsa;
    SOCKET sockfd;
    struct sockaddr_in server_addr;
    char buffer[BUFFER_SIZE];
    HANDLE tid;

    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        printf("WSAStartup failed. Error Code: %d\n", WSAGetLastError());
        return 1;
    }

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
        printf("Socket creation error. Error Code: %d\n", WSAGetLastError());
        return 1;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        printf("Connection failed. Error Code: %d\n", WSAGetLastError());
        return 1;
    }

    printf("Connected to server\n");

    tid = CreateThread(NULL, 0, receive_messages, &sockfd, 0, NULL);

    while (1) {
        fgets(buffer, BUFFER_SIZE, stdin);
        send(sockfd, buffer, strlen(buffer), 0);
    }

    closesocket(sockfd);
    WSACleanup();

    return 0;
}

DWORD WINAPI receive_messages(LPVOID sockfd) {
    SOCKET sock = *((SOCKET *)sockfd);
    char buffer[BUFFER_SIZE];
    int nbytes;

    while ((nbytes = recv(sock, buffer, sizeof(buffer), 0)) > 0) {
        buffer[nbytes] = '\0';
        printf("%s", buffer);
    }

    return 0;
}
