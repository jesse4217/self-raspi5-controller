#include <stdio.h>
#include <time.h>

int main(void) {
  time_t timer; // declare a var of type time_t``
  time(&timer); // fills pointer `timers` with current time

  char *read_time = ctime(&timer); // convert to readable time

  printf("Local time is: %s", read_time);

  return 0;
}
