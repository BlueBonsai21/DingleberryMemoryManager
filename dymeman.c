/*
TODO: 
Create unique tag strings and return them from benchmark_create(), so users don't have to make them up and have no
possibility of overriding previous tags.
Gotta add a detailed file+line report for auto_benchmark and thread_safety flags switches.
Make the AUTO_BENCHMARK be either global or local to the first memory management operation called in the program.
Add better documentation
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>
#include <time.h>
#include <string.h>

#include "dymeman.h";

bool final_report_flag = true; // flag
void final_report() {
    final_report_flag = false;
}

typedef struct {
    bool running;
    char *tag;
    clock_t start;
    clock_t finish;
} time_bench;
bool auto_benchmark_flag = false; // flag
bool auto_benchmarking = false;
unsigned int timers_count = 0;
time_bench **benchmarks = NULL;

void auto_benchmark() {
    auto_benchmark_flag = true;
}

#define FLAG_ACTIVE  "✓"
#define FLAG_INACTIVE "X"

static unsigned int safety_switch_count = 0;
static bool thread_safe = true; // flag
void thread_safety(bool state) {
    if (state != thread_safe) safety_switch_count++;
    thread_safe = !thread_safe;
}

typedef struct {
    bool busy;
    void *ptr;
    unsigned int size;
    const char *file;
    unsigned int line;
} mem_t;
static unsigned int total_blocks_allocated = 0;
static unsigned int total_memory_allocated = 0;

static unsigned int count = 0;
static mem_t *manager = NULL;
static pthread_mutex_t thread_lock = PTHREAD_MUTEX_INITIALIZER;

unsigned int freed_count = 0;
unsigned int freed_size = 0;

/* Function called by atexit(). Cleans up memory and gives a final report. */
static void clear() {
    if (!final_report_flag) {
        if (manager) {
            for (unsigned int i=0; i<count; i++) {
                if (manager[i].ptr) free(manager[i].ptr);
            }
            free(manager);
        }
        if (benchmarks) {
            for (unsigned int i=0; i<count; i++) {
                if (benchmarks[i]) free(benchmarks[i]);
            }
            free(benchmarks);
        }
        return;
    }

    if (manager) {
        for (unsigned int i=0; i<count; i++) {
            // in case weird shit happens in other files, we first check whether manager[i] exists
            if (manager[i].ptr) {
                freed_size += sizeof(manager[i].size);
                free(manager[i].ptr);
                manager[i].ptr = NULL;
                freed_count++;
            }
        }
    
        freed_size += sizeof(void *)*freed_count;
        free(manager);
        manager = NULL;
        freed_count++;
    }

    static char *threadSafetyFlag = FLAG_ACTIVE;
    if (!thread_safe) threadSafetyFlag = FLAG_INACTIVE;

    static char *autoBenchmarkFlag = FLAG_ACTIVE;
    if (!auto_benchmark) autoBenchmarkFlag = FLAG_INACTIVE;

    char report[0xfff]; // 4095 chars
    report[0] = '\0';
    strcat(report, "\
Dymeman - PROGRAM EXECUTION TERMINATED\n\
-------------------------------------------\n");

    char flags[1000];
    sprintf(flags, "\
1. FLAGS\n\
-> thread_safety: %s (switched %i during execution)\n\
-> auto_benchmark: %s\n\
-------------------------------------------\n",
threadSafetyFlag, safety_switch_count, autoBenchmarkFlag);

    char heap[1000];
    sprintf(heap, "\
2. HEAP\n\
-> Total allocations: %i\n\
-> Total frees: %i\n\
-> Total memory allocated (bytes): %i\n\
-> Total memory freed (bytes): %i\n\
-------------------------------------------\n",
total_blocks_allocated, freed_count, total_memory_allocated, freed_size);

    char benchmark_report[1000];
    strcat(benchmark_report, "3. BENCHMARKS\n");
    if (benchmarks) {
        for (unsigned int i=0; i<timers_count; i++) {
            sprintf(heap, "\
                #%i:\n\
                \tTag: %s\n\
                \tTime (ms): %i\n",
                i+1,
                benchmarks[i]->tag,
                benchmarks[i]->finish-benchmarks[i]->start);

            free(benchmarks[i]);
            benchmarks[i] = NULL;
        }
        free(benchmarks);
        benchmarks = NULL;
    } else {
        strcat(heap, "-> NO BENCHMARKS STARTED.");
    }
    strcat(benchmark_report, "-------------------------------------------\n");

    char notes[1000] = "4. NOTES\nMemory management is safe as long as this manager is used.\n\
Malloc, calloc, realloc, free are all overwritten via preprocessor directives and made safe.\n\
This doesn't mean that once you've stopped using this manager your program will run as intended.\n\
By turning off all flags you'll get the best results.\n\
This memory manager does NOT check for buffer overflows, nor allows multi-thread memory management.\n";

    strcat(report, flags);
    strcat(report, heap);
    strcat(report, benchmark_report);
    strcat(report, notes);

    printf("%s", report);
}

/* Check and returns the index of the first eventual free slot */
static unsigned int check_free_slots(unsigned int minSize) {
    pthread_mutex_lock(&thread_lock);

    for (unsigned int i=0; i<count; i++) {
        if (manager[i].ptr == NULL && !manager[i].busy && manager[i].size >= minSize) {
            manager[i].busy = true;
            return i;
        }
    }
    pthread_mutex_unlock(&thread_lock);

    return (unsigned int)-1;
}

/* Safely mallocs, adding the ptr to the manager array */
bool atexit_active = false;
static void* s_malloc(unsigned int size, const char *file, unsigned int line) {
    if(!atexit_active) {
        atexit(clear);
        atexit_active = true;
    }

    if (thread_safe) pthread_mutex_lock(&thread_lock);

    if (auto_benchmark_flag && !auto_benchmarking) {
        benchmark_create("AUTO_BENCHMARK_FLAG");
    }

    void *newPtr = NULL;
    unsigned int free_slot = check_free_slots(size);
    if (free_slot == (unsigned int)-1) {
        mem_t *temp = (mem_t *)realloc(manager, (count+1)*sizeof(mem_t));
        if (!temp) {
            printf("Manager realloc failed. Remained the same size (F: %s, L: %i).\n", file, line);

            pthread_mutex_unlock(&thread_lock);
            
            return NULL;
        }
    
        void *newPtr = (void *)malloc(size);
        if (!newPtr) {
            printf("NewMem alloc failed. Realloc cancelled (F: %s, L: %i).\n", file, line);

            pthread_mutex_unlock(&thread_lock);
            
            return NULL;
        }

        manager = temp;
        manager[count].size = size;
        manager[count].ptr = newPtr;
        manager[count].busy = false;
        manager[count].file = file;
        manager[count].line = line;
        
        total_blocks_allocated++;
        total_memory_allocated+=size;

        count++;

        pthread_mutex_unlock(&thread_lock);
    } else {
        manager[free_slot].busy = false;    
        manager[free_slot].file = file;
        manager[free_slot].line = line;

        newPtr = manager[free_slot].ptr;
    }
    
    pthread_mutex_unlock(&thread_lock);

    return newPtr; // that's the newly allocated memory if no free slot is found, else it's precisely that slot.
}

/* Safely callocs, adding the ptr to the manager array */
static void* s_calloc(unsigned int n, unsigned int size, const char *file, unsigned int line) {
    if (!atexit_active) {
        atexit(clear);
        atexit_active = true;
    }

    if (size <= 0) return NULL;

    if (thread_safe) pthread_mutex_lock(&thread_lock);

    if (auto_benchmark_flag && !auto_benchmarking) {
        benchmark_create("AUTO_BENCHMARK_FLAG");
    }

    mem_t *temp = (mem_t *)realloc(manager, (count+1)*sizeof(mem_t));
    if (!temp) {
        printf("Manager realloc failed (F: %s, L: %i).\n", file, line);

        pthread_mutex_unlock(&thread_lock);
        
        return NULL;
    }
    
    void *newPtr = (void *)calloc(n, size);
    if (!newPtr) {
        printf("NewMem calloc failed. Realloc cancelled (F: %s, L: %i).\n", file, line);

        pthread_mutex_unlock(&thread_lock);
        
        return NULL;
    }

    manager = temp;
    manager[count].busy = false;
    manager[count].ptr = newPtr;
    manager[count].size = size;
    manager[count].file = file;
    manager[count].line = line;
    count++;

    total_blocks_allocated++;
    total_memory_allocated+=size;

    pthread_mutex_unlock(&thread_lock);

    return newPtr;
}

/* Safely reallocs, without losing the original ptr in case of failure. */
static void* s_realloc(void *ptr, unsigned int newSize, const char *file, unsigned int line) {
    if (!ptr || !manager) return NULL;

    if (!atexit_active) {
        atexit(clear);
        atexit_active = true;
    }

    pthread_mutex_lock(&thread_lock);

    if (auto_benchmark_flag && !auto_benchmarking) {
        benchmark_create("AUTO_BENCHMARK_FLAG");
    }

    bool manager_index = 0;
    bool found = false;
    for (unsigned int i=0; i<count; i++) {
        if (manager[i].ptr == ptr) {
            found = true;
            manager_index = i;
            break;
        }
    }
    if (!found) {
        printf("Couldn't find the provided pointer inside of the manager (F: %s, L: %i).\n", file, line);

        pthread_mutex_unlock(&thread_lock);

        return ptr;
    }
    void *temp = (void *)realloc(ptr, newSize);
    if (!temp) {
        printf("Realloc failed (F: %s, L: %i).\n", file, line);

        pthread_mutex_unlock(&thread_lock);

        return ptr;
    }
    manager[manager_index].ptr = temp;
    manager[manager_index].file = file;
    manager[manager_index].line = line;

    total_blocks_allocated++;
    total_memory_allocated+=newSize-manager[manager_index].size;

    manager[manager_index].size = newSize;

    pthread_mutex_unlock(&thread_lock);

    return temp;
}

/* Safely frees, NULL-ifying the reference in the manager array */
static void s_free(void *p, const char *file, unsigned int line) {
    if (thread_safe) pthread_mutex_lock(&thread_lock);

    if (auto_benchmark_flag && !auto_benchmarking) {
        benchmark_create("AUTO_BENCHMARK_FLAG");
    }

    for (unsigned int i = 0; i < count; i++) {
        if (manager[i].ptr == p) {
            free(p);
            manager[i].ptr = NULL;
            freed_count++;
            
            pthread_mutex_unlock(&thread_lock);

            return;
        }
    }

    pthread_mutex_unlock(&thread_lock);
}

/* Tag uniqueness is NOT ensured. Might implement in the future. */
static void s_benchmark_create(const char *tag, const char *file, unsigned int line) {
    time_bench **temp = (time_bench **)realloc(benchmarks, (timers_count+1)*(sizeof(time_bench *)));
    if (!temp) {
        printf("Benchmarks realloc failed. Continuing execution normally.\n");
        return;
    }

    time_bench *newBench = (time_bench *)malloc(sizeof(time_bench));
    if (!newBench) printf("Benchmark malloc failed.");
    newBench->start = clock();
    newBench->running = true;
    newBench->tag = tag;

    benchmarks = temp;
    benchmarks[timers_count] = newBench;
    timers_count++;
}

/* Stops the specified benchmark. Use stop_benchmark_all, instead, to stop every benchmark. */
static void s_benchmark_stop(const char *tag, const char *file, unsigned int line) {
    if (thread_safe) pthread_mutex_lock(&thread_lock);

    for (unsigned int i=0; i<timers_count; i++) {
        if (strcmp(benchmarks[i]->tag, tag) == 0) {
            if (!benchmarks[i]->running) break;
            benchmarks[i]->running = false;
            benchmarks[i]->finish = clock();
        }
    }

    pthread_mutex_unlock(&thread_lock);
}

/* Stops every benchmark. */
static void s_benchmark_stop_all(const char *file, unsigned int line) {
    if (thread_safe) pthread_mutex_lock(&thread_lock);

    for (unsigned int i=0; i<timers_count; i++) {
        if (benchmarks[i]->running) {
            benchmarks[i]->running = false;
            benchmarks[i]->finish = clock();
        }
    }

    pthread_mutex_unlock(&thread_lock);
}