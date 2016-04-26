#include <stdio.h>
#include <unistd.h>

int main()
{
	int i;
	for(i=0; i < 50; i++) {
		printf("[::NUMBER::]\n");
		usleep(200*1000);
	}
  return 0;
}
