#include "syscall.h"

int
main()
{
  int n;
  for (n = 1; n < 5; ++n) {}
  PrintInt(3);
  for (n = 1; n < 10000; ++n);  
  Exit(3);
}
