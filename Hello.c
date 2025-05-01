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

// Helper function to send HTTP response
void send_response(int client_socket, const char *status, const char *content_type, const char *body, long body_length) {
    char response_header[BUFFER_SIZE];
    int header_length = snprintf(response_header, BUFFER_SIZE,
                                 "HTTP/1.0 %s\r\n"
                                 "Server: C-Simple-Server\r\n"
                                 "Content-Type: %s\r\n"
                                 "Content-Length: %ld\r\n"
                                 "\r\n", // End of headers
                                 status, content_type, body_length);

    // Send header
    send(client_socket, response_header, header_length, 0);

    // Send body (if any)
    if (body != NULL && body_length > 0) {
        send(client_socket, body, body_length, 0);
    }
}

// Helper function to determine MIME type based on file extension (very basic)
const char* get_mime_type(const char* path) {
    const char *dot = strrchr(path, '.'); // Find last dot
    if (!dot || dot == path) return "application/octet-stream"; // No extension or starts with dot
    if (strcmp(dot, ".html") == 0 || strcmp(dot, ".htm") == 0) return "text/html";
    if (strcmp(dot, ".css") == 0) return "text/css";
    if (strcmp(dot, ".js") == 0) return "application/javascript";
    if (strcmp(dot, ".jpg") == 0 || strcmp(dot, ".jpeg") == 0) return "image/jpeg";
    if (strcmp(dot, ".png") == 0) return "image/png";
    if (strcmp(dot, ".gif") == 0) return "image/gif";
    if (strcmp(dot, ".txt") == 0) return "text/plain";
    return "application/octet-stream"; // Default
}

int main() {
    int server_fd, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    char buffer[BUFFER_SIZE];
    char method[16], uri[256], version[16]; // To store parts of the request line
    char file_path[512];

    // 1. Create socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
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
    server_addr.sin_addr.s_addr = INADDR_ANY; // Listen on all available interfaces
    server_addr.sin_port = htons(PORT);       // Convert port to network byte order

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // 3. Listen for incoming connections
    if (listen(server_fd, 10) < 0) { // Backlog of 10 pending connections
        perror("Listen failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d, serving files from %s\n", PORT, DOCUMENT_ROOT);

    // --- Create the document root directory if it doesn't exist ---
    struct stat st = {0};
    if (stat(DOCUMENT_ROOT, &st) == -1) {
        if (mkdir(DOCUMENT_ROOT, 0777) == -1) { // 0777 permissions (adjust if needed)
            perror("Failed to create document root directory");
            // Continue maybe? Or exit? Let's print a warning and continue.
            fprintf(stderr, "Warning: Could not create %s directory.\n", DOCUMENT_ROOT);
        } else {
             printf("Created directory: %s\n", DOCUMENT_ROOT);
             // Optional: Create a default index.html
             FILE *index_file = fopen(DOCUMENT_ROOT "/index.html", "w");
             if (index_file) {
                 fprintf(index_file, "<html><body><h1>Hello from C Server!</h1></body></html>");
                 fclose(index_file);
                 printf("Created default index.html in %s\n", DOCUMENT_ROOT);
             }
        }
    }
    // --- End directory creation ---


    // 4. Accept connections in a loop
    while (1) {
        client_socket = accept(server_fd, (struct sockaddr *)&client_addr, &client_addr_len);
        if (client_socket < 0) {
            perror("Accept failed");
            continue; // Continue to next iteration instead of exiting server
        }

        // Optional: Print client connection info
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
        printf("Connection accepted from %s:%d\n", client_ip, ntohs(client_addr.sin_port));

        // 5. Read client request
        memset(buffer, 0, BUFFER_SIZE); // Clear buffer
        int bytes_received = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
        if (bytes_received < 0) {
            perror("Recv failed");
            close(client_socket);
            continue;
        }
        if (bytes_received == 0) {
            printf("Client disconnected gracefully.\n");
            close(client_socket);
            continue;
        }
        buffer[bytes_received] = '\0'; // Null-terminate the received data
        printf("--- Request Start ---\n%s--- Request End ---\n", buffer); // Log request


        // 6. Parse the HTTP request (very basic: GET /path HTTP/1.x)
        if (sscanf(buffer, "%15s %255s %15s", method, uri, version) < 3) {
            // Malformed request
            const char *body = "<html><body><h1>400 Bad Request</h1></body></html>";
            send_response(client_socket, "400 Bad Request", "text/html", body, strlen(body));
            close(client_socket);
            continue;
        }

        // We only handle GET requests here
        if (strcmp(method, "GET") != 0) {
            const char *body = "<html><body><h1>501 Not Implemented</h1><p>Only GET method is supported.</p></body></html>";
            send_response(client_socket, "501 Not Implemented", "text/html", body, strlen(body));
            close(client_socket);
            continue;
        }

        // Construct the full file path
        // Handle root request "/" -> serve index.html
        if (strcmp(uri, "/") == 0) {
            snprintf(file_path, sizeof(file_path), "%s/index.html", DOCUMENT_ROOT);
        } else {
            // Prevent directory traversal attacks (basic check)
            if (strstr(uri, "..") != NULL) {
                 const char *body = "<html><body><h1>403 Forbidden</h1></body></html>";
                 send_response(client_socket, "403 Forbidden", "text/html", body, strlen(body));
                 close(client_socket);
                 continue;
            }
            snprintf(file_path, sizeof(file_path), "%s%s", DOCUMENT_ROOT, uri);
        }

        printf("Attempting to serve file: %s\n", file_path);

        // 7. Try to open and serve the file
        FILE *file = fopen(file_path, "rb"); // Open in binary read mode
        if (file == NULL) {
            // File not found
            perror("fopen failed"); // Log the specific error
            const char *body = "<html><body><h1>404 Not Found</h1></body></html>";
            printf("Sending 404 Not Found for %s\n", file_path);
            send_response(client_socket, "404 Not Found", "text/html", body, strlen(body));
        } else {
            // File found, get its size
            fseek(file, 0, SEEK_END);
            long file_size = ftell(file);
            fseek(file, 0, SEEK_SET); // Go back to the start

            // Allocate memory for file content
            char *file_content = malloc(file_size);
            if (file_content == NULL) {
                perror("Malloc failed");
                const char *body = "<html><body><h1>500 Internal Server Error</h1></body></html>";
                send_response(client_socket, "500 Internal Server Error", "text/html", body, strlen(body));
                fclose(file);
            } else {
                // Read file content
                if (fread(file_content, 1, file_size, file) != file_size) {
                    perror("fread failed");
                    const char *body = "<html><body><h1>500 Internal Server Error</h1><p>Error reading file.</p></body></html>";
                    send_response(client_socket, "500 Internal Server Error", "text/html", body, strlen(body));
                } else {
                     // Send 200 OK response with file content
                    const char *mime_type = get_mime_type(file_path);
                    printf("Sending 200 OK for %s (%ld bytes, type: %s)\n", file_path, file_size, mime_type);
                    send_response(client_socket, "200 OK", mime_type, file_content, file_size);
                }
                 free(file_content); // Free allocated memory
            }
            fclose(file); // Close the file
        }

        // 8. Close the client connection
        close(client_socket);
        printf("Connection closed for %s:%d\n\n", client_ip, ntohs(client_addr.sin_port));
    }

    // 9. Close server socket (won't be reached in this infinite loop)
    close(server_fd);
    printf("Server shutting down.\n"); // You'd need signal handling for graceful shutdown

    return 0;
}