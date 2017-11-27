#!/bin/sh
if [ "$#" -ne 1 ];
then
  echo "Usage: $0 should input version" >&2
  exit 1
fi

#compile
g++ -std=c++0x -g -o optimal  -I/data1/home/jl1749/sigir15_full_session/indri-5.0/include -I/data1/home/jl1749/sigir15_full_session/indri-5.0/contrib/lemur/include -DP_NEEDS_GNU_CXX_NAMESPACE=1 optimal.cpp /data1/home/jl1749/sigir15_full_session/indri-5.0/contrib/lemur/obj/liblemur.a /data1/home/jl1749/sigir15_full_session/indri-5.0/install/lib/libindri.a -lstdc++ -lm -lpthread -lz

#run
./optimal -session=sessiontrack2013_87_C.xml -topic=2013SessionTopic.txt -qrel=eval/2013.qrels.txt -rule=method:d,mu:5000 -past=1 -alpha=2.2 -beta=1.8 -gamma=0.4 -fbDocs=20 -runid=optimal_version_$1 -index=/data1/index/ClueWeb12CatB_Clean > optimal_version_$1\.log

#evaluate
cd eval/
./session_eval_main.py --qrel_file=original-2013-qrels.txt --mapping_file=original-2013-sessiontopicmap.txt --run_file=optimal_version_$1\.rl2 > optimal_version_$1\.rslt
