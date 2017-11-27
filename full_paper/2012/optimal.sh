#!/bin/sh
if [ "$#" -ne 1 ];
then
  echo "Usage: $0 should input version" >&2
  exit 1
fi

#compile
g++ -std=c++0x -g -o optimal  -I/data1/home/jl1749/sigir15_full_session/indri-5.0/include -I/data1/home/jl1749/sigir15_full_session/indri-5.0/contrib/lemur/include -DP_NEEDS_GNU_CXX_NAMESPACE=1 optimal.cpp /data1/home/jl1749/sigir15_full_session/indri-5.0/contrib/lemur/obj/liblemur.a /data1/home/jl1749/sigir15_full_session/indri-5.0/install/lib/libindri.a -lstdc++ -lm -lpthread -lz

#runs the run
./optimal -session=topic_sessiontrack2012-082312.xml -fbDocs=20 -topic=2012SessionTopic.txt -qrel=eval/2012.qrels.txt -rule=method:d,mu:5000  -past=0.92 -alpha=2.2 -beta=1.8 -gamma=0.4  -index=/data2/repository/ClueWeb09CatB -spam=clueweb09spam.GroupX.catb -runid=optimal_version_$1  > optimal_version_$1\.log

#run the evaluation
cd eval
./session_eval_main.py --qrel_file=original_2012_qrels.txt --mapping_file=original_sessiontopicmap.txt --run_file=optimal_version_$1\.rl2 > optimal_version_$1\.rslt
