#include <stdio.h>
#include <stdlib.h>

#include <signal.h>

#include <sys/ipc.h>
#include <sys/types.h>

#include <sys/sem.h>
#include <sys/shm.h>

#include <limits.h>
#include <errno.h>

#include <unistd.h>
#include <time.h>

#include "mutual.h"


#define exit_error(format1, ...) \
do{ \
	fprintf(stderr, RED format1 RESET, ##__VA_ARGS__); \
	perror("");\
	exit(0); \
}while(0)


//shared memory ID and pointer
int shmID;
struct q_memory *shmPTR;

//semaphore ID
int semID;


//time function
void print_stamp(int pid)
{
	struct timespec measure;
	if(clock_gettime(CLOCK_MONOTONIC, &measure) == -1)
	{
		perror("Time measure problem.");
		return;
	}
	printf(RESET);
	printf("[%lds:%ldms]", measure.tv_sec, measure.tv_nsec/1000);
	if(pid != 0)
		printf("[%d]", pid);
}

//atexit functions
void delete_shm()
{
	if(shmctl(shmID, IPC_RMID, NULL) == -1) perror("Couldn't marker\
		       	as \"to destroyed\"");
}

void detach_shm()
{
	if(shmdt((void*) shmPTR) == -1) perror("Couldn't detach shm.");
}

void delete_sem()
{
	if(semctl(semID, 0, IPC_RMID) == -1) perror("Couldn't delete semaphore.");
}


//SIGTERM handle
void handle_sigterm(int signum)
{
	printf("Got SIGTERM.\n");
	exit(0);
}

//set mask and signal handle
void set_sig_options()
{
	signal(SIGTERM, handle_sigterm);

	sigset_t mask;
	sigfillset(&mask);
	sigdelset(&mask, SIGTERM);
	sigprocmask(SIG_BLOCK, &mask, NULL);
}

//parse numeber of chairs in queue
long parse_arg(char *args)
{
	char *strtol_end = NULL;

	int arg_long = strtol(args, &strtol_end, 10);

	if(args == strtol_end)
	{
		fprintf(stderr, RED "Couldn't parse argument.\n" RESET);
		exit(0);
	}
	if((arg_long == LONG_MAX || arg_long == LONG_MIN) && errno == ERANGE)
	{
		exit_error("Argument is out of range.");
	}
	if(arg_long <= 0)
	{
		fprintf(stderr, RED "Chairs number should be positive.\n" RESET);
		exit(0);
	}

	return arg_long;
}

//create, attach, initialize shared memory
//shmID = memory ID; shmPTR = memory pointer
void get_around_memory(int chairs_num)
{
	//create shared memory
	if((shmID = shmget(ftok(PATH, to_key),
				sizeof(struct q_memory) + sizeof(int) * (chairs_num + 1),
				IPC_CREAT | IPC_EXCL | 0666)) == -1)
		exit_error("Shared mem alloc error.");
	atexit(delete_shm);

	//attach shared memory
	if((shmPTR = (struct q_memory*) shmat(shmID, NULL, 0)) == (void*) -1)
		exit_error("Couldn't link shm.");
	atexit(detach_shm);

	//initialize shared memory
	shmPTR->chairs_num = chairs_num;
	shmPTR->chairs_taken = 0;
	shmPTR->first_in_queue = 0;
	shmPTR->first_free_chair = 0;
	shmPTR->barber_state = 's';
	shmPTR->current_client = -1;
}

//create and initialize semaphore
//semID = semaphore ID
//add delete to atexit
void get_around_semaphore()
{
	//create semaphore
	if((semID = semget(ftok(PATH, to_key), 1, IPC_CREAT | IPC_EXCL | 0666))
	 				== -1)
		exit_error("Couldn't create semaphore.");
	atexit(delete_sem);

	//initializing semaphore
	union semun init_val = {.val = 0};
	if(semctl(semID, 0, SETVAL, init_val) == -1)
		exit_error("Couldn't initialize semaphore value.");
}


//barber functions
void go_to_sleep()
{
	shmPTR->barber_state = 's';
	print_stamp(0);
	printf(MAGENTA "Zasypiam.\n" RESET);

	//return resourses
	struct sembuf sops = {
		.sem_num = 0,
		.sem_op = 1,
		.sem_flg = 0
	};
	if(semop(semID, &sops, 1) == -1)
	{
		perror("Semop returned error2.");

		exit(5);
	}

	//waiting for 0 "resourses" on semaphore
	sops.sem_op = 0;
	if(semop(semID, &sops, 1) == -1)
		exit_error("Semop returned error.");

	//waiting for client to end
	sops.sem_op = -1;
		if(semop(semID, &sops, 1) == -1)
			exit_error("Semop returned error.");

	print_stamp(shmPTR->current_client);
	printf(GREEN "Wstaje!\n" RESET);
}

void ask_client_in()
{
	shmPTR->current_client = shmPTR->pids_queue[shmPTR->first_in_queue];
	print_stamp(shmPTR->current_client);
	printf(GREEN "Zapraszam!\n" RESET);
}

void client_move()
{
	//sign that it's his turn
	shmPTR->barber_state = 'W';

    while(shmPTR->barber_state != 'w')
    {
        //give client initiative
        struct sembuf sops = {
            .sem_num = 0,
            .sem_op = 1,
            .sem_flg = 0
        };
        if(semop(semID, &sops, 1) == -1)
        {
            perror("Semop returned error2.");
            exit(5);
        }

        //wait for him to react
        sops.sem_op = -1;
        if(semop(semID, &sops, 1) == -1)
        {
            perror("Semop returned error2.");
            exit(5);
        }
    }
}

int main(int argc, char *argv[])
{
	//set mask and signal handle
	set_sig_options();

	//parse argument
	if(argc != 2)
	{
		fprintf(stderr, RED "Wrong arguments number. Given %d, should be 1"
							" - chairs number.\n" RESET, argc-1);
		exit(0);
	}
	long chairs_num = parse_arg(argv[1]);

	printf( "Fryzjer %d zaprasza!\n" , getpid());

	//create, attach, initialize shared memory
	//shmID = memory ID; shmPTR = memory pointer
	//add detach and delete to atexit
	get_around_memory(chairs_num);

	//create and initialize semaphore
	//semID = semaphore ID
	//add delete to atexit
	get_around_semaphore();



	//barber starts working
	while( "true" )
	{
		if(shmPTR->current_client == -1 && shmPTR->chairs_taken == 0)
		{
			//handle sleep and wake up print
			go_to_sleep();
			continue;
		}
		else if(shmPTR->current_client == -1)
			ask_client_in();

		//client's business
		client_move();

		print_stamp(shmPTR->current_client);
		printf( "Zaczynajmy!\n");
		//shaving in process
		print_stamp(shmPTR->current_client);
		printf(BLUE "Koniec!\n" RESET);

		//client's business
		client_move();
	}

	exit(0);
}
