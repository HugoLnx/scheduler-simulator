#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#define DEBUG_LOTTERY //printf

int *programs_size, id_programs=-1, id_types=-1, id_programs_size=-1;
int pids[100];
int rrobin_pids[100];
int rrobin_current = 0;
char *programs;
int *types;
int lottery_pids[21], tickets[21];
int free_tickets[21];
int current_program = 0;

#define PROGRAMS_KEY      8762
#define TYPES_KEY         8763
#define PROGRAMS_SIZE_KEY 8764

void set_private_shared_memory() {
	pids[0] = 0;
	rrobin_pids[0] = 0;
	memset(tickets, 0x00, sizeof(int)*21);
	memset(lottery_pids, 0x00, sizeof(int)*21);
}

void release_shared_memory_and_exit(int code) {
	shmdt(programs);
	shmdt(types);
	shmdt(programs_size);

	shmctl(id_programs, IPC_RMID, 0);
	shmctl(id_types, IPC_RMID, 0);
	shmctl(id_programs_size, IPC_RMID, 0);
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

void initialize_tickets() {
	int i;
	free_tickets[0] = 20;
	for(i = 1; i <= 20; i++) {
		free_tickets[i] = i;
	}
}

int randomize(int init, int end) {
	return init+rand()%(1+end-init);
}

int get_ticket() {
	int ticket, i;
	int tmp[21];
	int inx = randomize(1,free_tickets[0]);
	ticket = free_tickets[inx];	
	memcpy(tmp, free_tickets+inx+1, sizeof(int)*(free_tickets[0]-inx));
	memcpy(free_tickets+inx, tmp, sizeof(int)*(free_tickets[0]-inx));
	free_tickets[0]--;
	tickets[++tickets[0]] = ticket;
	return ticket;
}

void get_tickets_for(int pid, int amount) {
	int i;
	DEBUG_LOTTERY("tickets for %d\n", pid);
	for(i = 0; i < amount; i++) {
		int ticket = get_ticket();
		lottery_pids[ticket] = pid;
		DEBUG_LOTTERY("> %d\n", ticket);
	}
}

void run_program(int id) {
	int pid = start_process(id);
	if(types[id*2] == 0) {
		// ROUND ROBIN
		rrobin_pids[rrobin_pids[0]+1] = pid;
		rrobin_pids[0] = rrobin_pids[0]+1;
	} else if(types[id*2] == 1) {
		// PRIORITY
	} else if(types[id*2] == 2) {
		// LOTTERY
		get_tickets_for(pid, types[id*2+1]);
	}
	pids[pids[0]+1] = pid;
	pids[0] = pids[0]+1;
}

void run_next_program() {
	run_program(current_program++);
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

void resume_robin_process() {
	if(rrobin_pids[0] > 0) {
		kill(rrobin_pids[rrobin_current+1], SIGCONT);
		rrobin_current = ((rrobin_current+1)%rrobin_pids[0]);
	}
}

void resume_lottery_process() {
	if(tickets[0] > 0) {
		int ticket = tickets[randomize(1, tickets[0])];
		DEBUG_LOTTERY("ticket choosed: %d\n", ticket);
		kill(lottery_pids[ticket], SIGCONT);
	}
}

int main()
{
	double time_past = 0.0;
	srand(time(NULL));
	initialize_tickets();
	// TODO: Ver se dá para fazer freopen funfar para subprocessos
	// freopen("saida.txt","w",stdout);
	wait_for_programs();
	set_private_shared_memory();

	while(1) {
		if(pids[0] > 0) {
			int i;
			for(i = 0; i < pids[0]; i++) kill(pids[i+1], SIGSTOP);
			//resume_robin_process();
			resume_lottery_process();
		}

		usleep(500*1000); // sleep 0.5 seconds
		time_past += 0.5;
		if(time_past >= 3.0) {
			run_next_program();
			time_past = 0.0;
		}
	}

	release_shared_memory_and_exit(0);
}
