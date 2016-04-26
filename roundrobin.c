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
#define DEBUG_PRIORITY //printf
#define DEBUG_STACK(A) A

int *programs_size, id_programs=-1, id_types=-1, id_programs_size=-1;
int pids[100];
int rrobin_pids[100];
int rrobin_current = 0;
char *programs;
int *types;
int lottery_pids[21], tickets[21];
int free_tickets[21];
int current_program = 0;
int current_pid[2] = {-1, -1};

struct prio{
    int pid;
    int priority;
};
typedef struct prio Prio;

Prio prio_pids[100];
int numPrio = 0;

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

void delete_at(void *array, int inx, int element_size, int array_size) {
	char tmp[1000];
	memcpy(tmp, array+(inx+1)*element_size, element_size*(array_size-inx));
	memcpy(array+inx*element_size, tmp, element_size*(array_size-inx));
}

int get_ticket() {
	int ticket, i;
	int tmp[21];
	int inx = randomize(1,free_tickets[0]);
	ticket = free_tickets[inx];	
	delete_at(free_tickets, inx, sizeof(int), free_tickets[0]);
	free_tickets[0]--;
	tickets[++tickets[0]] = ticket;
	return ticket;
}

void get_tickets_for(int pid, int amount) {
	int i;
	for(i = 0; i < amount; i++) {
		int ticket = get_ticket();
		lottery_pids[ticket] = pid;
	}
}

int comparePrio(const void* a, const void* b)
{
    Prio *a1 = (Prio*) a;
    Prio *b1 = (Prio*) b;
    
    return a1->priority > b1->priority;
}

void set_priority_process_in_memory(int pid, int prio) {
    prio_pids[numPrio].pid = pid;
    prio_pids[numPrio].priority = prio;
    
    DEBUG_PRIORITY("PROCESSO: Pid = %d , Prioridade = %d \n", prio_pids[numPrio].pid, prio_pids[numPrio].priority);
    numPrio++;
    
    qsort(prio_pids, numPrio, sizeof(Prio), comparePrio);
}

void run_program(int id) {
	int pid = start_process(id);
	if(types[id*2] == 0) {
		// ROUND ROBIN
		rrobin_pids[rrobin_pids[0]+1] = pid;
		rrobin_pids[0] = rrobin_pids[0]+1;
	} else if(types[id*2] == 1) {
		// PRIORITY
        set_priority_process_in_memory(pid, types[id*2+1]);
	} else if(types[id*2] == 2) {
		// LOTTERY
		get_tickets_for(pid, types[id*2+1]);
	}
	pids[pids[0]+1] = pid;
	pids[0] = pids[0]+1;
}

void run_next_program() {
	if(current_program < *programs_size) {
		run_program(current_program++);
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

void resume_robin_process() {
	if(rrobin_pids[0] > 0) {
		kill(rrobin_pids[rrobin_current+1], SIGCONT);
		current_pid[0] = 0;
		current_pid[1] = rrobin_pids[rrobin_current+1];
		rrobin_current = ((rrobin_current+1)%rrobin_pids[0]);
	}
}

void resume_lottery_process() {
	if(tickets[0] > 0) {
		int ticket = tickets[randomize(1, tickets[0])];
		DEBUG_LOTTERY("ticket choosed: %d\n", ticket);
		kill(lottery_pids[ticket], SIGCONT);
		current_pid[0] = 2;
		current_pid[1] = lottery_pids[ticket];
	}
}

void resume_priority_process() {
    if(numPrio > 0) {
        kill(prio_pids[0].pid, SIGCONT);
        current_pid[0] = 1;
        current_pid[1] = prio_pids[0].pid;
        DEBUG_PRIORITY("pid chosed: %d\n", prio_pids[0].pid);
    }
}

void remove_pid(int pid) {
	int inx = 1;
	while(pids[inx] != pid) inx++;
	delete_at(pids, inx, sizeof(int), pids[0]);
	pids[0]--;
}

void remove_rrobin_pid(int pid) {
	int inx = 1;
	while(rrobin_pids[inx] != pid) inx++;
	delete_at(rrobin_pids, inx, sizeof(int), rrobin_pids[0]);
	rrobin_pids[0]--;
}

void remove_priority_pid(int pid) {
	int inx = 0;
	while(prio_pids[inx].pid != pid) inx++;
	delete_at(prio_pids, inx, sizeof(Prio), numPrio);
	numPrio--;
}

void remove_ticket(int ticket) {
	int inx = 1;
	while(tickets[inx] != ticket) inx++;
	delete_at(tickets, inx, sizeof(int), tickets[0]);
	tickets[0]--;

	lottery_pids[ticket] = 0;
	free_tickets[++free_tickets[0]] = ticket;
}

void remove_lottery_pid(int pid) {
	int ticket;
	for(ticket = 1; ticket <= 20; ticket++) {
		if(lottery_pids[ticket] == pid) {
			remove_ticket(ticket);
		}
	}
}

void finalize_current_process_when_finished() {
	int process_have_finished = waitpid(current_pid[1], NULL, WNOHANG) > 0;
	if(process_have_finished) {
		remove_pid(current_pid[1]);
		if(current_pid[0] == 0) {
			remove_rrobin_pid(current_pid[1]);
		} else if(current_pid[0] == 1) {
			remove_priority_pid(current_pid[1]);
		} else if(current_pid[0] == 2) {
			remove_lottery_pid(current_pid[1]);
		}
	}
}

void print_sized_array(char *name, int *array) {
	int i;
	printf("%s (%d): [", name, array[0]);
	for(i = 1; i <= array[0]; i++) {
		printf("%d ", array[i]);
	}
	printf("]\n");
}

void print_priority_pids() {
	int i;
	printf("Priority (%d): [", numPrio);
	for(i = 0; i < numPrio; i++) {
		printf("(%d,%d) ", prio_pids[i].priority, prio_pids[i].pid);
	}
	printf("]\n");
}

void print_array(char *name, int *array, int size) {
	int i;
	printf("%s (%d): [", name, array[0]);
	for(i = 0; i < size; i++) {
		printf("%d ", array[i]);
	}
	printf("]\n");
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
			if(current_pid[0] >= 0){
				kill(current_pid[1], SIGSTOP);
				finalize_current_process_when_finished();
			}
            
            if (prio_pids[0].pid > 0)
                resume_priority_process();
            else if (rrobin_pids[0] > 0)
                resume_robin_process();
            else if (tickets[0] > 0)
                resume_lottery_process();
		}

		usleep(500*1000); // sleep 0.5 seconds
		time_past += 0.5;
		if(time_past >= 3.0) {
			run_next_program();
			time_past = 0.0;
		}
		DEBUG_STACK(print_sized_array)("Robin", rrobin_pids);
		DEBUG_STACK(print_array)("Lottery", lottery_pids, 20);
		DEBUG_STACK(print_sized_array)("Used Tickets", tickets);
		DEBUG_STACK(print_sized_array)("Free Tickets", free_tickets);
		DEBUG_STACK(print_priority_pids());
		DEBUG_STACK(print_sized_array)("PIDs", pids);
	}

	release_shared_memory_and_exit(0);
}
