#include "tests/lib.h"

int main(void) {
  int f;
  prints("tarvitaaks siel passi?");
  f = syscall_open((char*)0xcafebabe);
  f = f;
  return 0;
}
