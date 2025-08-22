#include "udp.h"
#include "time_protocol.h"
#include <time.h>
#include <ctype.h>

// Global sub-client registry
struct sub_client clients[MAX_SUB_CLIENTS];
int client_count = 0;

// Function to find or add a sub-client
int register_client(const char *device_id, struct sockaddr_storage *addr, socklen_t addr_len) {
  // Check if client already exists
  for (int i = 0; i < client_count; i++) {
    if (strcmp(clients[i].device_id, device_id) == 0) {
      // Update existing client
      clients[i].address = *addr;
      clients[i].address_len = addr_len;
      clients[i].last_heartbeat = time(NULL);
      clients[i].active = 1;
      printf("SUCCESS: Updated registration for device: %s\n", device_id);
      return i;
    }
  }
  
  // Add new client if space available
  if (client_count < MAX_SUB_CLIENTS) {
    strncpy(clients[client_count].device_id, device_id, DEVICE_ID_SIZE - 1);
    clients[client_count].device_id[DEVICE_ID_SIZE - 1] = '\0';
    clients[client_count].address = *addr;
    clients[client_count].address_len = addr_len;
    clients[client_count].last_heartbeat = time(NULL);
    clients[client_count].active = 1;
    client_count++;
    printf("SUCCESS: Registered new device: %s (Total: %d)\n", device_id, client_count);
    return client_count - 1;
  }
  
  printf("ERROR: Maximum client limit reached\n");
  return -1;
}

// Function to remove inactive clients
void cleanup_inactive_clients() {
  time_t current_time = time(NULL);
  for (int i = 0; i < client_count; i++) {
    if (clients[i].active && 
        (current_time - clients[i].last_heartbeat) > CLIENT_TIMEOUT_SEC) {
      clients[i].active = 0;
      printf("WARNING: Marked client %s as inactive (no heartbeat)\n", 
             clients[i].device_id);
    }
  }
}

// Function to forward request to all active sub-clients
void forward_to_subclients(SOCKET socket_listen, const char *message, int msg_len) {
  printf("Forwarding request to %d registered clients...\n", client_count);
  
  for (int i = 0; i < client_count; i++) {
    if (clients[i].active) {
      int bytes_sent = sendto(socket_listen, message, msg_len, 0,
                            (struct sockaddr *)&clients[i].address, 
                            clients[i].address_len);
      if (bytes_sent < 0) {
        printf("ERROR: Failed to forward to %s\n", clients[i].device_id);
      } else {
        printf("Forwarded %d bytes to %s\n", bytes_sent, clients[i].device_id);
      }
    }
  }
}

int main() {

#if defined(_WIN32)
  WSADATA d;
  if (WSAStartup(MAKEWORD(2, 2), &d)) {
    fprintf(stderr, "Failed to initialize.\n");
    return 1;
  }
#endif

  // Initialize client registry
  memset(clients, 0, sizeof(clients));
  
  printf("Configuring relay server on port %s...\n", RELAY_SERVER_PORT);
  struct addrinfo hints;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_DGRAM;
  hints.ai_flags = AI_PASSIVE;

  struct addrinfo *bind_address;
  if (getaddrinfo(0, RELAY_SERVER_PORT, &hints, &bind_address)) {
    fprintf(stderr, "getaddrinfo() failed. (%d)\n", GETSOCKETERRNO());
    return 1;
  }

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

  printf("[ONLINE] Time Relay Server ready on port %s\n", RELAY_SERVER_PORT);
  printf("Listening on all interfaces (0.0.0.0)\n");
  printf("Supported commands: REGISTER, TIME_REQUEST, LS_REQUEST, HEARTBEAT\n\n");

  // Variables for storing main client info when forwarding
  struct sockaddr_storage main_client_addr;
  socklen_t main_client_len = 0;
  int waiting_for_responses = 0;
  time_t forward_start_time = 0;
  struct time_response responses[MAX_SUB_CLIENTS];
  int response_count = 0;

  while (1) {
    fd_set reads;
    FD_ZERO(&reads);
    FD_SET(socket_listen, &reads);
    
    struct timeval timeout;
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;
    
    if (select(socket_listen + 1, &reads, 0, 0, &timeout) < 0) {
      fprintf(stderr, "select() failed. (%d)\n", GETSOCKETERRNO());
      return 1;
    }

    // Timeout check for unreachable clients
    if (waiting_for_responses) {
      time_t current_time = time(NULL);
      if ((current_time - forward_start_time) >= RESPONSE_TIMEOUT_SEC) {
        printf("\nTimeout reached - some clients may be unreachable\n");
        waiting_for_responses = 0;
        response_count = 0;
      }
    }

    if (FD_ISSET(socket_listen, &reads)) {
      struct sockaddr_storage client_address;
      socklen_t client_len = sizeof(client_address);

      char read[MSG_BUFFER_SIZE];
      int bytes_received =
          recvfrom(socket_listen, read, MSG_BUFFER_SIZE - 1, 0,
                   (struct sockaddr *)&client_address, &client_len);
      if (bytes_received < 1) {
        fprintf(stderr, "recvfrom() failed. (%d)\n", GETSOCKETERRNO());
        continue;
      }
      
      // CRITICAL: Null terminate the received data
      read[bytes_received] = '\0';
      
      // Get client address for logging
      char addr_buffer[ADDRESS_BUFFER_SIZE];
      getnameinfo((struct sockaddr *)&client_address, client_len,
                  addr_buffer, sizeof(addr_buffer), 0, 0, NI_NUMERICHOST);
      
      printf("\nReceived from %s: %s", addr_buffer, read);
      
      // Parse message type
      if (strncmp(read, MSG_REGISTER ":", strlen(MSG_REGISTER ":")) == 0) {
        // Handle registration
        char *device_id = read + strlen(MSG_REGISTER ":");
        // Remove newline if present
        char *newline = strchr(device_id, '\n');
        if (newline) *newline = '\0';
        
        register_client(device_id, &client_address, client_len);
        
        // Send acknowledgment
        const char *ack = "REGISTERED:OK\n";
        sendto(socket_listen, ack, strlen(ack), 0,
               (struct sockaddr *)&client_address, client_len);
               
      } else if (strncmp(read, MSG_TIME_REQUEST, strlen(MSG_TIME_REQUEST)) == 0) {
        // Handle time request from main client
        printf("Processing TIME_REQUEST from main client\n");
        
        // Store main client info
        main_client_addr = client_address;
        main_client_len = client_len;
        waiting_for_responses = 1;
        forward_start_time = time(NULL);
        response_count = 0;
        memset(responses, 0, sizeof(responses));
        
        // Forward to all sub-clients
        forward_to_subclients(socket_listen, read, bytes_received);
        
      } else if (strncmp(read, MSG_LS_REQUEST, strlen(MSG_LS_REQUEST)) == 0) {
        // Handle ls request from main client
        printf("Processing LS_REQUEST from main client\n");
        
        // Store main client info
        main_client_addr = client_address;
        main_client_len = client_len;
        waiting_for_responses = 1;
        forward_start_time = time(NULL);
        response_count = 0;
        
        // Forward to all sub-clients
        forward_to_subclients(socket_listen, read, bytes_received);
        
      } else if (strncmp(read, MSG_TIME_RESPONSE ":", strlen(MSG_TIME_RESPONSE ":")) == 0) {
        // Handle time response from sub-client - forward immediately
        if (waiting_for_responses) {
          char *response_data = read + strlen(MSG_TIME_RESPONSE ":");
          char device_id[DEVICE_ID_SIZE];
          char timestamp[64];
          
          // Parse device_id:timestamp
          if (sscanf(response_data, "%31[^:]:%63s", device_id, timestamp) == 2) {
            // Forward this response immediately to main client
            char single_response[MSG_BUFFER_SIZE];
            snprintf(single_response, sizeof(single_response), 
                    "TIME_RESPONSE:%s:%s\n", device_id, timestamp);
            
            sendto(socket_listen, single_response, strlen(single_response), 0,
                   (struct sockaddr *)&main_client_addr, main_client_len);
            printf("Forwarded response from %s immediately\n", device_id);
            
            response_count++;
            
            // Check if all active clients have responded
            int active_client_count = 0;
            for (int i = 0; i < client_count; i++) {
              if (clients[i].active) active_client_count++;
            }
            
            if (response_count >= active_client_count) {
              printf("All %d clients have responded\n", response_count);
              waiting_for_responses = 0;
              response_count = 0;
            }
          }
        }
        
      } else if (strncmp(read, MSG_LS_RESPONSE ":", strlen(MSG_LS_RESPONSE ":")) == 0) {
        // Handle ls response from sub-client - forward immediately
        if (waiting_for_responses) {
          // Forward the entire ls response to main client
          sendto(socket_listen, read, bytes_received, 0,
                 (struct sockaddr *)&main_client_addr, main_client_len);
          
          // Extract device ID for logging
          char *response_data = read + strlen(MSG_LS_RESPONSE ":");
          char device_id[DEVICE_ID_SIZE];
          if (sscanf(response_data, "%31[^:]", device_id) == 1) {
            printf("Forwarded LS response from %s\n", device_id);
          }
          
          response_count++;
          
          // Check if all active clients have responded
          int active_client_count = 0;
          for (int i = 0; i < client_count; i++) {
            if (clients[i].active) active_client_count++;
          }
          
          if (response_count >= active_client_count) {
            printf("All %d clients have responded to LS request\n", response_count);
            waiting_for_responses = 0;
            response_count = 0;
          }
        }
        
      } else if (strncmp(read, MSG_HEARTBEAT ":", strlen(MSG_HEARTBEAT ":")) == 0) {
        // Handle heartbeat
        char *device_id = read + strlen(MSG_HEARTBEAT ":");
        char *newline = strchr(device_id, '\n');
        if (newline) *newline = '\0';
        
        // Update last heartbeat time
        for (int i = 0; i < client_count; i++) {
          if (strcmp(clients[i].device_id, device_id) == 0) {
            clients[i].last_heartbeat = time(NULL);
            printf("Heartbeat from %s\n", device_id);
            break;
          }
        }
      }
    }
    
    // Periodic cleanup of inactive clients
    static time_t last_cleanup = 0;
    time_t current_time = time(NULL);
    if (current_time - last_cleanup > 30) {
      cleanup_inactive_clients();
      last_cleanup = current_time;
    }
    
  } // while(1)

  printf("Closing listening socket...\n");
  CLOSESOCKET(socket_listen);

#if defined(_WIN32)
  WSACleanup();
#endif

  printf("Finished.\n");
  return 0;
}