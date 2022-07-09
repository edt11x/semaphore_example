
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>     // sleep(), ftruncate()
#include <sys/types.h>  // ftruncate()

// stuff for POSIX threads
#include <pthread.h>
// stuff for Semaphores
#include <semaphore.h>

// stuff for POSIX shared memory objects
#include <sys/mman.h>
#include <sys/stat.h>   // mode constants
#include <fcntl.h>      // file handling constants

#define MAX_EVENT_ARRAY (10240U)
#define MAX_SLEEP_TIME  ((1U << 19)-1U)

enum thread_states { UNINITIALIZED, IN_THREAD, OPENING_SEM, WAITING_ON_SEM, HAS_SEM,
                     RELEASE_SEM };
struct event_list
{
    const char * name;
    enum thread_states new_state;
} list_of_events[MAX_EVENT_ARRAY];

unsigned int this_event_idx = 0U;   // points at the place to record the next event
sem_t * sem = NULL;                 // The named semaphore in shared memory
const char * main_name = "Main";    // The name of the first thread, the main thread
const char * second_name = "Second";// The name of the second thread

extern void die(const char * message);
void die(const char * message) // subroutine to reduce the lines of code
{
    perror(message);
    exit(1);
}

// This is the important part, creating a named shared memory semaphore that
// can be shared between processes.
//
// LOOK HERE LOOK HERE
// I am not going to use sem_open(), but rather the individual steps since I
// think it explains the shared memory and file descriptor access in a better
// way. 
extern sem_t * my_fake_sem_open(void);
sem_t * my_fake_sem_open(void)
{
    int shm = 0;
    sem_t * semaphore = NULL;

    if ((shm = shm_open("MY_NAMED_SEM", O_RDWR | O_CREAT, S_IRWXU)) < 0)
    {
        die("shm_open() failed to create MY_NAMED_SEM");
    }
    if (ftruncate(shm, sizeof(sem_t)) < 0) // Note we are using a file operation on shm as a fd
    {
        die("ftruncate() failed");
    }
    // map the shared memory into our process
    if ((semaphore = mmap(NULL, sizeof(sem_t), PROT_READ | PROT_WRITE,
                    MAP_SHARED, shm, 0)) == MAP_FAILED)
    {
        die("mmap() failed");
    }
    // initialize the semaphore in shared memory
    if (sem_init(semaphore, 1, 1) < 0)
    {
        die("sem_init() failed");
    }
    return semaphore;
}

extern void put_event(const char * name, enum thread_states this_state);
void put_event(const char * name, enum thread_states this_state)
{
    // We use this routine, put_event, to record the event that has happened
    // so we are not manipulating charater strings or using printf() in a 
    // thread. This should be relatively fast and should mostly work.
    //
    // This is still a race condition, since we could get our event index we
    // are going to use, and get descheduled before the next line.  Then the
    // other thread could come in and populate the event with the exact same
    // index we are going to use and we could pick up running overwriting what
    // the other thread wrote.I should use another semaphore or the bakery
    // algorithm to handle this, but I am trying to leave this example somewhat
    // readable and digestable.  So I will leave this race condition.
    unsigned int our_event_idx = this_event_idx;
    unsigned int new_event_idx = (++this_event_idx) & (MAX_EVENT_ARRAY - 1U);
    list_of_events[our_event_idx].name = name;
    list_of_events[our_event_idx].new_state = this_state;
    this_event_idx = new_event_idx;
}

extern void * p_main(void * p);
void * p_main(void * p) // Main thread
{
    put_event(main_name, IN_THREAD);
    if (p)
    {
        printf("Unexpected parameter in p_main()\n");
        return(NULL);
    }
    for (;;)
    {
        put_event(main_name, WAITING_ON_SEM);
        sem_wait(sem);
        put_event(main_name, HAS_SEM);
        usleep((unsigned int)rand() & MAX_SLEEP_TIME); // sleep a substitute for crit region work
        put_event(main_name, RELEASE_SEM);
        sem_post(sem);
        usleep((unsigned int)rand() & MAX_SLEEP_TIME); // sleep to give others time to run
    }
    return(NULL);
}

extern void * p_second(void * p);
void * p_second(void * p) // Secondary thread
{
    put_event(second_name, IN_THREAD);
    if (p)
    {
        printf("Unexpected parameter in p_second()\n");
        return(NULL);
    }
    for (;;)
    {
        put_event(second_name, WAITING_ON_SEM);
        sem_wait(sem);
        put_event(second_name, HAS_SEM);
        usleep((unsigned int)rand() & MAX_SLEEP_TIME); // sleep a substitute for crit region work
        put_event(second_name, RELEASE_SEM);
        sem_post(sem);
        usleep((unsigned int)rand() & MAX_SLEEP_TIME); // sleep to give others time to run
    }
    return(NULL);
}

extern void report_state(const char * thread_name, enum thread_states this_state);
void report_state(const char * thread_name, enum thread_states this_state)
{
    if (thread_name)
    {
        switch(this_state)
        {
            case UNINITIALIZED:
                printf("%s thread is uninitialized\n", thread_name);
                break;
            case IN_THREAD:
                printf("In the %s thread\n", thread_name);
                break;
            case OPENING_SEM:
                printf("%s thread is opening the semaphore\n", thread_name);
                break;
            case WAITING_ON_SEM:
                printf("%s thread is WAITING on the semaphore\n", thread_name);
                break;
            case HAS_SEM:
                printf("%s has aquired the semaphore, it is in the critical region\n",
                        thread_name);
                break;
            case RELEASE_SEM:
                printf("%s is releasing the semaphore, it has left the critical region\n",
                        thread_name);
                break;
            default:
                printf("%s is in an unknown state\n", thread_name);
                break;
        }
    }
}

int main(int argc, char ** argv)
{
    pthread_t pthread_main, pthread_second;
    unsigned int i = 0U;

    if (argc > 1)
    {
        printf("No options, argc %d\n", argc);
        exit(1);
    }
    if (!argv)
    {
        printf("Pointer to argv is NULL\n");
        exit(1);
    }
    for (i = 0U; i < MAX_EVENT_ARRAY; i++) // initialize the event array with valid data
    {
        list_of_events[i].name = "UNINITIALIZED";
        list_of_events[i].new_state = UNINITIALIZED;
    }
    put_event(main_name, OPENING_SEM);
    sem = my_fake_sem_open();
    this_event_idx = 0U;

    if (pthread_create(&pthread_main, NULL, p_main, NULL))
    {
        die("pthread_create() failed for main");
    }
    if (pthread_create(&pthread_second, NULL, p_second, NULL))
    {
        die("pthread_create() failed for main");
    }

    for (i = 0U;;) // this main loop spins hard, and we could potentially get so
    {        // far behind that we actually lose the whole arrays worth of events
        if (i != this_event_idx)
        {
            report_state(list_of_events[i].name, list_of_events[i].new_state);
            i = (i + 1) & (MAX_EVENT_ARRAY - 1U);
        }
    }
    exit(0);
}
