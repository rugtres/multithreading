#include <cassert>
#include <iostream>
#include <chrono>
#include <vector>
#include <array>
#include <iterator>
#include <numeric>
#include <random>
#include <execution>
#include <mutex>
#include <atomic>
#include <thread>
#include <tbb/tbb.h>


/*
sudu apt install g++-10
g++-10 main.cpp -O2 -fopenmp -std=c++2a -I../ -ltbb -pthread -o reduction
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


template <typename IT>
auto sequential(IT first, IT last)
{
  std::cout << "sequential: ";
  using T = typename std::iterator_traits<IT>::value_type;
  return std::accumulate(first, last, T(0));
}


template <typename IT>
auto std_par_mutex(IT first, IT last)
{
  std::cout << "std_par_mutex: ";
  using T = typename std::iterator_traits<IT>::value_type;
  auto sum = T(0);
  std::mutex mutex;
  std::for_each(
    std::execution::par, 
    first, last, 
    [&](auto x) {
      std::lock_guard<std::mutex> _(mutex);
      sum += x;
    }
  );
  return sum;
}


template <typename IT>
auto std_par_atomic(IT first, IT last)
{
  std::cout << "std_par_atomic: ";
  using T = typename std::iterator_traits<IT>::value_type;
  std::atomic<T> sum = T(0);
  std::for_each(
    std::execution::par, 
    first, last, 
    [&](auto x) {
      sum += x;
    }
  );
  return sum.load();
}


template <typename IT>
auto std_par_chunked(IT first, IT last)
{
  constexpr static std::array<ptrdiff_t, 64> chunk{ 
    0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,
    32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,52,53,54,55,56,57,58,59,60,61,62,63
  };

  std::cout << "std_par_chunked: ";
  using T = typename std::iterator_traits<IT>::value_type;
  std::atomic<T> sum = T(0);
  const auto N = std::distance(first, last);
  const auto chunk_size = N / std::min<ptrdiff_t>(chunk.size(), std::thread::hardware_concurrency() >> 1);
  const auto chunks = chunk_size ? N / chunk_size : 0;
  std::for_each_n(
    std::execution::par, 
    chunk.cbegin(), chunks, 
    [&](ptrdiff_t c) {
      auto it = first + c * chunk_size;
      T local_sum = std::reduce(it, it + chunk_size, T(0));
      sum += local_sum;
    }
  );
  // don't forget the tail:
  return std::accumulate(first + chunks * chunk_size, last, sum.load());
}


template <typename IT>
auto std_par_reduce(IT first, IT last)
{
  std::cout << "std_reduce: ";
  using T = typename std::iterator_traits<IT>::value_type;
  return std::reduce(std::execution::par, first, last, T(0));
}


template <typename IT>
auto openmp(IT first, IT last)
{
  std::cout << "openmp: ";
  using T = typename std::iterator_traits<IT>::value_type;
  const int N = static_cast<int>(std::distance(first, last));
  T sum = 0;
#pragma omp parallel for shared(first) reduction(+: sum)
  for (int i = 0; i < N; ++i) {
    sum += first[i];
  }
  return sum;
}


template <typename IT>
auto tbb_parallel_for(IT first, IT last)
{
  std::cout << "tbb_parallel_for: ";
  using T = typename std::iterator_traits<IT>::value_type;

  tbb::combinable<T> comb{};      // thread local
  tbb::parallel_for(
    tbb::blocked_range<IT>{first, last}, 
    [&](const auto& r) {
      auto local_sum = std::reduce(r.begin(), r.end(), T(0));
      comb.local() += local_sum;
    }
  );
  return comb.combine(std::plus<T>{});
}


template <typename IT>
auto tbb_parallel_reduce(IT first, IT last)
{
  std::cout << "tbb_parallel_reduce: ";
  using T = typename std::iterator_traits<IT>::value_type;

  return tbb::parallel_reduce(
    tbb::blocked_range<IT>{first, last}, 
    T(0),
    [&](const auto& r, T sum) { return std::reduce(r.begin(), r.end(), sum); },
    std::plus<T>{}
  );
}



template <typename FUN>
auto reduction(const std::vector<float>& v, FUN&& fun)
{
  stopwatch watch{};
  auto res = fun(v.cbegin(), v.end());
  watch.stop();
  std::cout << res << "  " << watch.elapsed<std::chrono::microseconds>().count() << " us\n";
  return res;
}


int main()
{
  std::vector<float> v(100000);
  std::default_random_engine reng{};
  auto dist = std::normal_distribution<float>(0, 1);
  for (auto& x : v) { x = dist(reng); }

  for (int i = 0; i < 3; ++i) {
    auto r1 = reduction(v, [](auto first, auto last) { return sequential(first, last); });
    auto r2 = reduction(v, [](auto first, auto last) { return std_par_mutex(first, last); });
    auto r3 = reduction(v, [](auto first, auto last) { return std_par_atomic(first, last); });
    auto r4 = reduction(v, [](auto first, auto last) { return std_par_chunked(first, last); });
    auto r5 = reduction(v, [](auto first, auto last) { return std_par_reduce(first, last); });
    auto r6 = reduction(v, [](auto first, auto last) { return openmp(first, last); });
    auto r7 = reduction(v, [](auto first, auto last) { return tbb_parallel_for(first, last); });
    auto r8 = reduction(v, [](auto first, auto last) { return tbb_parallel_reduce(first, last); });
    std::cout << '\n';
  }
  return 0;
}
