#include <stdio.h>

int main(int argc, const char* argv[], char **env_var_ptr) {
  int i;

  for (i = 0; i < argc; ++i) {
    printf("Arg[%d] = %p", i, argv[i]);
    if (argv[i] != NULL)
      printf(", content=%s", argv[i]);
    printf("\n");
  }
  while (*env_var_ptr != NULL) {
      i++;
      printf ("Env[%d] : %s\n",i, *(env_var_ptr++));
  }

  return 0;
}
