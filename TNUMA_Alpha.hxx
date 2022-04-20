#ifndef ROOT_TNUMAExecutor
#define ROOT_TNUMAExecutor

// Require TBB without captured exceptions
#define TBB_USE_CAPTURED_EXCEPTION 0

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
      
      const unsigned bcCpus = ROOT::Internal::LogicalCPUBandwithControl();
      
      if (nThreads > bcCpus) {
         Warning("TNUMAExecutor", "CPU Bandwith Control Active. Proceeding with %d threads accordingly.", bcCpus);
         nThreads = bcCpus;
      }
      // TODO: apply this condition
      //if (nThreads > tbb::global_control::active_value(tbb::global_control::max_allowed_parallelism)) {
      //   Warning("TNUMAExecutor", "tbb::global_control is active, limiting the number of parallel workers.");
      //}
      
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
   unsigned fNDomains{};
   unsigned fDomainNThreads{};
};

template <class F, class R, class Cond>
auto TNUMAExecutor::MapReduce(F func, unsigned nTimes, R redfunc, unsigned nChunks) ->
   typename std::result_of<F()>::type
{
   if (fNDomains == 1) { // there are no NUMA domains, no need to create processes
      ROOT::TThreadExecutor threadExecutor{fDomainNThreads};
      return nChunks ? threadExecutor.MapReduce(func, nTimes, redfunc, nChunks)
                     : threadExecutor.MapReduce(func, nTimes, redfunc);
   } else { // if there are NUMA domains, fork and create task arenas inside each domain
      auto runOnNode = [&](unsigned i) {
         numa_run_on_node(i); // run current process on all cores in specific NUMA domain i
         numa_run_on_node_mask(numa_all_nodes_ptr);
         ROOT::TThreadExecutor threadExecutor{fDomainNThreads}; // threads created in 1 specific domain

         if (i != fNDomains - 1) {
            const unsigned nTimesPerProc = (nTimes + fNDomains - 1) / fNDomains; // ceiling the division
            const unsigned nChunksPerProc = (nChunks + fNDomains - 1) / fNDomains; // ceiling the division
            return nChunks ? threadExecutor.MapReduce(func, nTimesPerProc, redfunc, nChunksPerProc)
                           : threadExecutor.MapReduce(func, nTimesPerProc, redfunc);
         } else {
            const unsigned nTimesPerProc = nTimes - ((nTimes + fNDomains - 1) / fNDomains) * (fNDomains - 1);
            const unsigned nChunksPerProc = nChunks - ((nChunks + fNDomains - 1) / fNDomains) * (fNDomains - 1);
            return nChunks ? threadExecutor.MapReduce(func, nTimesPerProc, redfunc, nChunksPerProc)
                           : threadExecutor.MapReduce(func, nTimesPerProc, redfunc);
         }
      };

      ROOT::TProcessExecutor processExecutor(fNDomains); // fork first (no RTaskArenas created so far)
      return processExecutor.MapReduce(runOnNode, ROOT::TSeq<unsigned>(fNDomains), redfunc);
   } 
}

template <class F, class T, class R, class Cond>
auto TNUMAExecutor::MapReduce(F func, std::vector<T> &args, R redfunc, unsigned nChunks) ->
   typename std::result_of<F(T)>::type
{
   if (fNDomains == 1) { // there are no NUMA domains, no need to create processes
      ROOT::TThreadExecutor threadExecutor{fDomainNThreads};
      return nChunks ? threadExecutor.MapReduce(func, args, redfunc, nChunks)
                     : threadExecutor.MapReduce(func, args, redfunc);
   } else {
      auto runOnNode = [&](unsigned int i) {
         numa_run_on_node(i);
         numa_run_on_node_mask(numa_all_nodes_ptr);
         ROOT::TThreadExecutor threadExecutor{fDomainNThreads};

         if (i != fNDomains - 1) {
            const auto lowerBoundRange = args.begin() + i * (args.size() + fNDomains - 1) / fNDomains; // beginning of vector split
            const auto upperBoundRange = args.begin() + (i + 1) * (args.size() + fNDomains - 1) / fNDomains; // end of vector split
            const unsigned nChunksPerProc = (nChunks + fNDomains - 1) / fNDomains; // ceiling the division
            return nChunks ? threadExecutor.MapReduce(func, std::vector<T>(lowerBoundRange, upperBoundRange), redfunc, nChunksPerProc)
                           : threadExecutor.MapReduce(func, std::vector<T>(lowerBoundRange, upperBoundRange), redfunc);
         } else {
            const auto lowerBoundRange = args.begin() + i * (args.size() + fNDomains - 1) / fNDomains; // beginning of vector split
            const auto upperBoundRange = args.end(); // end of vector split
            const unsigned nChunksPerProc = nChunks - ((nChunks + fNDomains - 1) / fNDomains) * (fNDomains - 1);
            return nChunks ? threadExecutor.MapReduce(func, std::vector<T>(lowerBoundRange, upperBoundRange), redfunc, nChunksPerProc)
                           : threadExecutor.MapReduce(func, std::vector<T>(lowerBoundRange, upperBoundRange), redfunc);
         }
      };
      ROOT::TProcessExecutor processExecutor(fNDomains);
      return processExecutor.MapReduce(runOnNode, ROOT::TSeq<unsigned>(fNDomains), redfunc);
   }
}

template <class F, class INTEGER, class R, class Cond>
auto TNUMAExecutor::MapReduce(F func, ROOT::TSeq<INTEGER> args, R redfunc, unsigned nChunks) ->
   typename std::result_of<F(INTEGER)>::type
{
   if (fNDomains == 1) { // there are no NUMA domains, no need to create processes
      ROOT::TThreadExecutor threadExecutor{fDomainNThreads};
      return nChunks ? threadExecutor.MapReduce(func, args, redfunc, nChunks)
                     : threadExecutor.MapReduce(func, args, redfunc);
   } else {
      auto runOnNode = [&](unsigned int i) {
         numa_run_on_node(i);
         numa_run_on_node_mask(numa_all_nodes_ptr);
         ROOT::TThreadExecutor threadExecutor{fDomainNThreads};

         if (i != fNDomains - 1) {
            const auto lowerBoundRange = args[0] + i * (args.size() + fNDomains - 1) / fNDomains; // beginning of vector split
            const auto upperBoundRange = args[0] + (i + 1) * (args.size() + fNDomains - 1) / fNDomains; // end of vector split
            const unsigned nChunksPerProc = (nChunks + fNDomains - 1) / fNDomains; // ceiling the division
            return nChunks ? threadExecutor.MapReduce(func, ROOT::TSeq<INTEGER>(lowerBoundRange, upperBoundRange, args.step()), redfunc, nChunksPerProc)
                           : threadExecutor.MapReduce(func, ROOT::TSeq<INTEGER>(lowerBoundRange, upperBoundRange, args.step()), redfunc);
         } else {
            const auto lowerBoundRange = args[0] + i * (args.size() + fNDomains - 1) / fNDomains; // beginning of vector split
            const auto upperBoundRange = args[args.size()-1] + 1; // end of vector split
            const unsigned nChunksPerProc = nChunks - ((nChunks + fNDomains - 1) / fNDomains) * (fNDomains - 1);
            return nChunks ? threadExecutor.MapReduce(func, ROOT::TSeq<INTEGER>(lowerBoundRange, upperBoundRange, args.step()), redfunc, nChunksPerProc)
                           : threadExecutor.MapReduce(func, ROOT::TSeq<INTEGER>(lowerBoundRange, upperBoundRange, args.step()), redfunc);
         }
      };
      ROOT::TProcessExecutor processExecutor(fNDomains);
      return processExecutor.MapReduce(runOnNode, ROOT::TSeq<INTEGER>(fNDomains), redfunc);
   }
}

} // namespace Experimental
} // namespace ROOT

#endif