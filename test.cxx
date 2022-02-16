#include "TNUMA_Alpha.hxx"
#include "ROOT/TSeq.hxx"
#include <iostream>

void numa(){

   auto func = [&](long int)->long int {
      long int x{}; 
      ROOT::TSeq<long int> s(17); 
      for(auto y: s) x+=y;
      return x;
   };
  
   auto func2 = []()->long int { 
      long int x{}; 
      ROOT::TSeq<long int> s(17); 
      for(auto y: s) x+=y;
      return x;    
   };

   auto redfunc = [](const std::vector<long int> &v){return std::accumulate(v.begin(), v.end(), 0l);};
  
   ROOT::Experimental::TNUMAExecutor n;
   ROOT::TThreadExecutor t;

   std::cout << "TEST 1:\n";
   std::cout << "numa with chunks -> " << n.MapReduce(func, ROOT::TSeq<long int>(11), redfunc, 5) << std::endl;
   std::cout << "thrd with chunks -> " << t.MapReduce(func, ROOT::TSeq<long int>(11), redfunc, 5) << std::endl;
   std::cout << "numa without chunks -> " << n.MapReduce(func, ROOT::TSeq<long int>(11), redfunc) << std::endl;
   std::cout << "thrd without chunks -> " << t.MapReduce(func, ROOT::TSeq<long int>(11), redfunc) << std::endl;

   std::cout << "TEST 2:\n";
   std::vector<long int> v(11);
   std::cout << "numa with chunks -> " << n.MapReduce(func, v, redfunc, 5) << std::endl;
   std::cout << "thrd with chunks -> " << t.MapReduce(func, v, redfunc, 5) << std::endl;
   std::cout << "numa without chunks -> " << n.MapReduce(func, v, redfunc) << std::endl;
   std::cout << "thrd without chunks -> " << t.MapReduce(func, v, redfunc) << std::endl;  

   std::cout << "TEST 3:\n";
   std::cout << "numa with chunks -> " << n.MapReduce(func2, 11, redfunc, 5) << std::endl;
   std::cout << "thrd with chunks -> " << t.MapReduce(func2, 11, redfunc, 5) << std::endl;
   std::cout << "numa without chunks -> " << n.MapReduce(func2, 11, redfunc) << std::endl;
   std::cout << "thrd without chunks -> " << t.MapReduce(func2, 11, redfunc) << std::endl;

}

int main(){
   numa();
   return 0;
}
