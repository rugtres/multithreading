#include <iostream>
#include <fstream>
#include <chrono>
#include <thread>
#include <mutex>
#include <future>
#include <thread>
#include <algorithm>
#include <vector>
#include <execution>
#include <nlohmann/json.hpp>
#include <tbb/tbb.h>


/*
g++ main.cpp -O2 -fopenmp -std=c++17 -I../ -ltbb -pthread -o simulations
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



struct Parameter
{
  double a, b;
};


NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Parameter, a, b)


double run_single_simulation(const Parameter& param)
{
  std::this_thread::sleep_for(std::chrono::milliseconds(1000));
  return param.a + param.b;
}


// for_each
auto sequential(const nlohmann::json& jp)
{
  std::cout << "sequential" << std::endl;;

  nlohmann::json jres;
  std::for_each(
    jp.begin(), jp.end(), 
    [&](const auto& j) {
      double result = run_single_simulation(j);
      jres.push_back(j); jres.back()["result"] = result;
      std::cout << "sim " << jres.back() << " finished" << std::endl;
    }
  );
  return jres;
};


// C++11 way of multi-threading
auto std_thread(const nlohmann::json& jp)
{
  std::cout << "std_thread" << std::endl;;

  nlohmann::json jres;
  std::mutex mutex;
  std::vector<std::thread> threads;
  for (const auto j : jp) {
    threads.emplace_back(
      [&](const auto& ji) {
        auto result = run_single_simulation(ji);
        std::lock_guard<std::mutex> _(mutex);
        jres.push_back(ji); jres.back()["result"] = result;
        std::cout << "sim " << jres.back() << " finished" << std::endl;
      },
      j
    );
  }
  for (size_t i = 0; i < threads.size(); ++i) {
    threads[i].join();    // wait for completion
  }
  return jres;
};


// C++11 way of multi-threading
auto std_async(const nlohmann::json& jp)
{
  std::cout << "std_async" << std::endl;;

  nlohmann::json jres;
  std::vector<std::future<double>> futures;
  for (const auto& j : jp) {
    auto future = std::async(
      std::launch::async, 
      [](const auto& ji) {
        return run_single_simulation(ji); 
      },
      j
    );
    futures.emplace_back(std::move(future));
  }
  for (size_t i = 0; i < futures.size(); ++i) {
    double result = futures[i].get();   // wait for the feature to become available
    jres.push_back(jp.at(i)); jres.back()["result"] = result;
    std::cout << "sim " << jres.back() << " finished" << std::endl;
  }
  return jres;
};


// since C++17 we have std::execution_policy
auto std_par(const nlohmann::json& jp)
{
  std::cout << "std_par" << std::endl;;

  nlohmann::json jres;
  std::mutex mutex;
  std::for_each(
    std::execution::par, 
    jp.begin(), jp.end(), 
    [&](const auto& ji) {
      double result = run_single_simulation(ji);
      std::lock_guard<std::mutex> _(mutex);
      jres.push_back(ji); jres.back()["result"] = result;
      std::cout << "sim " << jres.back() << " finished" << std::endl;
    }
  );
  return jres;
};


// the tbb way of a parallel loop
auto tbb_parallel_for_each(const nlohmann::json& jp)
{
  std::cout << "tbb_parallel_for_each" << std::endl;;

  nlohmann::json jres;
  std::mutex mutex;
  tbb::parallel_for_each(
    jp.begin(), jp.end(), 
    [&](const auto& ji) {
      double result = run_single_simulation(ji);
      std::lock_guard<std::mutex> _(mutex);
      jres.push_back(ji); jres.back()["result"] = result;
      std::cout << "sim " << jres.back() << " finished" << std::endl;
    }
  );
  return jres;
};


// for C we have OpenMP
auto openmp(const nlohmann::json& jp)
{
  std::cout << "openmp" << std::endl;;

  nlohmann::json jres;
  const int N = static_cast<int>(jp.size());
#pragma omp parallel for 
  for (int i = 0; i < N; ++i) {
    double result = run_single_simulation(jp.at(i));
#pragma omp critical
    {
      jres.push_back(jp.at(i)); jres.back()["result"] = result;
      std::cout << "sim " << jres.back() << " finished" << std::endl;
    }
  }
  return jres;
};


template <typename FUN>
void run_all(const nlohmann::json& jp, FUN&& fun)
{
  stopwatch watch{};
  auto jres = fun(jp);
  watch.stop();
  std::cout << watch.elapsed<std::chrono::milliseconds>().count() << " ms\n" << std::endl;
}


int main()
{
  try {
    std::ifstream is("params.json");
    nlohmann::json jp{};
    is >> jp;
    run_all(jp, sequential);
    run_all(jp, std_par);
    run_all(jp, std_thread);
    run_all(jp, std_async);
    run_all(jp, openmp);
    run_all(jp, tbb_parallel_for_each);
    return 0;
  }
  catch (const std::exception& err) {
    std::cout << err.what() << std::endl;
  }
  return -1;
}
