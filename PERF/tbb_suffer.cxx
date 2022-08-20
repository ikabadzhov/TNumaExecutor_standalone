#include <oneapi/tbb/parallel_for_each.h>

#include <chrono>
#include <iostream>
#include <vector>

std::vector<int> make_data() { return std::vector<int>(2'000'000'000u, 2); }

int main() {
  auto data1 = make_data();
  auto data2 = make_data();
  auto data3 = make_data();

  auto do_work = [&](int s) {
    for (int i = s; i < s + 500000; ++i) {
      data2[i] = data3[i] * data3[i] * data3[i] + i;
      data1[i] = data2[i] * data3[i] * 10009 % (data2[i] + 1) + data2[i] % data3[i];
    }
  };

  const auto start = std::chrono::system_clock::now();
  for (int i = 0; i < 100;  ++i) {
    oneapi::tbb::parallel_for(0ul, data1.size(), 500000ul, do_work);
  }
  const auto end = std::chrono::system_clock::now();
  std::cout << std::chrono::duration_cast<std::chrono::milliseconds>(end -
                                                                     start)
                   .count()
            << '\n';
}

