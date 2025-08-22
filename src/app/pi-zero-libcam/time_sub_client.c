#include "time_protocol.h"
#include "udp.h"
#include <signal.h>
#include <stdlib.h>
#include <time.h>

// Global variables for graceful shutdown
static volatile int keep_running = 1;
static SOCKET global_socket = -1;

void signal_handler(int sig) {
  printf("\nReceived signal %d, shutting down...\n", sig);
  keep_running = 0;
}

void get_current_time_string(char *buffer, size_t size) {
  time_t now = time(NULL);
  struct tm *tm_info = localtime(&now);
  strftime(buffer, size, "%Y-%m-%d %H:%M:%S", tm_info);
}

int main(int argc, char *argv[]) {

  if (argc < 3) {
    fprintf(stderr, "usage: %s device_id relay_server_hostname [port]\n",
            argv[0]);
    fprintf(stderr, "example: %s PiZero-01 192.168.1.100 8080\n", argv[0]);
    return 1;
  }

  const char *device_id = argv[1];
  const char *hostname = argv[2];
  const char *port = (argc >= 4) ? argv[3] : RELAY_SERVER_PORT;

  // Validate device_id length
  if (strlen(device_id) >= DEVICE_ID_SIZE) {
    fprintf(stderr, "ERROR: Device ID too long (max %d characters)\n",
            DEVICE_ID_SIZE - 1);
    return 1;
  }

  // Set up signal handlers for graceful shutdown
  signal(SIGINT, signal_handler);
  signal(SIGTERM, signal_handler);

#if defined(_WIN32)
  WSADATA d;
  if (WSAStartup(MAKEWORD(2, 2), &d)) {
    fprintf(stderr, "Failed to initialize.\n");
    return 1;
  }
#endif

  printf("Configuring relay server address...\n");
  struct addrinfo hints;
  memset(&hints, 0, sizeof(hints));
  hints.ai_socktype = SOCK_DGRAM;
  struct addrinfo *relay_address;
  if (getaddrinfo(hostname, port, &hints, &relay_address)) {
    fprintf(stderr, "getaddrinfo() failed. (%d)\n", GETSOCKETERRNO());
    return 1;
  }

  printf("Relay server: ");
  char address_buffer[ADDRESS_BUFFER_SIZE];
  char service_buffer[100];
  getnameinfo(relay_address->ai_addr, relay_address->ai_addrlen, address_buffer,
              sizeof(address_buffer), service_buffer, sizeof(service_buffer),
              NI_NUMERICHOST);
  printf("%s port %s\n", address_buffer, service_buffer);

  printf("Creating socket...\n");
  SOCKET socket_peer;
  socket_peer = socket(relay_address->ai_family, relay_address->ai_socktype,
                       relay_address->ai_protocol);
  if (!ISVALIDSOCKET(socket_peer)) {
    fprintf(stderr, "socket() failed. (%d)\n", GETSOCKETERRNO());
    return 1;
  }
  global_socket = socket_peer;

  // Register with relay server
  printf("Registering with relay server as '%s'...\n", device_id);
  char register_msg[MSG_BUFFER_SIZE];
  snprintf(register_msg, sizeof(register_msg), "%s:%s\n", MSG_REGISTER,
           device_id);

  int bytes_sent = sendto(socket_peer, register_msg, strlen(register_msg), 0,
                          relay_address->ai_addr, relay_address->ai_addrlen);
  if (bytes_sent < 0) {
    fprintf(stderr, "Failed to send registration. (%d)\n", GETSOCKETERRNO());
    CLOSESOCKET(socket_peer);
    return 1;
  }

  // Wait for registration acknowledgment
  char ack_buffer[MSG_BUFFER_SIZE];
  struct sockaddr_storage sender_addr;
  socklen_t sender_len = sizeof(sender_addr);

  // Set timeout for registration ACK
  struct timeval tv;
  tv.tv_sec = 5;
  tv.tv_usec = 0;
  setsockopt(socket_peer, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv,
             sizeof(tv));

  int ack_received = recvfrom(socket_peer, ack_buffer, MSG_BUFFER_SIZE - 1, 0,
                              (struct sockaddr *)&sender_addr, &sender_len);
  if (ack_received > 0) {
    ack_buffer[ack_received] = '\0';
    printf("Registration response: %s", ack_buffer);
  } else {
    printf("WARNING: No registration acknowledgment received\n");
  }

  // Reset timeout for normal operation
  tv.tv_sec = 0;
  tv.tv_usec = 100000; // 100ms
  setsockopt(socket_peer, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv,
             sizeof(tv));

  printf("[ONLINE] Sub-client '%s' ready. Listening for time requests...\n\n",
         device_id);

  time_t last_heartbeat = time(NULL);

  while (keep_running) {
    fd_set reads;
    FD_ZERO(&reads);
    FD_SET(socket_peer, &reads);

    struct timeval timeout;
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;

    int ready = select(socket_peer + 1, &reads, 0, 0, &timeout);
    if (ready < 0) {
      if (keep_running) { // Only print error if not shutting down
        fprintf(stderr, "select() failed. (%d)\n", GETSOCKETERRNO());
      }
      break;
    }

    // Handle incoming messages
    if (ready > 0 && FD_ISSET(socket_peer, &reads)) {
      char read[MSG_BUFFER_SIZE];
      struct sockaddr_storage sender_address;
      socklen_t sender_len = sizeof(sender_address);

      int bytes_received =
          recvfrom(socket_peer, read, MSG_BUFFER_SIZE - 1, 0,
                   (struct sockaddr *)&sender_address, &sender_len);
      if (bytes_received > 0) {
        // Null terminate
        read[bytes_received] = '\0';

        char sender_addr_str[ADDRESS_BUFFER_SIZE];
        getnameinfo((struct sockaddr *)&sender_address, sender_len,
                    sender_addr_str, sizeof(sender_addr_str), 0, 0,
                    NI_NUMERICHOST);

        printf("Received from %s: %s", sender_addr_str, read);

        // Check if it's a time request
        if (strncmp(read, MSG_TIME_REQUEST, strlen(MSG_TIME_REQUEST)) == 0) {
          // Generate time response
          char time_str[64];
          get_current_time_string(time_str, sizeof(time_str));

          char response[MSG_BUFFER_SIZE];
          snprintf(response, sizeof(response), "%s:%s:%s\n", MSG_TIME_RESPONSE,
                   device_id, time_str);

          // Send response back through relay (it came from relay)
          int resp_sent =
              sendto(socket_peer, response, strlen(response), 0,
                     (struct sockaddr *)&sender_address, sender_len);
          if (resp_sent < 0) {
            printf("ERROR: Failed to send time response\n");
          } else {
            printf("Sent time response: %s", response);
          }
        } else if (strncmp(read, MSG_LS_REQUEST, strlen(MSG_LS_REQUEST)) == 0) {
          // Execute ls command
          printf("Executing ls command...\n");

          // Use popen to execute ls and capture output
          FILE *fp = popen("ls -la", "r");
          if (fp == NULL) {
            printf("ERROR: Failed to execute ls command\n");
          } else {
            char ls_output[MSG_BUFFER_SIZE - 100]; // Leave space for header
            char line[256];
            int offset = 0;

            // Read ls output
            while (fgets(line, sizeof(line), fp) != NULL &&
                   offset < (MSG_BUFFER_SIZE - 200)) {
              int line_len = strlen(line);
              if (offset + line_len < MSG_BUFFER_SIZE - 200) {
                memcpy(ls_output + offset, line, line_len);
                offset += line_len;
              } else {
                break; // Buffer full
              }
            }
            ls_output[offset] = '\0';
            pclose(fp);

            // Send ls response
            char response[MSG_BUFFER_SIZE];
            snprintf(response, sizeof(response), "%s:%s:\n%s", MSG_LS_RESPONSE,
                     device_id, ls_output);

            int resp_sent =
                sendto(socket_peer, response, strlen(response), 0,
                       (struct sockaddr *)&sender_address, sender_len);
            if (resp_sent < 0) {
              printf("ERROR: Failed to send ls response\n");
            } else {
              printf("Sent ls response (%d bytes)\n", resp_sent);
            }
          }
        } else if (strncmp(read, MSG_CAMERA_REQUEST,
                           strlen(MSG_CAMERA_REQUEST)) == 0) {
          // Execute libcamera-still command
          printf("Executing camera capture...\n");

          // Generate filename with timestamp
          char filename[256];
          time_t now = time(NULL);
          struct tm *tm_info = localtime(&now);
          strftime(filename, sizeof(filename), "%Y%m%d_%H%M%S.png", tm_info);

          // Build the libcamera-still command
          char camera_cmd[512];
          snprintf(camera_cmd, sizeof(camera_cmd),
                   "libcamera-still -n -t 1 --width 4056 --height 3040 -e png "
                   "-o \"%s\" --immediate 2>&1",
                   filename);

          printf("Command: %s\n", camera_cmd);

          // Execute camera command
          FILE *fp = popen(camera_cmd, "r");
          if (fp == NULL) {
            // Send error response
            char response[MSG_BUFFER_SIZE];
            snprintf(response, sizeof(response),
                     "%s:%s:ERROR:Failed to execute camera command\n",
                     MSG_CAMERA_RESPONSE, device_id);
            sendto(socket_peer, response, strlen(response), 0,
                   (struct sockaddr *)&sender_address, sender_len);
            printf("ERROR: Failed to execute camera command\n");
          } else {
            char output[512];
            char combined_output[MSG_BUFFER_SIZE - 100];
            int offset = 0;

            // Read command output
            while (fgets(output, sizeof(output), fp) != NULL &&
                   offset < (MSG_BUFFER_SIZE - 200)) {
              int out_len = strlen(output);
              if (offset + out_len < MSG_BUFFER_SIZE - 200) {
                memcpy(combined_output + offset, output, out_len);
                offset += out_len;
              }
            }
            combined_output[offset] = '\0';

            int exit_code = pclose(fp);

            // Send camera response with filename and status
            char response[MSG_BUFFER_SIZE];
            if (exit_code == 0) {
              snprintf(response, sizeof(response),
                       "%s:%s:SUCCESS:Image saved as %s\n%s",
                       MSG_CAMERA_RESPONSE, device_id, filename,
                       combined_output);
              printf("SUCCESS: Image captured and saved as %s\n", filename);
            } else {
              snprintf(response, sizeof(response),
                       "%s:%s:ERROR:Camera capture failed\n%s",
                       MSG_CAMERA_RESPONSE, device_id, combined_output);
              printf("ERROR: Camera capture failed with exit code %d\n",
                     exit_code);
            }

            int resp_sent =
                sendto(socket_peer, response, strlen(response), 0,
                       (struct sockaddr *)&sender_address, sender_len);
            if (resp_sent < 0) {
              printf("ERROR: Failed to send camera response\n");
            } else {
              printf("Sent camera response (%d bytes)\n", resp_sent);
            }
          }
        } else if (strncmp(read, MSG_S3_UPLOAD_REQUEST,
                           strlen(MSG_S3_UPLOAD_REQUEST)) == 0) {
          // Upload all PNG images to S3
          printf("Processing S3 upload request...\n");

          // Generate S3 path with current date/time
          time_t now = time(NULL);
          struct tm *tm_info = localtime(&now);
          char s3_date_path[128];
          char s3_time_path[128];

          // Format: 2025-0822-scan/2025-0822-1430
          strftime(s3_date_path, sizeof(s3_date_path), "%Y-%m%d-scan", tm_info);
          strftime(s3_time_path, sizeof(s3_time_path), "%Y-%m%d-%H%M", tm_info);

          char s3_full_path[256];
          snprintf(s3_full_path, sizeof(s3_full_path),
                   "s3://berryscan-dome-scanner/%s/%s/", s3_date_path,
                   s3_time_path);

          printf("S3 destination: %s\n", s3_full_path);

          // Build AWS CLI command to upload all PNG files
          char upload_cmd[1024];
          snprintf(
              upload_cmd, sizeof(upload_cmd),
              "aws s3 cp . %s --recursive --exclude \"*\" --include \"*.png\"",
              s3_full_path);

          printf("Upload command: %s\n", upload_cmd);

          // Execute S3 upload command
          FILE *fp = popen(upload_cmd, "r");
          if (fp == NULL) {
            // Send error response
            char response[MSG_BUFFER_SIZE];
            snprintf(response, sizeof(response),
                     "%s:%s:ERROR:Failed to execute S3 upload command\n",
                     MSG_S3_UPLOAD_RESPONSE, device_id);
            sendto(socket_peer, response, strlen(response), 0,
                   (struct sockaddr *)&sender_address, sender_len);
            printf("ERROR: Failed to execute S3 upload\n");
          } else {
            char output[512];
            char combined_output[MSG_BUFFER_SIZE - 200];
            int offset = 0;
            int file_count = 0;

            // Read upload output and count uploaded files
            while (fgets(output, sizeof(output), fp) != NULL &&
                   offset < (MSG_BUFFER_SIZE - 300)) {
              // Count upload success messages
              if (strstr(output, "upload:") != NULL) {
                file_count++;
              }

              int out_len = strlen(output);
              if (offset + out_len < MSG_BUFFER_SIZE - 300) {
                memcpy(combined_output + offset, output, out_len);
                offset += out_len;
              }
            }
            combined_output[offset] = '\0';

            int exit_code = pclose(fp);

            // Send S3 upload response
            char response[MSG_BUFFER_SIZE];
            if (exit_code == 0) {
              snprintf(response, sizeof(response),
                       "%s:%s:SUCCESS:Uploaded %d files to %s\n%s",
                       MSG_S3_UPLOAD_RESPONSE, device_id, file_count,
                       s3_full_path, combined_output);
              printf("SUCCESS: Uploaded %d files to S3\n", file_count);
            } else {
              snprintf(response, sizeof(response),
                       "%s:%s:ERROR:S3 upload failed (exit code %d)\n%s",
                       MSG_S3_UPLOAD_RESPONSE, device_id, exit_code,
                       combined_output);
              printf("ERROR: S3 upload failed with exit code %d\n", exit_code);
            }

            int resp_sent =
                sendto(socket_peer, response, strlen(response), 0,
                       (struct sockaddr *)&sender_address, sender_len);
            if (resp_sent < 0) {
              printf("ERROR: Failed to send S3 upload response\n");
            } else {
              printf("Sent S3 upload response (%d bytes)\n", resp_sent);
            }
          }
        }
      }
    }

    // Send periodic heartbeat
    time_t current_time = time(NULL);
    if ((current_time - last_heartbeat) >= HEARTBEAT_INTERVAL_SEC) {
      char heartbeat_msg[MSG_BUFFER_SIZE];
      snprintf(heartbeat_msg, sizeof(heartbeat_msg), "%s:%s\n", MSG_HEARTBEAT,
               device_id);

      int hb_sent = sendto(socket_peer, heartbeat_msg, strlen(heartbeat_msg), 0,
                           relay_address->ai_addr, relay_address->ai_addrlen);
      if (hb_sent < 0) {
        printf("WARNING: Failed to send heartbeat\n");
      } else {
        char time_str[64];
        get_current_time_string(time_str, sizeof(time_str));
        printf("[%s] Heartbeat sent to relay server\n", time_str);
      }
      last_heartbeat = current_time;
    }
  }

  // Send unregister message before closing
  if (keep_running == 0) {
    printf("\nSending unregister message...\n");
    char unregister_msg[MSG_BUFFER_SIZE];
    snprintf(unregister_msg, sizeof(unregister_msg), "%s:%s\n", MSG_UNREGISTER,
             device_id);
    sendto(socket_peer, unregister_msg, strlen(unregister_msg), 0,
           relay_address->ai_addr, relay_address->ai_addrlen);
  }

  printf("Closing socket...\n");
  CLOSESOCKET(socket_peer);
  freeaddrinfo(relay_address);

#if defined(_WIN32)
  WSACleanup();
#endif

  printf("[OFFLINE] Sub-client '%s' shutdown complete.\n", device_id);
  return 0;
}
