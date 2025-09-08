#pragma once

/*
! IMPORTANT: when using stdlib.h built-in memory management functions and applying this library to debug or 
! see if any memory leak is happening, whenever the pointer you're allocating is NOT correctly initialised
! this library will NOT track it and will NOT consider it in the final report.
! Planning onto changing this in the future.
*/



//! FLAGS

enum flags {THREAD_SAFE = 0};

/* Sets the given flag to the provided state. Default:
THREAD_SAFE = true */
void set_flag(enum flags flag, bool state);



//! CUSTOM ALLOCATORS/DEALLOCATORS - SAFE MEMORY MANAGEMENT FUNCTIONS

/* There is NO need to check for fails manually. Everything is done internally.
Realloc returns the realloc-ed pointer, if successful, else the provided one.
All pointers are freed autmatically upon program termination. */

/* Meant for internal use. Refer to dmalloc(). */
void* s_malloc(unsigned int size, const char *file, unsigned int line);

/* Meant for internal use. Refer to dcalloc(). */
void* s_calloc(unsigned int n, unsigned int size, const char *file, unsigned int line);

/* Meant for internal use. Refer to drealloc(). */
void* s_realloc(void *ptr, unsigned int newSize, const char *file, unsigned int line);

/* Meant for internal use. Refer to dfree(). */
void s_free(void *p, const char *file, unsigned int line);

/* Safely mallocs, adding the ptr to the manager array */
#define dmalloc(size) s_malloc(size, __FILE__, __LINE__)

/* Safely callocs, adding the ptr to the manager array */
#define dcalloc(n, size) s_calloc(n, size, __FILE__, __LINE__)

/* Safely reallocs, without losing the original ptr in case of failure. */
#define drealloc(ptr, size) s_realloc(ptr, size, __FILE__, __LINE__)

/* Safely frees, NULL-ifying the reference in the manager array */
#define dfree(ptr) s_free(ptr, __FILE__, __LINE__)



//! BENCHMARKING FUNCTIONS

/* benchmark_create() creates a benchmark, with a provided unique tag.
Results of the benchmarks are tracked and displayed only when final_report_flag = true [for this, check final_report()].
NOTE: tag uniqueness is NOT enforced. By using the same tags you may lose control over a benchmark. */
void s_benchmark_create(const char *tag, const char *file, unsigned int line);
void s_benchmark_stop(const char *tag, const char *file, unsigned int line);
void s_benchmark_stop_all(const char *file, unsigned int line);

/* Tag uniqueness is NOT ensured. Might implement in the future. */
#define benchmark_create(tag) s_benchmark_create(tag, __FILE__, __LINE__)

/* Stops the specified benchmark. Use stop_benchmark_all, instead, to stop every benchmark. */
#define benchmark_stop(tag) s_benchmark_stop(tag, __FILE__, __LINE__)

/* Stops every benchmark. */
#define benchmark_stop_all() s_benchmark_stop_all(__FILE__, __LINE__)



//! MEMORY DIAGNOSIS - DEFAULT ALLOCATORS/DEALLOCATORS USED; NO MEMORY SAFETY GRANTED.

/* Ment for internal use. Refer to malloc(). */
void* debug_malloc(unsigned int size, const char *file, unsigned int line);

/* Meant for internal use. Refer to calloc(). */
void* debug_calloc(unsigned int n, unsigned int size, const char *file, unsigned int line);

/* Meant for internal use. Refer to realloc(). */
void* debug_realloc(void *ptr, unsigned int size, const char *file, unsigned int line);

/* Meant for internal use. Refer to free(). */
void debug_free(void *ptr, const char *file, unsigned int line);

#define malloc(size) debug_malloc(size, __FILE__, __LINE__)
#define calloc(n, size) debug_calloc(n, size, __FILE__, __LINE__)
#define realloc(ptr, size) debug_realloc(ptr, size, __FILE__, __LINE__)
#define free(ptr) debug_free(ptr, __FILE__, __LINE__)