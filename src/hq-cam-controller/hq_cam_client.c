#include "tcp.h"
#include <stdlib.h>

#define BUFFER_SIZE 4096
#define ADDRESS_BUFFER_SIZE 100
#define SERVICE_BUFFER_SIZE 100
#define SELECT_TIMEOUT_US 100000

// Function to configure remote address
struct addrinfo *configureRemoteAddress(const char *hostname,
                                        const char *port) {
  printf("Configuring remote address...\n");

  struct addrinfo hints;
  memset(&hints, 0, sizeof(hints));
  hints.ai_socktype = SOCK_STREAM;

  struct addrinfo *peer_address;
  if (getaddrinfo(hostname, port, &hints, &peer_address)) {
    fprintf(stderr, "getaddrinfo() failed. (%d)\n", GETSOCKETERRNO());
    return NULL;
  }

  return peer_address;
}

// Function to display remote address information
void displayRemoteAddress(struct addrinfo *peer_address) {
  printf("Remote address is: ");

  char address_buffer[ADDRESS_BUFFER_SIZE];
  char service_buffer[SERVICE_BUFFER_SIZE];

  getnameinfo(peer_address->ai_addr, peer_address->ai_addrlen, address_buffer,
              sizeof(address_buffer), service_buffer, sizeof(service_buffer),
              NI_NUMERICHOST);

  printf("%s %s\n", address_buffer, service_buffer);
}

// Function to create socket
SOCKET createSocket(struct addrinfo *peer_address) {
  printf("Creating socket...\n");

  SOCKET socket_peer =
      socket(peer_address->ai_family, peer_address->ai_socktype,
             peer_address->ai_protocol);

  if (!ISVALIDSOCKET(socket_peer)) {
    fprintf(stderr, "socket() failed. (%d)\n", GETSOCKETERRNO());
    return -1;
  }

  return socket_peer;
}

// Function to connect to server
int connectToServer(SOCKET socket_peer, struct addrinfo *peer_address) {
  printf("Connecting...\n");

  if (connect(socket_peer, peer_address->ai_addr, peer_address->ai_addrlen)) {
    fprintf(stderr, "connect() failed. (%d)\n", GETSOCKETERRNO());
    return -1;
  }

  printf("Connected.\n");
  return 0;
}

// Function to handle incoming data from server
int handleServerData(SOCKET socket_peer) {
  char buffer[BUFFER_SIZE];
  int bytes_received = recv(socket_peer, buffer, BUFFER_SIZE, 0);

  if (bytes_received < 1) {
    printf("Connection closed by peer.\n");
    return -1;
  }

  printf("Received (%d bytes): %.*s", bytes_received, bytes_received, buffer);
  return 0;
}

// Function to handle user input and send to server
int handleUserInput(SOCKET socket_peer) {
  char buffer[BUFFER_SIZE];

  if (!fgets(buffer, BUFFER_SIZE, stdin))
    return -1;

  printf("Sending: %s", buffer);
  int bytes_sent = send(socket_peer, buffer, strlen(buffer), 0);
  printf("Sent %d bytes.\n", bytes_sent);

  return 0;
}

// Function to handle the main communication loop
void communicationLoop(SOCKET socket_peer) {
  printf("To send data, enter text followed by enter.\n");

  while (1) {
    fd_set reads;
    FD_ZERO(&reads);
    FD_SET(socket_peer, &reads);
    FD_SET(0, &reads); // stdin

    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = SELECT_TIMEOUT_US;

    if (select(socket_peer + 1, &reads, 0, 0, &timeout) < 0) {
      fprintf(stderr, "select() failed. (%d)\n", GETSOCKETERRNO());
      break;
    }

    // Check for data from server
    if (FD_ISSET(socket_peer, &reads)) {
      if (handleServerData(socket_peer) < 0)
        break;
    }

    // Check for user input
    if (FD_ISSET(0, &reads)) {
      if (handleUserInput(socket_peer) < 0)
        break;
    }
  }
}

// Function to cleanup and close socket
void cleanup(SOCKET socket_peer) {
  printf("Closing socket...\n");
  CLOSESOCKET(socket_peer);
  printf("Finished.\n");
}

int main(int argc, char *argv[]) {
  // Validate command line arguments
  if (argc < 3) {
    fprintf(stderr, "usage: tcp_client hostname port\n");
    return 1;
  }

  // Configure remote address
  struct addrinfo *peer_address = configureRemoteAddress(argv[1], argv[2]);
  if (!peer_address) {
    return 1;
  }

  // Display address information
  displayRemoteAddress(peer_address);

  // Create socket
  SOCKET socket_peer = createSocket(peer_address);
  if (socket_peer == -1) {
    freeaddrinfo(peer_address);
    return 1;
  }

  // Connect to server
  if (connectToServer(socket_peer, peer_address) < 0) {
    CLOSESOCKET(socket_peer);
    freeaddrinfo(peer_address);
    return 1;
  }

  // Free address info after connection
  freeaddrinfo(peer_address);

  // Main communication loop
  communicationLoop(socket_peer);

  // Cleanup
  cleanup(socket_peer);

  return 0;
}
