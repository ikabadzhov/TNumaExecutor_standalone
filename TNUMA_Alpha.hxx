#ifndef ROOT_TNUMAExecutor
#define ROOT_TNUMAExecutor

//#include "ROOT/RSpan.hxx"
#include "ROOT/RTaskArena.hxx"
#include "ROOT/TProcessExecutor.hxx"
#include "ROOT/TThreadExecutor.hxx"
#include "tbb/task_arena.h"
#define TBB_PREVIEW_GLOBAL_CONTROL 1 // required for TBB versions preceding 2019_U4
#include "tbb/global_control.h"

#include <algorithm> // std::min, std::max
#include <numa.h>
#include <thread>

namespace ROOT {
namespace Experimental {

class TNUMAExecutor {
public:
   template <class F, class... T>
   using noReferenceCond =
      typename std::enable_if<"Function can't return a reference" &&
                              !(std::is_reference<typename std::result_of<F(T...)>::type>::value)>::type;

   TNUMAExecutor(unsigned nThreads = 0u)
   {
      // TODO: use fTBBArena->max_concurrency();
      const unsigned tbbDefaultNumberThreads = std::thread::hardware_concurrency();
      nThreads = nThreads > 0 ? std::min(nThreads, tbbDefaultNumberThreads) : tbbDefaultNumberThreads;
      /*
      const unsigned bcCpus = ROOT::Internal::LogicalCPUBandwithControl();
      if (nThreads > bcCpus) {
         Warning("TNUMAExecutor", "CPU Bandwith Control Active. Proceeding with %d threads accordingly", bcCpus);
         nThreads = bcCpus;
      }
      if (nThreads > tbb::global_control::active_value(tbb::global_control::max_allowed_parallelism)) {
         Warning("TNUMAExecutor", "tbb::global_control is active, limiting the number of parallel workers.");
      }
      */
      fNDomains = numa_max_node() + 1;
      fDomainNThreads = tbbDefaultNumberThreads / fNDomains;
   }

   unsigned GetNUMADomains() const { return fNDomains; }

   template <class F, class R, class Cond = noReferenceCond<F>>
   auto MapReduce(F func, unsigned nTimes, R redfunc, unsigned nChunks = 0) -> typename std::result_of<F()>::type;
   template <class F, class INTEGER, class R, class Cond = noReferenceCond<F, INTEGER>>
   auto MapReduce(F func, ROOT::TSeq<INTEGER> args, R redfunc, unsigned nChunks = 0) ->
      typename std::result_of<F(INTEGER)>::type;
   template <class F, class T, class R, class Cond = noReferenceCond<F, T>>
   auto MapReduce(F func, std::initializer_list<T> args, R redfunc, unsigned nChunks = 0) ->
      typename std::result_of<F(T)>::type
   {
      std::vector<T> vargs(std::move(args));
      return MapReduce(func, vargs, redfunc, nChunks);
   }
   template <class F, class T, class R, class Cond = noReferenceCond<F, T>>
   auto MapReduce(F func, std::vector<T> &args, R redfunc, unsigned nChunks = 0) -> typename std::result_of<F(T)>::type;

private:
   template <class T>
   std::vector<std::vector<T>> splitData(std::vector<T> &av);

   unsigned fNDomains{};
   unsigned fDomainNThreads{};
};

template <class T>
std::vector<std::vector<T>> TNUMAExecutor::splitData(std::vector<T> &av)
{
   unsigned stride = (av.size() + fNDomains - 1) / fNDomains; // ceiling the division
   std::vector<std::vector<T>> v;
   unsigned i;
   for (i = 0; i * stride < av.size() - stride; i++) {
      v.emplace_back(std::vector<T>(av.begin() + i * stride, av.begin() + (i + 1) * stride));
   }
   v.emplace_back(std::vector<T>(av.begin() + i * stride, av.end()));

   return v;
}

template <class F, class R, class Cond>
auto TNUMAExecutor::MapReduce(F func, unsigned nTimes, R redfunc, unsigned nChunks) ->
   typename std::result_of<F()>::type
{
   auto runOnNode = [&](unsigned int i) {
      numa_run_on_node(i);
      ROOT::TThreadExecutor threadExecutor{fDomainNThreads};
      if (fNDomains != 1) {
         auto res =  i ? (nChunks ? threadExecutor.MapReduce(func, int(nTimes / fNDomains), redfunc, nChunks / fNDomains)
                                  : threadExecutor.MapReduce(func, int(nTimes / fNDomains), redfunc))
                       : (nChunks ? threadExecutor.MapReduce(func, nTimes % fNDomains, redfunc, nChunks / fNDomains)
                                  : threadExecutor.MapReduce(func, nTimes % fNDomains, redfunc));
         numa_run_on_node_mask(numa_all_nodes_ptr);
         return res;
      }
      else {
         auto res = threadExecutor.MapReduce(func, nTimes, redfunc);
         numa_run_on_node_mask(numa_all_nodes_ptr);
         return res;
      }
   };

   ROOT::TProcessExecutor processExecutor(fNDomains);
   return processExecutor.MapReduce(runOnNode, ROOT::TSeq<unsigned>(fNDomains), redfunc);
}

template <class F, class T, class R, class Cond>
auto TNUMAExecutor::MapReduce(F func, std::vector<T> &args, R redfunc, unsigned nChunks) ->
   typename std::result_of<F(T)>::type
{
   auto dataRanges = splitData(args);
   auto runOnNode = [&](unsigned int i) {
      numa_run_on_node(i);
      ROOT::TThreadExecutor threadExecutor{fDomainNThreads};
      auto res = nChunks ? threadExecutor.MapReduce(func, dataRanges[i], redfunc, nChunks / fNDomains)
                         : threadExecutor.MapReduce(func, dataRanges[i], redfunc);
      numa_run_on_node_mask(numa_all_nodes_ptr);
      return res;
   };

   ROOT::TProcessExecutor processExecutor(fNDomains);
   return processExecutor.MapReduce(runOnNode, ROOT::TSeq<unsigned>(fNDomains), redfunc);
}

template <class F, class INTEGER, class R, class Cond>
auto TNUMAExecutor::MapReduce(F func, ROOT::TSeq<INTEGER> args, R redfunc, unsigned nChunks) ->
   typename std::result_of<F(INTEGER)>::type
{
   unsigned stride = (*args.end() - *args.begin() + fNDomains - 1) / fNDomains; // ceiling the division
   auto runOnNode = [&](unsigned int i) {
      numa_run_on_node(i);
      ROOT::TThreadExecutor threadExecutor{fDomainNThreads};
      ROOT::TSeq<unsigned> sequence(std::max(unsigned(*args.begin()), i *stride),
                                    std::min((i + 1) * stride, unsigned(*args.end())));
      auto res = threadExecutor.MapReduce(func, sequence, redfunc, nChunks / fNDomains);
      numa_run_on_node_mask(numa_all_nodes_ptr);
      return res;
   };

   ROOT::TProcessExecutor processExecutor(fNDomains);
   return processExecutor.MapReduce(runOnNode, ROOT::TSeq<unsigned>(fNDomains), redfunc);
}

} // namespace Experimental
} // namespace ROOT

#endif