#!/bin/sh
if [ "$#" -ne 1 ];
then
  echo "Usage: $0 should input version" >&2
  exit 1
fi

#compile
g++ -std=c++0x -g -o optimal  -I/data1/home/jl1749/sigir15_full_session/indri-5.0/include -I/data1/home/jl1749/sigir15_full_session/indri-5.0/contrib/lemur/include -DP_NEEDS_GNU_CXX_NAMESPACE=1 optimal.cpp /data1/home/jl1749/sigir15_full_session/indri-5.0/contrib/lemur/obj/liblemur.a /data1/home/jl1749/sigir15_full_session/indri-5.0/install/lib/libindri.a -lstdc++ -lm -lpthread -lz

#run
./optimal -session=sessiontrack2014.xml -fbDocs=20 -topic=2014TopicMap.txt -qrel=eval/qrel.2014.txt -badquery=0.8 -rule=method:d,mu:5000 -past=1 -alpha=2.2 -beta=1.8 -gamma=0.4 -runid=optimal_version_$1 -index=/data1/index/ClueWeb12CatB_Clean > optimal_version_$1\.log

#eval
perl eval/eval.pl eval/optimal_version_$1\.rl2 > eval/optimal_version_$1\.rslt
