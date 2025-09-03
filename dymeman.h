#pragma once

#define malloc(size) s_malloc(size, __FILE__, __LINE__)
#define calloc(n, size) s_calloc(n, size, __FILE__, __LINE__)
#define realloc(ptr, size) s_realloc(ptr, size, __FILE__, __LINE__)
#define free(ptr) s_free(ptr, __FILE__, __LINE__)

#define benchmark_create(tag) s_benchmark_create(tag, __FILE__, __LINE__)
#define benchmark_stop(tag) s_benchmark_stop(tag, __FILE__, __LINE__)
#define benchmark_stop_all() s_benchmark_stop_all(__FILE__, __LINE__)

/* Switches the flag to the provided state.
Default: thread_safe = true. */
void thread_safety(bool state);

/* Switches the flag to true.
If called, the moment the first memory management operation is called, a new benchmark will start, lasting up to the end of the program.
Default: auto_benchmark = false. */
void auto_benchmark();

/* Switches the flag to false.
If called, no statistics will be saved nor anything will be printed to console.
Default: final_report_flag = true */
void final_report();