#include "tests/lib.h"

int main(void) {
  int f;
  prints("tarvitaaks siel passi?");
  f = syscall_open((char*)0x19248);
  f = f;
  return 0;
}
