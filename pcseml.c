#include <stdio.h>
#include <stdlib.h>
#include "eventbuf.h"
#include <pthread.h>
#include <semaphore.h>
#include <fcntl.h>
#include <errno.h>

#define EXIT_FAILURE 1
#define EXIT_SUCCESS 0

struct eventbuf *eb;
int num_consumers;
int num_producers;
int max_outstanding;
int num_events;

sem_t *mutex, *items, *spaces;

sem_t *sem_open_temp(const char *name, int value){
    sem_t *sem;

    // Create the semaphore
    if ((sem = sem_open(name, O_CREAT, 0600, value)) == SEM_FAILED)
        return SEM_FAILED;

    // Unlink it so it will go away after this process exits
    if (sem_unlink(name) == -1) {
        sem_close(sem);
        return SEM_FAILED;
    }

    return sem;
}

// Producer function to add events to buffer
void *producer(void *arg) {
    // Extract the producer ID from the void pointer
    int id = *((int*) arg);

    // Generate num_events events and add them to the buffer
    for (int i = 0; i < num_events; i++) {
        // Calculate the unique event number for this producer
        int event_num = id * 100 + i;

        // Wait until there is an available space in the buffer
        sem_wait(spaces);

        // Wait until the mutex is available
        sem_wait(mutex);

        // Add the event to the buffer and print a message
        eventbuf_add(eb, event_num);
        printf("P%d: adding event %d\n", id, event_num);

        // Release the mutex and signal that an item has been added
        sem_post(mutex);
        sem_post(items);
    }

    // Print exit message and return
    printf("P%d: exiting\n", id);
    return NULL;
}

// Consumer function to consume events from buffer
void *consumer(void *arg) {
    // Extract the consumer ID from the void pointer
    int id = *((int*) arg);

    while (1) {
        // Wait for an available event to consume
        sem_wait(items);

        // Acquire the lock
        sem_wait(mutex);

        // Check if the buffer is empty and exit if it is
        if (eventbuf_empty(eb)) {
            sem_post(mutex);
            break;
        }

        // Consume the next event from the buffer and print a message
        int event_num = eventbuf_get(eb);
        printf("C%d: got event %d\n", id, event_num);

        // Release the lock and signal that a space is available
        sem_post(mutex);
        sem_post(spaces);
    }

    // Print exit message and return
    printf("C%d: exiting\n", id);
    return NULL;
}

// Creates multiple threads
void create_threads(pthread_t *thread, int *thread_id, int num_threads, void* (*start_routine)(void*)){
    // Loop through each thread
    for (int i = 0; i < num_threads; i++) {
        thread_id[i] = i;
        pthread_create(thread + i, NULL, start_routine, thread_id + i);
    }
}

void join_threads(pthread_t *thread, int num_threads){
  // Wait for threads to complete execution
    for (int i = 0; i < num_threads; i++)
        pthread_join(thread[i], NULL);
}

void unblock_consumer_threads() {
  // Unblock all consumer threads waiting for items
    for (int i = 0; i < num_consumers; i++) {
        sem_post(items);
    }
}

void print_usage_and_exit(int argc) {
    // Check if the correct number of command-line arguments have been passed
    if (argc != 5) {
        // Print an error message to stderr and exit with a non-zero status code
        fprintf(stderr, "usage: pcseml num_producers num_consumers num_events max_outstanding \n");
        exit(1);
    }
}

// Convert command-line arguments to integers and assign them to variables
void parse_arguments(char* argv[]) {
    num_producers = atoi(argv[1]);
    num_consumers = atoi(argv[2]);
    num_events = atoi(argv[3]);
    max_outstanding = atoi(argv[4]);
}

// Initialize synchronization objects
void initialize_synchronization_objects(int max_outstanding) {
    eb = eventbuf_create();
    mutex = sem_open_temp("program-3-mutex", 1);
    items = sem_open_temp("program-3-items", 0);
    spaces = sem_open_temp("program-3-spaces", max_outstanding);
}

// Create and start producer or consumer threads
void create_and_start_threads(pthread_t *thread, int *thread_id, int num_threads, void* (*start_routine)(void*)) {
    for (int i = 0; i < num_threads; i++) {
        thread_id[i] = i;
        pthread_create(&thread[i], NULL, start_routine, &thread_id[i]);
    }
}

int main(int argc, char *argv[]) {
    // Check if the correct number of command-line arguments have been passed
    print_usage_and_exit(argc);

    // Convert command-line arguments to integers and assign them to variables
    parse_arguments(argv);

    // Initialize synchronization objects
    initialize_synchronization_objects(max_outstanding);

    // Create and start producer threads
    pthread_t prod_thread[num_producers];
    int prod_thread_id[num_producers];
    create_and_start_threads(prod_thread, prod_thread_id, num_producers, producer);

    // Create and start consumer threads
    pthread_t cons_thread[num_consumers];
    int cons_thread_id[num_consumers];
    create_and_start_threads(cons_thread, cons_thread_id, num_consumers, consumer);

    // Wait for all producer threads to finish
    join_threads(prod_thread, num_producers);

    // Unblock all consumer threads by posting to the items semaphore
    unblock_consumer_threads();

    // Wait for all consumer threads to finish
    join_threads(cons_thread, num_consumers);

    // Free resources
    eventbuf_free(eb);

    return 0;
}
