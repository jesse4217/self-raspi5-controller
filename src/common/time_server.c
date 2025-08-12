#if defined(_WIN32)
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")

#else
#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#endif

/** Preprocessor Macros **/
#if defined(_WIN32)
#define ISVALIDSOCKET(s) ((s) != INVALID_SOCKET)
#define CLOSESOCKET(s) closesocket(s)
#define GETSOCKETERRNO() (WSAGetLastError())

#else
#define ISVALIDSOCKET(s) ((s) >= 0) // non-negative indicates valid
#define CLOSESOCKET(s) close(s)     // close a socket function
#define SOCKET int                  // define data type
#define GETSOCKETERRNO() (errno)    // get last socket error function
#endif

#include <stdio.h>
#include <string.h>
#include <time.h>

int main(int argc, char *argv[]) {

#if defined(_WIN32)
  WSADATA d;
  if (WSAStartup(MAKEWORD(2, 2), &d)) {
    fprintf(stderr, "Failed to initialize.\n");
    return 1;
  }
#endif

  /** Configure Local Address for Time Server **/
  printf("Configuring local address for time server...\n");
  struct addrinfo hints;            // for configuring preferences
  memset(&hints, 0, sizeof(hints)); // set memory
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM; // SOCK_STREAM = TCP, SOCK_DGRAM = UDP
  hints.ai_flags =
      AI_PASSIVE; // bind to any available network interface" (0.0.0.0) without
                  // this flag, it would only bind to localhost

  struct addrinfo *bind_address; // for store the result

  getaddrinfo(0, "8080", &hints,
              &bind_address); // `8080` used for HTTP traffic; resolve
                              // infomation for binding

  printf("Creating socket...\n");
  SOCKET socket_listen;
  socket_listen = socket(bind_address->ai_family, bind_address->ai_socktype,
                         bind_address->ai_protocol);

  if (!ISVALIDSOCKET(socket_listen)) {
    fprintf(stderr, "socket() failed. (%d)\n", GETSOCKETERRNO());
    return 1;
  }

  printf("Binding socket to local address...\n");
  if (bind(socket_listen, bind_address->ai_addr, bind_address->ai_addrlen)) {
    fprintf(stderr, "bind() failed. (%d)\n", GETSOCKETERRNO());
    return 1;
  }

  freeaddrinfo(bind_address);

  printf("Listening...\n");
  if (listen(socket_listen, 10) < 0) {
    fprintf(stderr, "listen() failed. (%d)\n", GETSOCKETERRNO());
    return 1;
  }

  printf("Waiting for connection...\n");
  struct sockaddr_storage client_address;
  socklen_t client_len = sizeof(client_address);
  SOCKET socket_client =
      accept(socket_listen, (struct sockaddr *)&client_address, &client_len);
  if (!ISVALIDSOCKET(socket_client)) {
    fprintf(stderr, "accept() failed. (%d)\n", GETSOCKETERRNO());
    return 1;
  }

  printf("Client is connected... ");
  char address_buffer[100];
  getnameinfo((struct sockaddr *)&client_address, client_len, address_buffer,
              sizeof(address_buffer), 0, 0, NI_NUMERICHOST);
  printf("%s\n", address_buffer);

  printf("Reading request...\n");
  char request[1024];
  int bytes_received = recv(socket_client, request, 1024, 0);
  printf("Received %d bytes.\n", bytes_received);
  // printf("%.*s", bytes_received, request);

  printf("Sending response...\n");
  const char *response = "HTTP/1.1 200 OK\r\n"
                         "Connection: close\r\n"
                         "Content-Type: text/plain\r\n\r\n"
                         "Local time is: ";
  int bytes_sent = send(socket_client, response, strlen(response), 0);
  printf("Sent %d of %d bytes.\n", bytes_sent, (int)strlen(response));

  time_t timer;
  time(&timer);
  char *time_msg = ctime(&timer);
  bytes_sent = send(socket_client, time_msg, strlen(time_msg), 0);
  printf("Sent %d of %d bytes.\n", bytes_sent, (int)strlen(time_msg));

  printf("Closing connection...\n");
  CLOSESOCKET(socket_client);

  printf("Closing listening socket...\n");
  CLOSESOCKET(socket_listen);

#if defined(_WIN32)
  WSACleanup();
#endif

  printf("Finished.\n");

  return 0;
}

/*
 * Test with `curl http://localhost:8080`
 *
 *
 * */
