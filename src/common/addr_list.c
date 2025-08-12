#include <ifaddrs.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>

int main() {

  struct ifaddrs *addresses; // declare a pointer var points to a struct type
                             // which is a linked list.

  if (getifaddrs(&addresses) == -1) { // returns 0 on success and -1 on failure
    printf("getifaddrs call failed\n");
    return -1;
  }

  struct ifaddrs *address =
      addresses; // used as a interator to traverse the linked list

  while (address) {
    if (address->ifa_addr == NULL) {
      address = address->ifa_next;
      continue;
    }

    int family = address->ifa_addr->sa_family;

    if (family == AF_INET || family == AF_INET6) {

      printf("%s\t", address->ifa_name);
      printf("%s\t", family == AF_INET ? "IPv4" : "IPv6");

      char ap[100];

      const int family_size = family == AF_INET ? sizeof(struct sockaddr_in)
                                                : sizeof(struct sockaddr_in6);

      getnameinfo(address->ifa_addr, family_size, ap, sizeof(ap), 0, 0,
                  NI_NUMERICHOST);

      printf("\t%s\n", ap);
    }

    address = address->ifa_next;
  }

  freeifaddrs(addresses);

  return 0;
}

/*
lo0     IPv4            127.0.0.1 // loopback
lo0     IPv6            ::1
lo0     IPv6            fe80::1%lo0
en0     IPv6            fe80::10c5:80ac:f3a0:460c%en0 // ethernet
en0     IPv4            192.168.100.45
en0     IPv6            2404:7a80:bf04:c00:88f:2414:4bfd:104c
en0     IPv6            2404:7a80:bf04:c00:5949:329d:694f:bd27
awdl0   IPv6            fe80::c086:e5ff:fee2:2bb5%awdl0 // apple wireless direct
link llw0    IPv6            fe80::c086:e5ff:fee2:2bb5%llw0 // apple low latency
wlan utun0   IPv6            fe80::f608:fc29:967c:68a6%utun0 // tunnel interface
utun1   IPv6            fe80::3b2:ef22:c027:d7e4%utun1
utun2   IPv6            fe80::116f:ecf5:c745:ed2e%utun2
utun3   IPv6            fe80::ce81:b1c:bd2c:69e%utun3
*/
