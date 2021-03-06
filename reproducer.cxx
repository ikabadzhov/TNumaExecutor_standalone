// source:
// http://stackoverflow.com/questions/7259363/measuring-numa-non-uniform-memory-access-no-observable-asymmetry-why
#include "TStopwatch.h"
#include <assert.h>
#include <numa.h>
#include <omp.h>
#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include "TNUMA_Alpha.hxx"

void pin_to_core(size_t core) {
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  CPU_SET(core, &cpuset);
  pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
}

void print_bitmask(const struct bitmask *bm) {
  for (size_t i = 0; i < bm->size; ++i)
    printf("%d", numa_bitmask_isbitset(bm, i));
}

double measure_access(void *x, size_t array_size, size_t ntrips) {

  TStopwatch StopWatch;
  StopWatch.Reset();
  StopWatch.Start();
  for (size_t i = 0; i < ntrips; ++i)
    for (size_t j = 0; j < array_size; ++j)
      *(((char *)x) + ((j * 1009) % array_size)) += 1;
  StopWatch.Stop();

  return StopWatch.RealTime();
}

int main() {
  int num_cpus = numa_num_task_cpus();
  printf("num cpus: %d\n", num_cpus);

  printf("numa available: %d\n", numa_available());
  numa_set_localalloc();

  struct bitmask *bm = numa_bitmask_alloc(num_cpus);
  for (int i = 0; i <= numa_max_node(); ++i) {
    numa_node_to_cpus(i, bm);
    printf("numa node %d ", i);
    print_bitmask(bm);
    printf(" - %g GiB\n", numa_node_size(i, 0) / (1024. * 1024 * 1024.));
  }
  numa_bitmask_free(bm);

  puts("");

  char *x;
  const size_t cache_line_size = 64;
  const size_t array_size = 100 * 1000 * 1000;
  size_t ntrips = 2;

  // Option 1:
  ROOT::Experimental::TNUMAExecutor t(num_cpus);

  // Option 2:
  //ROOT::TThreadExecutor t(num_cpus);


#pragma omp parallel
  {
    assert(omp_get_num_threads() == num_cpus);
    int tid = omp_get_thread_num();

    // Option 3:
    //pin_to_core(tid);

    if (tid == 0)
      x = (char *)numa_alloc_local(array_size);

      // {{{ single access
#pragma omp barrier
    for (int i = 0; i < num_cpus; ++i) {
      if (tid == i) {
        double t = measure_access(x, array_size, ntrips);
        printf("sequential core %d -> core 0 : BW %g MB/s\n", i,
               array_size * ntrips * cache_line_size / t / 1e6);
      }
#pragma omp barrier
    }
    // }}}

    // {{{ everybody contends for one

    {
      if (tid == 0)
        puts("");

#pragma omp barrier
      double t = measure_access(x, array_size, ntrips);
#pragma omp barrier
      for (int i = 0; i < num_cpus; ++i) {
        if (tid == i)
          printf("all-contention core %d -> core 0 : BW %g MB/s\n", tid,
                 array_size * ntrips * cache_line_size / t / 1e6);
#pragma omp barrier
      }
    }

    // }}}

    // {{{ zero and someone else contending

    if (tid == 0)
      puts("");

#pragma omp barrier
    for (int i = 1; i < num_cpus; ++i) {
      double t;
      if (tid == i || tid == 0)
        t = measure_access(x, array_size, ntrips);

#pragma omp barrier
      if (tid == 0) {
        printf("two-contention core %d -> core 0 : BW %g MB/s\n", tid,
               array_size * ntrips * cache_line_size / t / 1e6);
      }
#pragma omp barrier
      if (tid == i) {
        printf("two-contention core %d -> core 0 : BW %g MB/s\n\n", tid,
               array_size * ntrips * cache_line_size / t / 1e6);
      }
#pragma omp barrier
    }
  }
  numa_free(x, array_size);

  return 0;
}
