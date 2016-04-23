#include <stdio.h>
#include <unistd.h>

int main()
{
	while(1) {
		printf("[1]\n");
		usleep(200*1000);
	}
  return 0;
}

