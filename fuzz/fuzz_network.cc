/**
 * Network-based fuzzer for the HTTP server
 * This fuzzer actually sends requests over the network
 */

#include <cstdint>
#include <cstddef>
#include <string>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <thread>
#include <chrono>

const int SERVER_PORT = 18080;  // Use non-standard port for fuzzing

// Start server in a thread
void start_server() {
    // This would normally exec the server
    // For fuzzing, we'd need to link the server code directly
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    // Create socket
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return 0;
    
    // Set timeout to prevent hanging
    struct timeval timeout;
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
    
    // Connect to server
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    
    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        close(sock);
        return 0;
    }
    
    // Send fuzzed data
    send(sock, data, size, 0);
    
    // Try to receive response
    char buffer[4096];
    recv(sock, buffer, sizeof(buffer), 0);
    
    close(sock);
    return 0;
}