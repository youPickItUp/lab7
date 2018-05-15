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
#include <sys/wait.h>
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


//time functions
void print_stamp()
{
	struct timespec measure;
	if(clock_gettime(CLOCK_MONOTONIC, &measure) == -1)
	{
		perror("Time measure problem.");
		return;
	}
	printf(RESET);
	printf("[%lds:%ldms][%d]", measure.tv_sec, measure.tv_nsec/1000, getpid());
}


//atexit functions
void munmap_shm()
{
	if(munmap((void*) shmPTR, shm_len) == -1) perror("Couldn't unmap shm.");
}

void close_sem_sleep()
{
	if(sem_close(semPTR_sleep) == -1) perror("Couldn't close semaphore.");
}

void close_sem_work()
{
	if(sem_close(semPTR_work) == -1) perror("Couldn't close semaphore.");
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
		fprintf(stderr, RED "Given numbers should be positive.\n" RESET);
		exit(0);
	}

	return arg_long;
}

//attach shared memory
//shmID = memory ID; shmPTR = memory pointer
void get_around_memory()
{
	//create shared memory
	if((shmID = shm_open(PATH1, O_RDWR, 0)) == -1)
		exit_error("Shared mem open error.");

	//get memory length
	struct stat buf;
	if(fstat(shmID, &buf) == -1)
		exit_error("Couldn't get shared memory length.");
	shm_len = buf.st_size;

	//map shared memory
	if((shmPTR = (struct q_memory*) mmap(NULL, shm_len, PROT_READ | PROT_WRITE,
		 								MAP_SHARED, shmID, 0)) == (void*) -1)
		exit_error("Couldn't map shm.");
	atexit(munmap_shm);
}

//semID = semaphore ID
//atexit - close semaphore
void get_around_semaphore()
{
	//get semaphore ID
	if((semPTR_sleep = sem_open(PATH1, 0, 0, 0)) == SEM_FAILED)
		exit_error("Couldn't get semaphore ID.");
	atexit(close_sem_sleep);

	//get semaphore ID
	if((semPTR_work = sem_open(PATH2, 0, 0, 1)) == SEM_FAILED)
		exit_error("Couldn't get semaphore ID.");
	atexit(close_sem_work);
}

//wait for barber to end his part (he sets 'W')
void barbers_move()
{
	shmPTR->barber_state = 'w';
    while(shmPTR->barber_state != 'W')
    {
        //give barber initiative
        if(sem_post(semPTR_work) == -1)
        {
            perror("Semop returned error2.");
            exit(5);
        }

        //wait for him to react
        if(sem_wait(semPTR_work) == -1)
        {
            perror("Semop returned error2.");
            exit(5);
        }
    }
}

//name is self-explanatory
void wait_for_your_turn()
{
	while(shmPTR->current_client != getpid())
	{
		//give barber initiative
		if(sem_post(semPTR_work) == -1)
		{
			perror("Semop returned error2.");
			exit(5);
		}

		//wait for him to react
		if(sem_wait(semPTR_work) == -1)
		{
			perror("Semop returned error2.");
			exit(5);
		}
	}
}


int main(int argc, char *argv[])
{
	//parse argument
	if(argc != 3)
	{
		fprintf(stderr, RED "Wrong arguments number. Given %d, should be 2"
							" - clients and shaves number.\n" RESET, argc-1);
		exit(0);
	}
	long clients_num = parse_arg(argv[1]);
    long rounds_num = parse_arg(argv[2]);

    //fork children
    int fork_val = 0;
    for(int i=0;i<clients_num;i++)
    {
        fork_val = fork();
        if(fork_val == -1)
            exit_error("Fork failed.");
        if(fork_val == 0) break;
    }

    //end parent process after last child death
    if(fork_val != 0)
    {
        for(int i=0;i<clients_num;i++)
        {
            wait(NULL);
        }
        exit(0);
    }

    //childish things like:
	//attach shared memory
	//shmID = memory ID; shmPTR = memory pointer
	//add detach to atexit
	get_around_memory();

	//semID = semaphore ID
	//atexit - close semaphore
	get_around_semaphore();

	//client starts working
	for(int i=0;i<rounds_num;i++)
	{
        //wait for resources
		if(sem_wait(semPTR_work) == -1)
		{
			perror("Semop returned error2.");
			exit(5);
		}

        //if barber sleeps
        if(shmPTR->barber_state == 's')
        {
			print_stamp();
            printf(GREEN "Pobudka!\n" RESET);
            shmPTR->current_client = getpid();

			if(sem_post(semPTR_sleep) == -1)
			{
				perror("Semop returned error2.");
				exit(5);
			}

            //wait for reaction
            barbers_move();

            //sit down
			print_stamp();
            printf( "Usiadlem.\n" );

            //wait for reaction
            barbers_move();

            //leave barber
			print_stamp();
            printf(BLUE "Wychodze!\n" RESET);
            shmPTR->current_client = -1;

			//mark work ended
			shmPTR->barber_state = 'w';

            //give initiative
            if(sem_post(semPTR_work) == -1)
            {
                perror("Semop returned error2.");
                exit(5);
            }

            continue;
        }

        if(shmPTR->chairs_num == shmPTR->chairs_taken)
        {
			print_stamp();
            printf(MAGENTA "Kolejka pelna.\n" RESET);
			if(sem_post(semPTR_work) == -1)
			{
				perror("Semop returned error2.");
				exit(5);
			}
			continue;
        }

		//take place in queue
		print_stamp();
		printf(GREEN "Czekam\n" RESET);

		++shmPTR->chairs_taken;
		shmPTR->pids_queue[shmPTR->first_free_chair] = getpid();
		shmPTR->first_free_chair =
							(shmPTR->first_free_chair + 1) % shmPTR->chairs_num;

		//quite explicit name so...
		wait_for_your_turn();

		//enter and perform changes in queue
		print_stamp();
		printf( "Usiadlem.\n");
		--shmPTR->chairs_taken;
		shmPTR->first_in_queue =
							(shmPTR->first_in_queue + 1) % shmPTR->chairs_num;


		//let him shave
		barbers_move();

		//leave barber
		print_stamp();
		printf(BLUE "Wychodze!\n" RESET);
		shmPTR->current_client = -1;

		//mark work ended
		shmPTR->barber_state = 'w';

		//give initiative
		if(sem_post(semPTR_work) == -1)
		{
			perror("Semop returned error2.");
			exit(5);
		}
	}

	exit(0);
}
