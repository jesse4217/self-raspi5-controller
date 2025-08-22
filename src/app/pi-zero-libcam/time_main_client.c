#include "udp.h"
#include "time_protocol.h"
#include <time.h>

void print_timestamp() {
  time_t now = time(NULL);
  char time_buffer[26];
  struct tm *tm_info = localtime(&now);
  strftime(time_buffer, 26, "%Y-%m-%d %H:%M:%S", tm_info);
  printf("[%s] ", time_buffer);
}

int main(int argc, char *argv[]) {

  if (argc < 2) {
    fprintf(stderr, "usage: %s relay_server_hostname [port]\n", argv[0]);
    fprintf(stderr, "example: %s 192.168.1.100 8080\n", argv[0]);
    return 1;
  }
  
  const char *hostname = argv[1];
  const char *port = (argc >= 3) ? argv[2] : RELAY_SERVER_PORT;

  printf("Configuring relay server address...\n");
  struct addrinfo hints;
  memset(&hints, 0, sizeof(hints));
  hints.ai_socktype = SOCK_DGRAM;
  struct addrinfo *peer_address;
  if (getaddrinfo(hostname, port, &hints, &peer_address)) {
    fprintf(stderr, "getaddrinfo() failed. (%d)\n", GETSOCKETERRNO());
    return 1;
  }

  printf("Relay server address is: ");
  char address_buffer[ADDRESS_BUFFER_SIZE];
  char service_buffer[100];
  getnameinfo(peer_address->ai_addr, peer_address->ai_addrlen, address_buffer,
              sizeof(address_buffer), service_buffer, sizeof(service_buffer),
              NI_NUMERICHOST);
  printf("%s port %s\n", address_buffer, service_buffer);

  printf("Creating socket...\n");
  SOCKET socket_peer;
  socket_peer = socket(peer_address->ai_family, peer_address->ai_socktype,
                       peer_address->ai_protocol);
  if (!ISVALIDSOCKET(socket_peer)) {
    fprintf(stderr, "socket() failed. (%d)\n", GETSOCKETERRNO());
    return 1;
  }

  printf("[ONLINE] Time Request Client ready.\n");
  printf("Commands:\n");
  printf("  time    - Request time from all sub-clients\n");
  printf("  status  - Show connection status\n");
  printf("  quit    - Exit program\n\n");

  while (1) {
    fd_set reads;
    FD_ZERO(&reads);
    FD_SET(socket_peer, &reads);
    FD_SET(0, &reads);  // stdin

    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 100000;  // 100ms

    if (select(socket_peer + 1, &reads, 0, 0, &timeout) < 0) {
      fprintf(stderr, "select() failed. (%d)\n", GETSOCKETERRNO());
      return 1;
    }

    // Check for server responses
    if (FD_ISSET(socket_peer, &reads)) {
      char read[MSG_BUFFER_SIZE];
      struct sockaddr_storage sender_address;
      socklen_t sender_len = sizeof(sender_address);
      
      int bytes_received = recvfrom(socket_peer, read, MSG_BUFFER_SIZE - 1, 0,
                                   (struct sockaddr *)&sender_address, &sender_len);
      if (bytes_received < 1) {
        printf("\nERROR: Failed to receive response\n");
      } else {
        // Null terminate
        read[bytes_received] = '\0';
        
        // Parse individual response
        if (strncmp(read, "TIME_RESPONSE:", 14) == 0) {
          char *response_data = read + 14;
          char device_id[DEVICE_ID_SIZE];
          char timestamp[64];
          
          if (sscanf(response_data, "%31[^:]:%63s", device_id, timestamp) == 2) {
            print_timestamp();
            printf("[%s] Time: %s\n", device_id, timestamp);
          }
        } else if (strncmp(read, "TIME_RESPONSES:", 15) == 0) {
          // Handle old aggregated format (for compatibility)
          print_timestamp();
          printf("Response from server:\n");
          char *line = strchr(read, '\n');
          if (line) {
            line++;  // Skip past newline
            
            // Parse each device response
            while (*line) {
              char device_id[DEVICE_ID_SIZE];
              char timestamp[64];
              char *next_line = strchr(line, '\n');
              
              if (next_line) {
                *next_line = '\0';
                if (sscanf(line, "%31[^:]:%63s", device_id, timestamp) == 2) {
                  printf("  [%s] Time: %s\n", device_id, timestamp);
                }
                line = next_line + 1;
              } else {
                break;
              }
            }
          }
        } else {
          print_timestamp();
          printf("%s", read);
        }
        printf("\n");
      }
    }
    
    // Check for user input
    if (FD_ISSET(0, &reads)) {
      char input[256];
      if (!fgets(input, sizeof(input), stdin)) {
        break;
      }
      
      // Remove newline
      size_t len = strlen(input);
      if (len > 0 && input[len-1] == '\n') {
        input[len-1] = '\0';
      }
      
      if (strcmp(input, "quit") == 0) {
        printf("Exiting...\n");
        break;
      } else if (strcmp(input, "time") == 0) {
        // Send time request
        const char *request = MSG_TIME_REQUEST "\n";
        
        print_timestamp();
        printf("Sending TIME_REQUEST to relay server...\n");
        
        int bytes_sent = sendto(socket_peer, request, strlen(request), 0,
                              peer_address->ai_addr, peer_address->ai_addrlen);
        if (bytes_sent < 0) {
          printf("ERROR: Failed to send request. (%d)\n", GETSOCKETERRNO());
        } else {
          printf("Request sent (%d bytes). Waiting for responses...\n", bytes_sent);
        }
      } else if (strcmp(input, "status") == 0) {
        print_timestamp();
        printf("Connected to relay server at %s:%s\n", address_buffer, service_buffer);
        printf("Socket: %d\n", socket_peer);
      } else if (strlen(input) > 0) {
        printf("Unknown command: %s\n", input);
        printf("Valid commands: time, status, quit\n");
      }
    }
  } // end while(1)

  printf("Closing socket...\n");
  CLOSESOCKET(socket_peer);
  freeaddrinfo(peer_address);

  printf("Finished.\n");
  return 0;
}