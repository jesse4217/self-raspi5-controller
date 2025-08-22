#ifndef TIME_PROTOCOL_H
#define TIME_PROTOCOL_H

#include <time.h>

// Port definitions
#define RELAY_SERVER_PORT "8080"
#define SUB_CLIENT_BASE_PORT 8081

// Buffer sizes
#define MSG_BUFFER_SIZE 1024
#define DEVICE_ID_SIZE 32
#define MAX_SUB_CLIENTS 10
#define ADDRESS_BUFFER_SIZE 100

// Message types
#define MSG_REGISTER "REGISTER"
#define MSG_TIME_REQUEST "TIME_REQUEST"
#define MSG_TIME_RESPONSE "TIME_RESPONSE"
#define MSG_HEARTBEAT "HEARTBEAT"
#define MSG_UNREGISTER "UNREGISTER"

// Timeouts (in seconds)
#define RESPONSE_TIMEOUT_SEC 2
#define HEARTBEAT_INTERVAL_SEC 30
#define CLIENT_TIMEOUT_SEC 90  // Remove client if no heartbeat for this duration

// Sub-client registry structure
struct sub_client {
  char device_id[DEVICE_ID_SIZE];
  struct sockaddr_storage address;
  socklen_t address_len;
  time_t last_heartbeat;
  int active;
};

// Response aggregation structure
struct time_response {
  char device_id[DEVICE_ID_SIZE];
  char timestamp[64];
  int received;
};

#endif // TIME_PROTOCOL_H