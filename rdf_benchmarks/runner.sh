for files in 10 20
do
	for exe in df h o6 o7
	do
		perf stat -r 5 --append -o "PERF_STATS_${exe}_${files}.txt" numactl --physcpubind 0-15 ./${exe} ${files} >> dumpo.txt
		perf stat -r 5 --append -o "PERF_STATS_${exe}_${files}.txt" numactl --physcpubind 0-7,16-23 ./${exe} ${files} >> dumpo.txt
		perf stat -r 5 --append -o "PERF_STATS_${exe}_${files}.txt" numactl --physcpubind 0-7,16-23 ./${exe} ${files} 1 >> dumpo.txt
		perf stat -r 5 --append -o "PERF_STATS_${exe}_${files}.txt" numactl --physcpubind 0-7 ./${exe} ${files} >> dumpo.txt
		perf stat -r 5 --append -o "PERF_STATS_${exe}_${files}.txt" numactl --physcpubind 0-3,16-19 ./${exe} ${files} >> dumpo.txt
		perf stat -r 5 --append -o "PERF_STATS_${exe}_${files}.txt" numactl --physcpubind 0-3,16-19 ./${exe} ${files} 1 >> dumpo.txt
		perf stat -r 5 --append -o "PERF_STATS_${exe}_${files}.txt" numactl --physcpubind 0,1 ./${exe} ${files} >> dumpo.txt
		perf stat -r 5 --append -o "PERF_STATS_${exe}_${files}.txt" numactl --physcpubind 0,16 ./${exe} ${files} >> dumpo.txt
		perf stat -r 5 --append -o "PERF_STATS_${exe}_${files}.txt" numactl --physcpubind 0,16 ./${exe} ${files} 1 >> dumpo.txt
	done
done