#include <stdio.h>
#include <sys/time.h>

typedef int (*fn)(struct timeval *, void *);

int main(void) {
  fn f = (fn)gettimeofday;

  struct timeval tv;
  printf("hello: %p\n", f(&tv, NULL));
  return 0;
}
