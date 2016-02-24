#include<stdio.h>
#include<stdlib.h>
#include<string.h>

void main() {
  printf ("the result should be 0  -- result: %lx\n", (unsigned long) strchr("hello", '#'));
}
