#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#define PROGRAMS_KEY      8762
#define TYPES_KEY         8763
#define PROGRAMS_SIZE_KEY 8764

int main()
{
	int id_programs_size, id_programs, id_types;
	int programs_size, *shared_programs_size;
	char programs[100][255], *shared_programs;
	int types[100][2], *shared_types;
	int i;
	char word[255];
	char *before_equal, *after_equal;
	freopen("exec.txt","r",stdin);
	memset(types, 0x00, sizeof(int)*100*2);
	memset(programs, 0x00, sizeof(char)*100*255);
	
	programs_size = 0;
	while(scanf(" %s", word) != EOF) {
		if(strcmp("Exec", word) == 0) {
			// ignore
		} else {
			before_equal = strtok(word,"=");
			after_equal = strtok(NULL,"=");
			if(after_equal == NULL) {
				strcpy(programs[programs_size], before_equal);
				programs_size++;
			} else if(strcmp(before_equal, "prioridade") == 0) {
				types[programs_size-1][0] = 1;
				sscanf(after_equal, "%d", &types[programs_size-1][1]);
			} else if(strcmp(before_equal, "numtickets") == 0) {
				types[programs_size-1][0] = 2;
				sscanf(after_equal, "%d", &types[programs_size-1][1]);
			}
		}
	}

	//for(i = 0; i < programs_size; i++) {
	//	printf("%s %d %d\n",programs[i], types[i][0], types[i][1]);
	//}

	id_programs = shmget(PROGRAMS_KEY, sizeof(char)*programs_size*255, IPC_CREAT | IPC_EXCL | S_IRWXU);
	shared_programs = shmat(id_programs, 0, 0);
	memcpy(shared_programs, programs, sizeof(char)*programs_size*255);

	id_types = shmget(TYPES_KEY, sizeof(int)*programs_size*2, IPC_CREAT | IPC_EXCL | S_IRWXU);
	shared_types = shmat(id_types, 0, 0);
	memcpy(shared_types, types, sizeof(int)*programs_size*2);

	id_programs_size = shmget(PROGRAMS_SIZE_KEY, sizeof(int), IPC_CREAT | IPC_EXCL | S_IRWXU);
	shared_programs_size = shmat(id_programs_size, 0, 0);
	*shared_programs_size = programs_size;

	sleep(1);

	shmdt(shared_programs);
	shmdt(shared_types);
	shmdt(shared_programs_size);

	shmctl(id_programs, IPC_RMID, 0);
	shmctl(id_types, IPC_RMID, 0);
	shmctl(id_programs_size, IPC_RMID, 0);
	
  return 0;
}

