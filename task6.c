
//compile with: g++ -lpthread <sourcename> -o <executablename>

//This exercise shows how to schedule threads with Rate Monotonic

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <unistd.h>
#include <math.h>
#include <string.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>

#define INNERLOOP 1000
#define OUTERLOOP 2000

#define NPERIODICTASKS 3
#define NAPERIODICTASKS 0
#define NTASKS NPERIODICTASKS + NAPERIODICTASKS

long int periods[NTASKS];
struct timespec next_arrival_time[NTASKS];
double WCET[NTASKS];
pthread_attr_t attributes[NTASKS];
int mutex_protocol;
pthread_t thread_id[NTASKS];
struct sched_param parameters[NTASKS];
int missed_deadlines[NTASKS];

//code of periodic tasks
void task1_code( );
void task2_code( );
void task3_code( );

//characteristic function of the thread, only for timing and synchronization
//periodic tasks
void *task1( void *);
void *task2( void *);
void *task3( void *);

// if taski_finished==0, taski is not finished yet.
int task1_finished=0;
int task2_finished=0;
// call system call
int write_to_simple(char str[]);

// initialization of mutexes
pthread_mutex_t task123_mutex     = PTHREAD_MUTEX_INITIALIZER;
//dclare a mutex attribute object
pthread_mutexattr_t mutexattr_prioceiling;
// initialization of condition variable
pthread_cond_t  condition_var_task2   = PTHREAD_COND_INITIALIZER;
pthread_cond_t  condition_var_task3   = PTHREAD_COND_INITIALIZER;
// the iteration of implement the task at the same time.
int interation=100;
// to save all the characters
char str[1000000];

int main()
{
  	// set task periods in nanoseconds
	//the first task has period 300 millisecond
	//the second task has period 500 millisecond
	//the third task has period 800 millisecond
	//you can already order them according to their priority;
	//if not, you will need to sort them
  	periods[0]= 300000000; //in nanoseconds
  	periods[1]= 500000000; //in nanoseconds
  	periods[2]= 800000000; //in nanoseconds

	//this is not strictly necessary, but it is convenient to
	//assign a name to the maximum and the minimum priotity in the
	//system. We call them priomin and priomax.

  	struct sched_param priomax;
  	priomax.sched_priority=sched_get_priority_max(SCHED_FIFO);
  	struct sched_param priomin;
  	priomin.sched_priority=sched_get_priority_min(SCHED_FIFO);

	// set the maximum priority to the current thread (you are required to be
  	// superuser). Check that the main thread is executed with superuser privileges
	// before doing anything else.

  	if (getuid() == 0)
    		pthread_setschedparam(pthread_self(),SCHED_FIFO,&priomax);

  	// execute all tasks in standalone modality in order to measure execution times
  	// (use gettimeofday). Use the computed values to update the worst case execution
  	// time of each task.

 	int i;
  	for (i =0; i < NTASKS; i++)
    	{

		// initializa time_1 and time_2 required to read the clock
		struct timespec time_1, time_2;
		clock_gettime(CLOCK_REALTIME, &time_1);

		//we should execute each task more than one for computing the WCET
		//periodic tasks
 	     	if (i==0)
			task1_code();
      		if (i==1)
			task2_code();
      		if (i==2)
			task3_code();

		clock_gettime(CLOCK_REALTIME, &time_2);


		// compute the Worst Case Execution Time (in a real case, we should repeat this many times under
		//different conditions, in order to have reliable values

      		WCET[i]= 1000000000*(time_2.tv_sec - time_1.tv_sec)
			       +(time_2.tv_nsec-time_1.tv_nsec);
      		printf("\nWorst Case Execution Time %d=%f \n", i, WCET[i]);
    	}

    	// compute U
	double U = WCET[0]/periods[0]+WCET[1]/periods[1]+WCET[2]/periods[2];

	// There are no harmonic relationships, use the following formula instead
	double Ulub = NPERIODICTASKS*(pow(2.0,(1.0/NPERIODICTASKS)) -1);

	//check the sufficient conditions: if they are not satisfied, exit
  	if (U > Ulub)
    	{
      		printf("\n U=%lf Ulub=%lf Non schedulable Task Set", U, Ulub);
      		return(-1);
    	}
  	printf("\n U=%lf Ulub=%lf Scheduable Task Set", U, Ulub);
  	fflush(stdout);
  	sleep(5);

  	// set the minimum priority to the current thread: this is now required because
	//we will assign higher priorities to periodic threads to be soon created
	//pthread_setschedparam

  	if (getuid() == 0)
    		pthread_setschedparam(pthread_self(),SCHED_FIFO,&priomin);

  	// set the attributes of each task, including scheduling policy and priority
  	for (i =0; i < NPERIODICTASKS; i++)
    	{
		//initialize the attribute structure of task i
      	pthread_attr_init(&(attributes[i]));

		//set the attributes to tell the kernel that the priorities and policies are explicitly chosen,
		//not inherited from the main thread (pthread_attr_setinheritsched) 
      	pthread_attr_setinheritsched(&(attributes[i]), PTHREAD_EXPLICIT_SCHED);

		// set the attributes to set the SCHED_FIFO policy (pthread_attr_setschedpolicy)
		pthread_attr_setschedpolicy(&(attributes[i]), SCHED_FIFO);

		//properly set the parameters to assign the priority inversely proportional 
		//to the period
      	parameters[i].sched_priority = priomin.sched_priority+NTASKS - i;

		//set the attributes and the parameters of the current thread (pthread_attr_setschedparam)
      		pthread_attr_setschedparam(&(attributes[i]), &(parameters[i]));
    	}
	// set the mutex of each task for priority ceiling
	//initialize attribute
	pthread_mutexattr_init(&mutexattr_prioceiling);
	pthread_mutexattr_getprotocol(&mutexattr_prioceiling, &mutex_protocol);
	// set the protocol attribute of mutex created by the function pthread_mutexattr_init()
	//set the PTHREAD_PRIO_PROTECT
	pthread_mutexattr_setprotocol(&mutexattr_prioceiling, PTHREAD_PRIO_PROTECT);
	// set to a priority ceiling which is the highest priority among all the threads that may lock that mutex.
	pthread_mutexattr_setprioceiling(&mutexattr_prioceiling,priomax.sched_priority);
	//initialize mutex
	pthread_mutex_init(&task123_mutex, &mutexattr_prioceiling);

	//delare the variable to contain the return values of pthread_create
  	int iret[NTASKS];

	//declare variables to read the current time
	struct timespec time_1;
	clock_gettime(CLOCK_REALTIME, &time_1);

  	// set the next arrival time for each task. This is not the beginning of the first
	// period, but the end of the first period and beginning of the next one. 
  	for (i = 0; i < NPERIODICTASKS; i++)
    	{
		long int next_arrival_nanoseconds = time_1.tv_nsec + periods[i];
		//then we compute the end of the first period and beginning of the next one
		next_arrival_time[i].tv_nsec= next_arrival_nanoseconds%1000000000;
		next_arrival_time[i].tv_sec= time_1.tv_sec + next_arrival_nanoseconds/1000000000;
       		missed_deadlines[i] = 0;
    	}

	// create all threads(pthread_create)
  	iret[0] = pthread_create( &(thread_id[0]), &(attributes[0]), task1, NULL);
  	iret[1] = pthread_create( &(thread_id[1]), &(attributes[1]), task2, NULL);
  	iret[2] = pthread_create( &(thread_id[2]), &(attributes[2]), task3, NULL);

  	// join all threads (pthread_join)
  	pthread_join( thread_id[0], NULL);
  	pthread_join( thread_id[1], NULL);
  	pthread_join( thread_id[2], NULL);

  	// set the next arrival time for each task. This is not the beginning of the first
	// period, but the end of the first period and beginning of the next one.
  	for (i = 0; i < NTASKS; i++)
    	{
      		printf ("\nMissed Deadlines Task %d=%d", i, missed_deadlines[i]);
		fflush(stdout);
    	}
  	exit(0);
}

// application specific task_1 code
void task1_code()
{
	printf(" [1 "); fflush(stdout);
	// char str1_1[]="[1";
	//when you do system call "read", if you want to show the all characters which are written.
	strcat(str, "[1");

	// call system call for writing
	write_to_simple(str);

	//this double loop with random computation is only required to waste time
	int i,j;
	double uno;
  	for (i = 0; i < OUTERLOOP; i++)
    	{
      		for (j = 0; j < INNERLOOP; j++)
		{
			uno = rand()*rand()%10;
    		}
  	}
	// char str1_2[]="1]";
  	//when you do system call "read", if you want to show the all characters which are written.
	strcat(str, "1]");

	// call system call for writing
	write_to_simple(str);

  	printf(" 1] "); fflush(stdout);
}


//thread code for task_1 (used only for temporization)
void *task1( void *ptr)
{
	// set thread affinity, that is the processor on which threads shall run
	cpu_set_t cset;
	CPU_ZERO (&cset);
	CPU_SET(0, &cset);
	pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cset);

   	//execute the task one hundred times... it should be an infinite loop (too dangerous)
  	int i=0;
  	for (i=0; i < interation ; i++)
    	{
		//Lock mutex
		pthread_mutex_lock( &task123_mutex );
		// execute application specific code
		task1_code();
		// Signal the condition to other tasks.
		pthread_cond_signal( &condition_var_task2);
		// task1 is unlocked
		pthread_mutex_unlock( &task123_mutex );
		// sleep until the end of the current period (which is also the start of the
		// new one
		clock_nanosleep(CLOCK_REALTIME, TIMER_ABSTIME, &next_arrival_time[0], NULL);

        if(i==interation-1){
            task1_finished=1;
        }
		// the thread is ready and can compute the end of the current period for
		// the next iteration

		long int next_arrival_nanoseconds = next_arrival_time[0].tv_nsec + periods[0];
		next_arrival_time[0].tv_nsec= next_arrival_nanoseconds%1000000000;
		next_arrival_time[0].tv_sec= next_arrival_time[0].tv_sec + next_arrival_nanoseconds/1000000000;
    	}
}

void task2_code()
{
	// char str2_1[]="[2";
	printf(" [2 "); fflush(stdout);
	//when you do system call "read", if you want to show the all characters which are written.
	strcat(str, "[2");
	// call system call for writing
	write_to_simple(str);
  	int i,j;
	double uno;
  	for (i = 0; i < OUTERLOOP; i++)
    	{
      		for (j = 0; j < INNERLOOP; j++)
		{
			uno = rand()*rand()%10;
		}
    	}
	// char str2_2[]="2]";
	//when you do system call "read", if you want to show the all characters which are written.
	strcat(str, "2]");
	write_to_simple(str);
  	printf(" 2] "); fflush(stdout);

}


void *task2( void *ptr )
{
	// set thread affinity, that is the processor on which threads shall run
	cpu_set_t cset;
	CPU_ZERO (&cset);
	CPU_SET(0, &cset);
	pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cset);
	int i=0;
  	for (i=0; i < interation; i++)
    	{
		//Lock mutex
		pthread_mutex_lock( &task123_mutex );
		// wait the condition from high priority tasks
		if (task1_finished==0){
            //task2 is waiting"
			pthread_cond_wait( &condition_var_task2, &task123_mutex);
		}
		task2_code();
		// task2 is sending signal to 3"
		pthread_cond_signal( &condition_var_task2);
		pthread_mutex_unlock( &task123_mutex );
		if (i==interation-1){
            task2_finished=1;
        }
        clock_nanosleep(CLOCK_REALTIME, TIMER_ABSTIME, &next_arrival_time[1], NULL);
		long int next_arrival_nanoseconds = next_arrival_time[1].tv_nsec + periods[1];
		next_arrival_time[1].tv_nsec= next_arrival_nanoseconds%1000000000;
		next_arrival_time[1].tv_sec= next_arrival_time[1].tv_sec + next_arrival_nanoseconds/1000000000;
    	}

}

void task3_code()
{
	// char str3_1[]="[3";
	//when you do system call "read", if you want to show the all characters which are written.
	strcat(str, "[3");
	// call system call for writing
	write_to_simple(str);
  	printf(" [3 "); fflush(stdout);
	int i,j;
	double uno;
  	for (i = 0; i < OUTERLOOP; i++)
    	{
      		for (j = 0; j < INNERLOOP; j++);
			double uno = rand()*rand()%10;
    	}
	// char str3_2[]="3]";
	//when you do system call "read", if you want to show the all characters which are written.
	strcat(str, "3]");

	// call system call for writing
	write_to_simple(str);
  	printf(" 3]"); fflush(stdout);


}

void *task3( void *ptr)
{
	// set thread affinity, that is the processor on which threads shall run
	cpu_set_t cset;
	CPU_ZERO (&cset);
	CPU_SET(0, &cset);
	pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cset);

	int i=0;
  	for (i=0; i < interation; i++)
    	{
			//Lock mutex
			pthread_mutex_lock( &task123_mutex);
			// wait the condition from high priority tasks
            if (task2_finished==0){
                // task3 is waiting
				pthread_cond_wait( &condition_var_task2, &task123_mutex);
			}
      		task3_code();
			//unLock mutex
			pthread_mutex_unlock( &task123_mutex );
			clock_nanosleep(CLOCK_REALTIME, TIMER_ABSTIME, &next_arrival_time[2], NULL);
			long int next_arrival_nanoseconds = next_arrival_time[2].tv_nsec + periods[2];
			next_arrival_time[2].tv_nsec= next_arrival_nanoseconds%1000000000;
			next_arrival_time[2].tv_sec= next_arrival_time[2].tv_sec + next_arrival_nanoseconds/1000000000;
	}
}

//call_system_call
int write_to_simple(char str[])
{
	//fd=file descriptor
	int fd, result, len;

	char buf[1000];
		if ((fd =open("/dev/simple", O_RDWR|O_CREAT|O_APPEND, 0666)) == -1) {
				perror("open failed");
				return -1;
		}

	 len = strlen(str)+1;
         if ((result = write (fd, str, len)) != len)
	 {
                  perror("write failed");
                  return -1;
         }
	// printf("%d bytes written \n", result);
	close(fd);
}