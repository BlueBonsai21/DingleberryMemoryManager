/*
TODO: 
Create unique tag strings and return them from benchmark_create(), so users don't have to make them up and have no
possibility of overriding previous tags.
Gotta add a detailed file+line report for auto_benchmark and thread_safety flags switches.
Make the AUTO_BENCHMARK be either global or local to the first memory management operation called in the program.
Add better documentation
Add a free_all() function, to free every pointer in manager pointer
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>
#include <time.h>
#include <string.h>

#include "dymeman.h"

#define FLAG_ACTIVE  "TRUE"
#define FLAG_INACTIVE "FALSE"

static pthread_mutex_t thread_lock = PTHREAD_MUTEX_INITIALIZER;

bool final_report_flag = true; // flag
void final_report() {
    final_report_flag = false;
}

typedef struct {
    bool running;
    const char *tag;
    clock_t start;
    clock_t finish;
} time_bench;
bool auto_benchmarking = false;
unsigned int timers_count = 0;
time_bench **benchmarks = NULL;

static unsigned int safety_switch_count = 0;
static bool thread_safe = true; // flag
void thread_safety(bool state) {
    if (state != thread_safe) safety_switch_count++;
    thread_safe = !thread_safe;
}

/* Memory tracking variables */
typedef struct {
    bool busy;
    void *ptr;
    unsigned int size;
    const char *file;
    unsigned int line;
} mem_t;
static unsigned int internal_use_allocated = 0;
static unsigned int total_blocks_allocated = 0;
static unsigned int total_memory_allocated = 0;

static unsigned int internal_use_freed = 0;
unsigned int freed_count = 0;
unsigned int freed_size = 0;

/* For tracking custom allocators/deallocators */
static unsigned int count = 0;
static mem_t *manager = NULL;

/* Debug functions substituting stdlib.h functions */
static unsigned int debug_track_fail = 0;
static unsigned int debug_count = 0;
static unsigned int debug_tracker_size = 0;
static mem_t *debug_tracker = NULL;


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
                freed_size += manager[i].size;
                free(manager[i].ptr);
                manager[i].ptr = NULL;
                freed_count++;
            }
        }
    
        free(manager);
        manager = NULL;
    }

    static char *threadSafetyFlag = FLAG_ACTIVE;
    if (!thread_safe) threadSafetyFlag = FLAG_INACTIVE;

    char report[0xfff]; // 4095 chars
    report[0] = '\0';
    strcat(report, "\
Dymeman - PROGRAM EXECUTION TERMINATED\n\
-------------------------------------------\n");

    char flags[1000];
    flags[0] = '\0';
    sprintf(flags, "\
1. FLAGS\n\
-> thread_safety: %s (switched %i times during execution)\n\
-------------------------------------------\n",
threadSafetyFlag, safety_switch_count);

    char heap[1000];
    heap[0] = '0';
    sprintf(heap, "\
2. HEAP\n\
-> Total allocations: %i\n\
-> Total frees: %i\n\
-> Total memory allocated (bytes): %i\n\
-> Total memory freed (bytes): %i\n\
-------------------------------------------\n",
total_blocks_allocated, freed_count, total_memory_allocated, freed_size);

    char benchmark_report[1000];
    benchmark_report[0] = '\0';
    strcat(benchmark_report, "3. BENCHMARKS\n");
    if (benchmarks) {
        for (unsigned int i=0; i<timers_count; i++) {
            sprintf(benchmark_report, "\
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
        strcat(benchmark_report, "-> NO BENCHMARKS STARTED.\n");
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
    // if (thread_safe) pthread_mutex_lock(&thread_lock);

    for (unsigned int i=0; i<count; i++) {
        if (manager[i].ptr == NULL && !manager[i].busy && manager[i].size >= minSize) {
            manager[i].busy = true;

            // pthread_mutex_unlock(&thread_lock);

            return i;
        }
    }
    // if (thread_safe) pthread_mutex_unlock(&thread_lock);

    return (unsigned int)-1;
}

bool atexit_active = false;
void* s_malloc(unsigned int size, const char *file, unsigned int line) {
    if(!atexit_active) {
        atexit(clear);
        atexit_active = true;
    }

    if (thread_safe) pthread_mutex_lock(&thread_lock);

    void *newPtr = NULL;
    unsigned int free_slot = check_free_slots(size);
    if (free_slot == (unsigned int)-1) {
        mem_t *temp = (mem_t *)realloc(manager, (count+1)*sizeof(mem_t));
        if (!temp) {
            printf("Manager realloc failed. Remained the same size (F: %s, L: %i).\n", file, line);

            if (thread_safe) pthread_mutex_unlock(&thread_lock);
            
            return NULL;
        }
    
        void *newPtr = (void *)malloc(size);
        if (!newPtr) {
            printf("NewMem alloc failed. Realloc cancelled (F: %s, L: %i).\n", file, line);

            if (thread_safe) pthread_mutex_unlock(&thread_lock);
            
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
    } else {
        manager[free_slot].busy = false;    
        manager[free_slot].file = file;
        manager[free_slot].line = line;

        newPtr = manager[free_slot].ptr;
    }
    
    if (thread_safe) pthread_mutex_unlock(&thread_lock);

    return newPtr; // that's the newly allocated memory if no free slot is found, else it's precisely that slot.
}

void* s_calloc(unsigned int n, unsigned int size, const char *file, unsigned int line) {
    if (!atexit_active) {
        atexit(clear);
        atexit_active = true;
    }

    if (size <= 0) return NULL;

    if (thread_safe) pthread_mutex_lock(&thread_lock);

    mem_t *temp = (mem_t *)realloc(manager, (count+1)*sizeof(mem_t));
    if (!temp) {
        printf("Manager realloc failed (F: %s, L: %i).\n", file, line);

        if (thread_safe) pthread_mutex_unlock(&thread_lock);
        
        return NULL;
    }
    
    void *newPtr = (void *)calloc(n, size);
    if (!newPtr) {
        printf("NewMem calloc failed. Realloc cancelled (F: %s, L: %i).\n", file, line);

        if (thread_safe) pthread_mutex_unlock(&thread_lock);
        
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

    if (thread_safe) pthread_mutex_unlock(&thread_lock);

    return newPtr;
}

void* s_realloc(void *ptr, unsigned int newSize, const char *file, unsigned int line) {
    if (!ptr || !manager) return NULL;

    if (!atexit_active) {
        atexit(clear);
        atexit_active = true;
    }

    if (thread_safe) pthread_mutex_lock(&thread_lock);

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

        if (thread_safe) pthread_mutex_unlock(&thread_lock);

        return ptr;
    }
    void *temp = (void *)realloc(ptr, newSize);
    if (!temp) {
        printf("Realloc failed (F: %s, L: %i).\n", file, line);

        if (thread_safe) pthread_mutex_unlock(&thread_lock);

        return ptr;
    }
    manager[manager_index].ptr = temp;
    manager[manager_index].file = file;
    manager[manager_index].line = line;

    total_blocks_allocated++;
    total_memory_allocated+=newSize-manager[manager_index].size;

    manager[manager_index].size = newSize;

    if (thread_safe) pthread_mutex_unlock(&thread_lock);

    return temp;
}

void s_free(void *p, const char *file, unsigned int line) {
    if (thread_safe) pthread_mutex_lock(&thread_lock);

    for (unsigned int i = 0; i < count; i++) {
        if (manager[i].ptr == p) {
            free(p);
            manager[i].ptr = NULL;
            freed_count++;
            
            if (thread_safe) pthread_mutex_unlock(&thread_lock);

            return;
        }
    }

    if (thread_safe) pthread_mutex_unlock(&thread_lock);
}

void s_free_all() {
    if (!manager) {
        printf("No manager active. Nothing to free.\n");
        return;
    }

    for (unsigned int i = 0; i < count; i++) {
        if (manager[i].ptr) {
            if (!manager[i].ptr) continue;

            if (manager[i].busy) {
                printf("Couldn't free a busy pointer: %p\n", manager[i].ptr);
                continue;
            }

            free(manager[i].ptr);
            manager[i].ptr = NULL;
            freed_size += manager[i].size;
            freed_count++;
        }
    }
    printf("Memory freed. Manager remained initialised.\n");
}

void s_benchmark_create(const char *tag, const char *file, unsigned int line) {
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

void s_benchmark_stop(const char *tag, const char *file, unsigned int line) {
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

void s_benchmark_stop_all(const char *file, unsigned int line) {
    if (thread_safe) pthread_mutex_lock(&thread_lock);

    for (unsigned int i=0; i<timers_count; i++) {
        if (benchmarks[i]->running) {
            benchmarks[i]->running = false;
            benchmarks[i]->finish = clock();
        }
    }

    pthread_mutex_unlock(&thread_lock);
}

void *debug_malloc(unsigned int size, const char *file, unsigned int line) {
    void *ptr = malloc(size);

    mem_t *temp = (mem_t *)realloc(debug_tracker, (debug_count+1)*sizeof(mem_t));
    if (!temp) debug_track_fail++;
    else {
        
        mem_t newMem;
        newMem.ptr = ptr;
        newMem.size = size;
        newMem.file = file;
        newMem.line = line;
        newMem.busy = false;
        
        debug_tracker = temp;
        debug_tracker[debug_count] = newMem;
        
        debug_count++;
    }
    
    return ptr;
}

void *debug_calloc(unsigned int n, unsigned int size, const char *file, unsigned int line) {
    void *ptr = calloc(n, size);

    mem_t *temp = (mem_t *)realloc(debug_tracker, (debug_count+1)*sizeof(mem_t));
    if (!temp) debug_track_fail++;
    else {
        
        mem_t newMem;
        newMem.ptr = ptr;
        newMem.size = size;
        newMem.file = file;
        newMem.line = line;
        newMem.busy = false;
        
        debug_tracker = temp;
        debug_tracker[debug_count] = newMem;
        
        debug_count++;
    }
    
    return ptr;
}

void *debug_realloc(void *ptr, unsigned int size, const char *file, unsigned int line) {
    void *ptr = realloc(ptr, size);

    mem_t *temp = (mem_t *)realloc(debug_tracker, (debug_count+1)*sizeof(mem_t));
    if (!temp) debug_track_fail++;
    else {
        if (ptr) {
             
        }
        
        mem_t newMem;
        newMem.ptr = ptr;
        newMem.size = size;
        newMem.file = file;
        newMem.line = line;
        newMem.busy = false;
        
        debug_tracker = temp;
        debug_tracker[debug_count] = newMem;
        
        debug_count++;
    }
    
    return ptr;
}

void *debug_free(unsigned int size, const char *file, unsigned int line) {
    if (debug_tracker) free
    
    return ptr;
}

/* Defining here since if I do in the header this shit become cyclical and I can't use 
stdlib.h functions. Not a big deal. */

#define malloc(size) debug_malloc(size, __FILE__, __LINE__)
#define calloc(n, size) debug_calloc(n, size, __FILE__, __LINE__)
#define realloc(ptr, size) debug_realloc(ptr, size, __FILE__, __LINE__)
#define free(ptr) debug_free(ptr, __FILE__, __LINE__)