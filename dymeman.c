/*
TODO: 
Create an array to store everything that's to be freed but not counted, so for internal use.
Finish benchmarks.
Create unique tag strings and return them from benchmark_create(), so users don't have to make them up and have no
possibility of overriding previous tags.
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>
#include <time.h>

#define THREAD_SAFE "TRUE"
#define THREAD_UNSAFE "FALSE"

static unsigned int safety_switch_count = 0;
static bool thread_safe = true;
void thread_safety(bool active) {
    if (active != thread_safe) safety_switch_count++;
    if (active) thread_safe = false;
    else thread_safe = true;
}

static unsigned int total_blocks_allocated = 0;
static unsigned int total_memory_allocated = 0;

static unsigned int count = 0;
static void **manager = NULL;
static pthread_mutex_t thread_lock = PTHREAD_MUTEX_INITIALIZER;

static void s_free_all() {
    if (!manager) return;

    unsigned int freed_count;
    unsigned int freed_size;

    for (unsigned int i=0; i<count; i++) {
        // in case weird shit happens in other files, we first check whether manager[i] exists
        if (manager[i]) {
            freed_size += sizeof(manager[i]);
            free(manager[i]);
            manager[i] = NULL;
            freed_count++;
        }
    }

    freed_size += sizeof(void *)*freed_count;
    free(manager);
    manager = NULL;
    freed_count++;

    static char threadSafetyFlag = '✓';
    if (!thread_safe) threadSafetyFlag = "X";

    // TODO: finish adding times, alloced memory
    printf("\n\n\
Dingleberry - PROGRAM EXECUTION TERMINATED\n\n\
\% THREAD_SAFETY: %c (switched %i times)\n\
-> Total heap blocks allocated: %i\n\
-> Total heap blocks freed: %i\n\
-> Total heap size allocated: %i bytes \n\
-> Total heap size freed: %i bytes\n\
-------------------------------------------\n\
\% ALLOC_TIME_TRACK: %c\n\
):\n\
-> Total execution time: \n\
", &threadSafetyFlag, &safety_switch_count, &total_blocks_allocated, &freed_count, &total_memory_allocated, &freed_size);
}

bool atexit_active = false;
void* s_malloc(unsigned int size) {
    if(!atexit_active) {
        atexit(s_free_all);
        atexit_active = true;
    }

    if (thread_safe) pthread_mutex_lock(&thread_lock);

    void **temp = (void **)realloc(manager, (count+1)*sizeof(void *));
    if (!temp) {
        printf("Manager realloc failed. Remained the same size.\n");
        pthread_mutex_unlock(&thread_lock);
        return NULL;
    }

    void *newMem = (void *)malloc(size);
    if (!newMem) {
        printf("NewMem alloc failed. Remained uninitialised. Realloc cancelled.\n");
        pthread_mutex_unlock(&thread_lock);
        return NULL;
    }

    manager = temp;
    manager[count] = newMem;
    count++;

    total_blocks_allocated++;
    total_memory_allocated += size;

    pthread_mutex_unlock(&thread_lock);

    return newMem;
}

void* s_calloc(unsigned int n, unsigned int size) {
    if (!atexit_active) {
        atexit(s_free_all);
        atexit_active = true;
    }

    if (thread_safe) pthread_mutex_lock(&thread_lock);

    void **temp = (void **)realloc(manager, (count+1)*size);
    if (!temp) {
        printf("Manager realloc failed. Remained the same size.\n");
        pthread_mutex_unlock(&thread_lock);
        return NULL;
    }
    
    void *newMem = (void *)calloc(n, size);
    if (!newMem) {
        printf("NewMem calloc failed. Remained uninitialised. Realloc cancelled.\n");
        pthread_mutex_unlock(&thread_lock);
        return NULL;
    }

    manager = temp;
    manager[count] = newMem;
    count++;

    total_blocks_allocated++;
    total_memory_allocated+=size;

    pthread_mutex_unlock(&thread_lock);

    return newMem;
}

void s_free(void *p) {
    if (thread_safe) pthread_mutex_lock(&thread_lock);

    for (unsigned int i = 0; i < count; i++) {
        if (manager[i] == p) {
            total_blocks_allocated--;
            total_memory_allocated-=sizeof(p);
            free(p);
            manager[i] = NULL;
            return;
        }
    }

    pthread_mutex_unlock(&thread_lock);
}

typedef struct {
    bool running;
    unsigned char *tag;
    clock_t start;
    clock_t finish;
} time_bench;

/* Tag uniqueness is NOT ensured. Might implement in the future. */
unsigned int timers_count = 0;
time_bench *benchmarks;
void benchmark_create(char *tag) {
    if (!benchmarks) {
        benchmarks = (time_bench *)realloc(timers_count+1, sizeof(time_bench *)); // not sure if it's right
        if (!benchmarks) printf("Benchmarks realloc failed. Continuing execution normally.\n");
        return;
    }
    time_bench *newBench = (time_bench *)malloc(sizeof(time_bench));
    if (!newBench) printf("Benchmark malloc failed.");

    // TODO: finish
}

void stop_benchmark(char *tag) {
    if (thread_safe) pthread_mutex_lock(&thread_lock);

    for (unsigned int i=0; i<timers_count; i++) {
        if (benchmarks[i].tag == tag) {
            if (!benchmarks[i].running) break;
            benchmarks[i].running = false;
            benchmarks[i].finish = clock();
        }
    }

    pthread_mutex_unlock(&thread_lock);
}

void stop_benchmark_all() {
    if (thread_safe) pthread_mutex_lock(&thread_lock);

    for (unsigned int i=0; i<timers_count; i++) {
        if (benchmarks[i].running) {
            benchmarks[i].running = false;
            benchmarks[i].finish = clock();
        }
    }

    pthread_mutex_unlock(&thread_lock);
}