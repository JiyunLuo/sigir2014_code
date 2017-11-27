To run the WSDM2014 workshop experiment,

1) run script baseline.pl.
  It extracts information from the test log and form the baseline which is the original rank provided by Yandex search log.
  
2) run script qrel.pl
  It generates some qrel files that are the input of our POMDP framework.
  
3) run script hideTest.pl
   It masks the qrel information from the test dataset and yield a dataset used to test the performance of pur re-ranking algorithm.
  
4) cd into folder "subdata"
   The file pomdpv7.pl is the version with the best performance in our experiments. Before run pomdpv7.pl, we need some pre-process about the data.

5) run script pd.ql
   The input of this script can be download from http://cs-sys-1.uis.georgetown.edu/~jl1749/paper_runs/sigir2014_short/QueryClickCount-2.zip
   The QueryClickCount-2.zip contains URL click counts in the qrel file, which will be used to generate reward score in the pomdpv7.pl
   
6) run script pdq.pl
   The input of this script can be download from http://cs-sys-1.uis.georgetown.edu/~jl1749/paper_runs/sigir2014_short/pdq.zip 
   It also contains some clicks counts from the qrel, however it is counted based on individual queries.
   
7) run script pomdpv7.pl



 
