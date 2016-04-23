#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

int *programs_size, id_programs=-1, id_types=-1, id_programs_size=-1;
int *pids, id_pids;
char *programs;
int *types;

#define PROGRAMS_KEY      8762
#define TYPES_KEY         8763
#define PROGRAMS_SIZE_KEY 8764

void release_shared_memory_and_exit(int code) {
	shmdt(programs);
	shmdt(types);
	shmdt(programs_size);
	shmdt(pids);

	shmctl(id_programs, IPC_RMID, 0);
	shmctl(id_types, IPC_RMID, 0);
	shmctl(id_programs_size, IPC_RMID, 0);
	shmctl(id_pids, IPC_RMID, 0);
	exit(code);
}

int start_process(int id) {
	int pid = fork();
	int is_child = pid == 0;
	if(is_child) {
		char *args[255] = {programs+(id*255), NULL};
		execv(programs+(id*255), args);
		release_shared_memory_and_exit(1);
	} else {
		kill(pid, SIGSTOP);
		return pid;
	}
}

void initialize_processes() {
	int i;
	int pid = fork();
	int is_child = pid == 0;
	if(is_child) {
		for(i = 0; i < *programs_size; i++) {
			pids[i+1] = start_process(i);
			pids[0] = i+1;
			sleep(3);
		}
		release_shared_memory_and_exit(0);
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
	int current = 0;
	// TODO: Ver se dá para fazer freopen funfar para subprocessos
	// freopen("saida.txt","w",stdout);
	wait_for_programs();

	id_pids = shmget(IPC_PRIVATE, sizeof(int)**programs_size, IPC_CREAT | IPC_EXCL | S_IRWXU);
	pids = shmat(id_pids, 0, 0);
	pids[0] = 0;

	initialize_processes();

	while(1) {
		if(pids[0] > 0) {
			int i;
			for(i = 0; i < pids[0]; i++) kill(pids[i+1], SIGSTOP);
			kill(pids[current+1], SIGCONT);
			
			usleep(500*1000); // sleep 0.5 seconds
			current = ((current+1)%pids[0]);
		}
	}

	release_shared_memory_and_exit(0);
}
