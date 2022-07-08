
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> // sleep(), ftruncate()
#include <sys/types.h> // ftruncate()

// stuff for POSIX threads
#include <pthread.h>
#include <semaphore.h>

// stuff for POSIX shared memory objects
#include <sys/mman.h>
#include <sys/stat.h> // mode constants
#include <fcntl.h>    // file handling constants

// this is the important part, creating a named shared memory semaphore that
// can be shared between processes
//
// LOOK HERE LOOK HERE
// I am not going to use sem_open(), but rather the individual steps since
// I think it explains the shared memory and file descriptor access in
// a better way.
extern sem_t * my_fake_sem_open(void);
sem_t * my_fake_sem_open(void)
{
    int shm = 0;
    sem_t * semaphore = NULL;

    if ((shm = shm_open("MY_NAMED_SEM", O_RDWR | O_CREAT, S_IRWXU)) < 0)
    {
        perror("shm_open() failed to create MY_NAMED_SEM");
        exit(1);
    }
    // Note we are using a file operation on shm as a file descriptor
    if (ftruncate(shm, sizeof(sem_t)) < 0)
    {
        perror("ftruncate() failed");
        exit(1);
    }
    // map the shared memory into our process
    if ((semaphore = mmap(NULL, sizeof(sem_t), PROT_READ | PROT_WRITE,
                    MAP_SHARED, shm, 0)) == MAP_FAILED)
    {
        perror("mmap() failed");
        exit(1);
    }
    // initialize the semaphore in shared memory
    if (sem_init(semaphore, 1, 1) < 0)
    {
        perror("sem_init() failed");
        exit(1);
    }

    return semaphore;
}

extern void * p_main(void * p);
void * p_main(void * p)
{
    sem_t * sem = NULL;
    if (p)
    {
        printf("Unexpected parameter in p_main()\n");
        return(NULL);
    }
    printf("In p_main()\n"); // never use printf in a thread...
    fflush(stdout);
    // suspend ourselves for coordinated initialization.
    printf("We are running, pthread_main()\n");
    fflush(stdout);
    printf("opening the named file system semaphore\n");
    sem = my_fake_sem_open();
    for (;;)
    {
        printf("p_main waiting\n");
        fflush(stdout);
        sem_wait(sem);
        printf("p_main has the semaphore\n");
        fflush(stdout);
        sleep(2); // just sleeping to see things happening a bit better
        printf("p_main leaving critical section\n");
        fflush(stdout);
        sem_post(sem);
        sleep(1);
    }
    return(NULL);
}

extern void * p_second(void * p);
void * p_second(void * p)
{
    sem_t * sem = NULL;
    if (p)
    {
        printf("Unexpected parameter in p_second()\n");
        return(NULL);
    }
    printf("In p_second()\n"); // never use printf in a thread...
    fflush(stdout);
    // suspend ourselves for coordinated initialization.
    printf("We are running, pthread_block()\n");
    printf("opening the named file system semaphore\n");
    sem = my_fake_sem_open();
    for (;;)
    {
        printf("p_second waiting\n");
        fflush(stdout);
        sem_wait(sem);
        printf("p_second has the semaphore\n");
        fflush(stdout);
        sleep(2);
        printf("p_second leaving critical section\n");
        fflush(stdout);
        sem_post(sem);
        sleep(1);
    }
    return(NULL);
}

int main(int argc, char ** argv)
{
    pthread_t pthread_main, pthread_second;

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
    printf("Hello World\n");
    if (pthread_create(&pthread_main, NULL, p_main, NULL))
    {
        perror("pthread_create() failed for main");
        exit(1);
    }

    sleep(1);
    if (pthread_create(&pthread_second, NULL, p_second, NULL))
    {
        perror("pthread_create() failed for main");
        exit(1);
    }
    sleep(10); // just run the whole thing for 10 seconds:w

    exit(0);
}
