#include <stdio.h>

void test(int a, int b, int c, int d, int e, int f, void (*func)()) {
  if ((int)func != 7) {
    func();
  }
}


int main(void) {
  test(1,2,3,4,5,6,7);
  return 0;
}
