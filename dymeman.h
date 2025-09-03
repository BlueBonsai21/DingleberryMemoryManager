#pragma once

/* There is NO need to check for fails manually. Everything is done internally.
Realloc returns the realloc-ed pointer, if successful, else the provided one.
All pointers are freed autmatically upon program termination. */
#define dmalloc(size) s_malloc(size, __FILE__, __LINE__)
#define dcalloc(n, size) s_calloc(n, size, __FILE__, __LINE__)
#define drealloc(ptr, size) s_realloc(ptr, size, __FILE__, __LINE__)
#define dfree(ptr) s_free(ptr, __FILE__, __LINE__)

/* benchmark_create() creates a benchmark, with a provided unique tag.
Benchmarking functions DO NOT return anything. They are tracked internally.
Results of the benchmarks are tracked and displayed only when final_report_flag = true.
NOTE: tag uniqueness is NOT enforced. By using the same tags you may lose control over a benchmark. */
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