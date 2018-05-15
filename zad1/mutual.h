#ifndef _MUTUAL


#define RESET   "\033[0m"

#define RED     "\033[1m\033[31m"
#define GREEN   "\033[1m\033[32m"
#define BLUE    "\033[1m\033[34m"
#define MAGENTA "\033[1m\033[35m"



#define PATH "/abstractpath"

union semun {
	int val;
	struct semid_ds *buf;
	unsigned short *array;
	struct seminfo *__buf;
};

struct q_memory {
	int chairs_taken;
	int chairs_num;
	int first_free_chair;
	int first_in_queue;
	int barber_state;
	int current_client;

	int pids_queue[];
};

char to_key = 'a';

#endif
