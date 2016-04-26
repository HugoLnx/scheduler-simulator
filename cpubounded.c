#include <stdio.h>
#include <unistd.h>

int main()
{
	int i, k, p;
	for(i=0; i < 50; i++) {
		printf("[::NUMBER::]\n");
		p = 0;
		for(k=0; k < 100000000; k++) p += k;
	}
  return 0;
}
