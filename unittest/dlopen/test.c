#include<stdio.h>
#include<stdlib.h>
#include <dlfcn.h>
#include <unistd.h>
#include <string.h>
 
int main(int argc, char **argv) {
  char cwd[1024] = {0};
  if (getcwd(cwd, sizeof(cwd)) != NULL) {
    strcpy(cwd + strlen(cwd), "/../../eglibc-2.19/build/math/libm.so");
    printf("@ cwd: %s\n", cwd);
    void *handle = dlopen (cwd, RTLD_LAZY);
    printf("@ handle: 0x%lx   \n", (long unsigned int)handle);
  }
  return 0; 
}

