#include "ROOT/RTaskArena.hxx"
#include <ROOT/TProcessExecutor.hxx>
#include <ROOT/TThreadExecutor.hxx>
#include <ROOT/TNUMAExecutor.hxx>

#include "ROOT/TSeq.hxx"
#include <iostream>

#include "TStopwatch.h"

void numa() {
  std::vector<long int> aux(200);
  std::iota(aux.rbegin(), aux.rend(), aux.size());

  auto func = [&](long int x) -> long int {
    for (auto y : aux)
      x = (x * 1009) % 71 + y;
    return x;
  };
  auto redfunc = [](const std::vector<long int> &vec) {
    long int res = 0;
    for (size_t i = 0; i < vec.size(); ++i) {
      res += vec[i];
    }
    return res;
  };

  std::vector<long int> v(100 * 1000 * 1000);
  std::iota(v.rbegin(), v.rend(), v.size());

  //ROOT::Experimental::TNUMAExecutor n;
  ROOT::TThreadExecutor t;
  TStopwatch StopWatch;
  StopWatch.Reset();
  StopWatch.Start();
  //std::cout << "numa -> " << n.MapReduce(func, v, redfunc) << std::endl;
  std::cout << "thrd -> " << t.MapReduce(func, v, redfunc, 10000) << std::endl;
  StopWatch.Stop();
  std::cout << StopWatch.RealTime() << std::endl;
}

int main() {
  numa();
  return 0;
}
