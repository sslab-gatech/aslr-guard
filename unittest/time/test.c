#include <stdio.h>
#include <time.h>
#include <string.h>

time_t rawtime;
int main(int argc, const char* argv[], char **env_var_ptr) {
  time ( &rawtime );
  printf("Rawtime: %lu\n", rawtime);

  char buff[50] = {0};
  strftime(buff, 20, "%Y-%m-%d %H:%M:%S", localtime(&rawtime));
  printf("Is this current time??? %s\n", buff);

  return 0;
}
