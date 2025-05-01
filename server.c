#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h> // For O_RDONLY
#include <sys/stat.h> // For stat()

#define PORT 8080
#define BUFFER_SIZE 1024
#define DOCUMENT_ROOT "./www" // Directory to serve files from

int main(){
    printf("Hello World\n");
    int server_fd, client_socket;
    struct sockaddr_in server_addr,client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    char buffer[BUFFER_SIZE];
    char method[16],uri[256],version[16]; // to store parts of the request line
    char file_path[512];

/*
server_fd : server file discriptor
socket() creates endpoint for communicaion always three integer arguments;
AF_NET : Address Family - Internet , this indicates socket will use IPV4 protocol
SOCK_STREAM : stream socket 
protocol: 0 , AF_INET and SOCK_STREAM, the default and standard protocol is TCP.
int opt = 1; : This line declares an integer variable opt and sets its value to 1
setsockopt(...): This is the system call used to manipulate options for the socket.
SOL_SOCKET means the option is at the general socket layer itself, not specific to a particular protocol like TCP or IP.


*/

    // 1. Create socket
    server_fd = socket(AF_INET,SOCK_STREAM,0);
    if (server_fd < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Optional: Set socket options (allow reuse of address)
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
       perror("setsockopt(SO_REUSEADDR) failed");
       // Non-fatal, but good practice
    }

    // 2. Bind socket to address and port
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY; //Listen on all available interfaces
    server_addr.sin_port = htons(PORT);

    if(bind(server_fd,(struct sockaddr *)&server_addr, sizeof(server_addr))<0){
        perror("Bind Failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    return 0;
}