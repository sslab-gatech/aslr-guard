#include <string.h>
#include <stdio.h>


class A {
  public:
    virtual void v(char * str) {
      printf("class A: %s\n", str);
    }

    int a;
};


class B: public A {
  public:
    void v(char * str) {
      printf("class B: %s\n", str);
    }

    int b;
};


static void (*fp) (char *);

int main () {

  A *a = new A();
  A *a1 = new B();
  a->v((char *)"hello");
  a1->v((char *)"hello");

}
