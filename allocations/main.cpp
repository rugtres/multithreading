#include <iostream>
#include <random>
#include <vector>
#include <chrono>
#include <tbb/tbb.h>


//#ifdef _WIN32
//  #include <tbb/scalable_allocator.h>
//  #include <tbb/tbbmalloc_proxy.h>
//#endif


/*
g++ main.cpp -O2 -std=c++17 -ltbb -o allocations
*/


class stopwatch
{
  using clock_t = std::chrono::high_resolution_clock;

public:
  stopwatch() { start(); }

  void start() { start_ = clock_t::now(); }
  void stop() { stop_ = clock_t::now(); }

  template <typename DURATION>
  auto elapsed() { return std::chrono::duration_cast<DURATION>(stop_ - start_); }

private:
  clock_t::time_point start_;
  clock_t::time_point stop_;
};


std::default_random_engine thread_local tlreng{};


struct foo
{
  foo() {}
  explicit foo(size_t k)
  {
    auto& reng = tlreng;    // pull thread local 
    auto dist = std::uniform_real_distribution<>(0, 100);
    for (size_t i = 0; i < k; ++i) {
      v.push_back(dist(reng));
    }
  }

  std::vector<double> v;
};


using population_t = std::vector<foo>;


population_t create_population(size_t N, size_t k)
{
  population_t pop{N};
  for (size_t n = 0; n < N; ++n) {
    pop[n] = foo(k);
  }
  return pop;
}


population_t create_population_par(size_t N, size_t k)
{
  population_t pop{N};
  tbb::parallel_for(
    tbb::blocked_range<size_t>(0, N),
    [&](const auto& r) {
      for (auto n = r.begin(); n < r.end(); ++n) {
        pop[n] = foo(k);
      }
    }
  );
  return pop;
}


int main()
{
  for (int i = 0; i < 1000; ++i) {
    {
      stopwatch watch{};
      auto pop = create_population(10000, 10000);
      watch.stop();
      std::cout << "  " << watch.elapsed<std::chrono::milliseconds>().count() << " ms  ";
    }
    {
      stopwatch watch{};
      auto pop = create_population_par(10000, 10000);
      watch.stop();
      std::cout << watch.elapsed<std::chrono::milliseconds>().count() << " ms\n";
    }
  }
  return 0;
}
