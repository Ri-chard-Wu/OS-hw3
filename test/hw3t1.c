#include "syscall.h"

int main() {
  int n;
  for (n = 1; n < 1000; ++n);

  PrintInt(1);

  for (n = 1; n < 20000; ++n);
  Exit(1);
}

