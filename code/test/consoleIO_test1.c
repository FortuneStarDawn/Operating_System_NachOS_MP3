#include "syscall.h"

int
main()
{
	int i, n=0;
	for (i=0; i<10; i++) {
		n++;
	}
  PrintInt(n);
  for (i=0; i<200; i++) {
    n++;
  }
}

