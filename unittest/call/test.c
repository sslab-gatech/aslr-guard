#include <stdio.h>

void hello() {
  printf("Hello World\n");
}

void test_direct_call() {
  printf("Hello World\n");
}

void test_indirect_call(void (*func)()) {
  func();
}

void test_print_func(void (*func)()) {
  printf("func: %p\n", func);
}

int main(void) {
  printf("hello: %p\n", hello);
  
  test_direct_call();
  test_indirect_call(hello);
  test_print_func(hello);
  
  return 0;
}
