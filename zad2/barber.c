#include <stdio.h>
#include <stdlib.h>

#include <signal.h>

#include <sys/stat.h>
#include <fcntl.h>

#include <semaphore.h>
#include <sys/mman.h>

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
size_t shm_len;
struct q_memory *shmPTR;

//semaphore addres
sem_t *semPTR_sleep;
sem_t *semPTR_work;


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
void unlink_shm()
{
	if(shm_unlink(PATH1) == -1) perror("Couldn't mark as \"to destroy\".");
}

void munmap_shm()
{
	if(munmap((void*) shmPTR, shm_len) == -1) perror("Couldn't unmap shm.");
}

void delete_sem_sleep()
{
	if(sem_unlink(PATH1) == -1) perror("Couldn't delete semaphore.");
}

void delete_sem_work()
{
	if(sem_unlink(PATH2) == -1) perror("Couldn't delete semaphore.");
}

void close_sem_sleep()
{
	if(sem_close(semPTR_sleep) == -1) perror("Couldn't close semaphore.");
}

void close_sem_work()
{
	if(sem_close(semPTR_work) == -1) perror("Couldn't close semaphore.");
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
	if((shmID = shm_open(PATH1, O_CREAT | O_EXCL | O_RDWR, 0666)) == -1)
		exit_error("Shared mem open error.");
	atexit(unlink_shm);

	//set length
	shm_len = sizeof(struct q_memory) + sizeof(int) * (chairs_num + 1);
	if(ftruncate(shmID, shm_len) == -1)
		exit_error("Error from ftrucate.");

	//map shared memory
	if((shmPTR = (struct q_memory*) mmap(NULL, shm_len, PROT_READ | PROT_WRITE,
										MAP_SHARED, shmID, 0)) == (void*) -1)
		exit_error("Couldn't map shm.");
	atexit(munmap_shm);

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
void get_around_semaphores()
{
	//get semaphore ID
	if((semPTR_sleep = sem_open(PATH1, O_CREAT | O_EXCL, 0666, 0)) == SEM_FAILED)
		exit_error("Couldn't get semaphore ID.");
	atexit(delete_sem_sleep);
	atexit(close_sem_sleep);

	//get semaphore ID
	if((semPTR_work = sem_open(PATH2, O_CREAT | O_EXCL, 0666, 0)) == SEM_FAILED)
		exit_error("Couldn't get semaphore ID.");
	atexit(delete_sem_work);
	atexit(close_sem_work);
}


//barber functions
void go_to_sleep()
{
	shmPTR->barber_state = 's';
	print_stamp(0);
	printf(MAGENTA "Zasypiam.\n" RESET);

	if(sem_post(semPTR_work) == -1)
	{
		perror("Semop returned error2.");
		exit(5);
	}

	if(sem_wait(semPTR_sleep) == -1)
	{
		perror("Semop returned error2.");
		exit(5);
	}

	if(sem_wait(semPTR_work) == -1)
	{
		perror("Semop returned error2.");
		exit(5);
	}

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
        if(sem_post(semPTR_work) == -1)
        {
            perror("Semop returned error2.");
            exit(5);
        }

        if(sem_wait(semPTR_work) == -1)
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
	get_around_semaphores();



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
