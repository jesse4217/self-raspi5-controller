
#include <ifaddrs.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>

int main() {

  struct ifaddrs *addresses;

  if (getifaddrs(&addresses) == -1) {
    printf("getifaddrs call failed\n");
    return -1;
  }

  struct ifaddrs *address = addresses;

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
 *  Interface   Type    Address                 Purpose
  ---------   ----    -------                 -------
  lo0         IPv4    127.0.0.1              Loopback (localhost)
  lo0         IPv6    ::1                    IPv6 loopback
  lo0         IPv6    fe80::1%lo0            Link-local loopback

  en0         IPv4    192.168.100.45         Your main WiFi/Ethernet (private
 IP) en0         IPv6    2404:7a80:bf04:c00:... Global IPv6 addresses
 (internet-routable) en0         IPv6    fe80::10c5:...%en0     Link-local
 address

  awdl0       IPv6    fe80::3071:...%awdl0   Apple Wireless Direct Link
 (AirDrop) llw0        IPv6    fe80::3071:...%llw0    Low-latency WLAN (Apple
 continuity) utun0-3     IPv6    fe80::...              VPN tunnel interfaces
 * */
