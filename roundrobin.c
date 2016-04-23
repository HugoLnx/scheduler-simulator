#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

int pids[100], *programs_size, id_programs=-1, id_types=-1, id_programs_size=-1;
char *programs;
int *types;

#define PROGRAMS_KEY      8762
#define TYPES_KEY         8763
#define PROGRAMS_SIZE_KEY 8764

int iniciar_processo(int id) {
	int pid = fork();
	int is_child = pid == 0;
	if(is_child) {
		char *args[255] = {programs+(id*255), NULL};
		execv(programs+(id*255), args);
		exit(1);
	} else {
		kill(pid, SIGSTOP);
		return pid;
	}
}

void wait_for_programs() {
	printf("Waiting for the programs...\n");
	while(id_programs == -1 || id_types == -1 || id_programs_size == -1) {
		sleep(1);
		id_programs_size = shmget(PROGRAMS_SIZE_KEY, sizeof(int), S_IRWXU);
		if(id_programs_size != -1) {
			programs_size = shmat(id_programs_size, 0, 0);	
			id_programs = shmget(PROGRAMS_KEY, sizeof(char)**programs_size*255, S_IRWXU);
			id_types = shmget(TYPES_KEY, sizeof(int)**programs_size*2, S_IRWXU);
		}
	}

	// Obtem referencia dos 3 espacos de memoria compartilhada
	// Cada espaco representa uma matrix
	programs = shmat(id_programs, 0, 0);
	types = shmat(id_types, 0, 0);
	printf("%d programs found\n", *programs_size);
}

int main()
{
	int i;
	wait_for_programs();
	int current = 0;
	for(i = 0; i < *programs_size; i++) {
		printf("%s %d %d\n",programs+i*255, types[i*2], types[i*2+1]);
	}
	for(i = 0; i < *programs_size; i++) {
		pids[i] = iniciar_processo(i);
		printf("pid: %d\n", pids[i]);
	}

	while(1) {
		int previous = (current+*programs_size*2-1) % *programs_size;
		kill(pids[previous], SIGSTOP);
		kill(pids[current], SIGCONT);
		
		sleep(1);
		current = (current+1)%*programs_size;
	}

	shmdt(programs);
	shmdt(types);
	shmdt(programs_size);

	shmctl(id_programs, IPC_RMID, 0);
	shmctl(id_types, IPC_RMID, 0);
	shmctl(id_programs_size, IPC_RMID, 0);
}
