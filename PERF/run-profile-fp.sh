#!/bin/bash

NAME="i-go-crazy-cern" #Your source code name without .cpp

#Compile example
#g++ -o $NAME -g -fno-omit-frame-pointer $NAME.cxx `root-config --cflags --glibs`
g++ -o $NAME -g -fno-omit-frame-pointer $NAME.cpp -I/data/ikabadzh/august_investigation/TBB_standalone/oneTBB/include -L/data/ikabadzh/august_investigation/TBB_standalone/oneTBB/build/install/lib64/ -ltbb
#Set arguments
COMMAND=./$NAME
DATA_FOLDER="Data"
OUT_FOLDER="Graphs"

#Create folders
[ ! -d $DATA_FOLDER ] && mkdir $DATA_FOLDER
[ ! -d $OUT_FOLDER ] && mkdir $OUT_FOLDER

#Run and profile
CLING_PROFILE=1 perf record --call-graph=fp -F 9999 -o $DATA_FOLDER/raw.data $COMMAND

#tools location
TOOL_PATH="./tools"

#Fold data
perf script --no-demangle -i $DATA_FOLDER/raw.data > $DATA_FOLDER/scripted.data
c++filt -p < $DATA_FOLDER/scripted.data > $DATA_FOLDER/after_filt.data
$TOOL_PATH/stackcollapse.pl --all $DATA_FOLDER/after_filt.data > $DATA_FOLDER/folded_by_sc.data

#Flame Graph
$TOOL_PATH/flamegraph.pl -w 1500 --colors java $DATA_FOLDER/folded_by_sc.data > $OUT_FOLDER/flamegraph.svg

#G2D
#python3 $TOOL_PATH/gprof2dot.py -f perf -o $DATA_FOLDER/graph.dot $DATA_FOLDER/after_filt.data
#dot -Tpng -o $OUT_FOLDER/graph.png $DATA_FOLDER/graph.dot
