#include "ROOT/TSeq.hxx"
#include "TNUMA_Alpha.hxx"
#include "TStopwatch.h"
#include <iostream>

void numa() {

  auto func = [&](long int) -> long int {
    long int x{};
    ROOT::TSeq<long int> s(100 * 1000 * 1000);
    for (auto y : s)
      x = (x * 1009) % 71 + y;
    // non-trivial action to avoid cache optimizations
    return x;
  };

  auto redfunc = [](const std::vector<long int> &vec) {
    long int res = 0;
    for (size_t i = 0; i < vec.size(); ++i) {
      res += 1009 * vec[i * 1009 % vec.size()] % (vec.size() + 1) + vec[i];
      // non-trivial action to avoid cache optimizations
    }
    return res;
  };

  std::vector<long int> v(120);

  ROOT::Experimental::TNUMAExecutor n;
  ROOT::TThreadExecutor n;

  TStopwatch StopWatch;
  StopWatch.Reset();
  StopWatch.Start();

  std::cout << "numa -> " << n.MapReduce(func, v, redfunc) << std::endl;
  //std::cout << "thrd -> " << t.MapReduce(func, v, redfunc) << std::endl;
  StopWatch.Stop();
  std::cout << StopWatch.RealTime() << std::endl;
}

int main() {
  numa();
  return 0;
}
