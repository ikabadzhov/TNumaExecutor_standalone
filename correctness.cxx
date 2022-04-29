#include "TNUMA_Alpha.hxx"
#include "ROOT/TSeq.hxx"
#include <iostream>

void numa(char executor[]){
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
   std::vector<long int> v(11);
   std::cout << "testing: with # repetitions, std::vector-s, TSeq-s\n";

   if (executor[0] == 'n') {
      ROOT::Experimental::TNUMAExecutor n;

      std::cout << "R: numa with chunks -> " << n.MapReduce(func2, 11, redfunc, 5) << std::endl;
      std::cout << "R: numa without chunks -> " << n.MapReduce(func2, 11, redfunc) << std::endl;

      std::cout << "V: numa with chunks -> " << n.MapReduce(func, v, redfunc, 5) << std::endl;
      std::cout << "V: numa without chunks -> " << n.MapReduce(func, v, redfunc) << std::endl;

      std::cout << "T: numa with chunks -> " << n.MapReduce(func, ROOT::TSeq<long int>(11), redfunc, 5) << std::endl;
      std::cout << "T: numa without chunks -> " << n.MapReduce(func, ROOT::TSeq<long int>(11), redfunc) << std::endl;
   }

   else if (executor[0] == 't') {
      ROOT::TThreadExecutor t;

      std::cout << "R: thrd with chunks -> " << t.MapReduce(func2, 11, redfunc, 5) << std::endl;
      std::cout << "R: thrd without chunks -> " << t.MapReduce(func2, 11, redfunc) << std::endl;

      std::cout << "V: thrd with chunks -> " << t.MapReduce(func, v, redfunc, 5) << std::endl;
      std::cout << "V: thrd without chunks -> " << t.MapReduce(func, v, redfunc) << std::endl;

      std::cout << "T: thrd with chunks -> " << t.MapReduce(func, ROOT::TSeq<long int>(11), redfunc, 5) << std::endl;
      std::cout << "T: thrd without chunks -> " << t.MapReduce(func, ROOT::TSeq<long int>(11), redfunc) << std::endl;
   }

   else { std::cout << "usage: valid args for executors are 'n' and 't'\n"; }
}

int main(int argc, char *argv[]){
   if (argc != 2) { std::cout << "usage: need to specify executor 'n' or 't'\n"; return -1; }
   numa(argv[1]);
   return 0;
}

