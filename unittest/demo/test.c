#include <stdio.h>

void hello() {
  printf("Hello World\n");
}

int main(void) {
  void (*fn)() = hello;

  printf("function pointer of hello(): %p\n", fn);
  
  fn();
 
  return 0;
}
