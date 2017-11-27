$usage = "Usage: perl qrel.pl\n";

$arg = 0;
$filename = "subdata/test";

#my $preCacheMsg = "";
my $cacheQuery="";
my $clickData = "";
my %qrel = ();
my $line = "";
my $sessionID="";

my $preClickID = "";
my $preTime = 0;
my $dwell=0;

open (TESTFILE, '>subdata/rslt/baseline');

open (RUN, $filename) || die "$0: cannot open \"$filename \": !$\n";
while (<RUN>) {
     s/[\r\n]//g;
     $line = $_;

     my @data = split(/\s+/ ,$_);

     if($data[1] eq "M"){
         if($cacheQuery ne ""){
              my @qData = split(/\s+/, $cacheQuery);
              foreach my $i (6..$#qData)
              {
                 my $line = $qData[$i];
                                        my @rank = split(/,/, $line);
                                        print  TESTFILE $sessionID , "," , $rank[0], "\n";
              }

              #$preCacheMsg = "";
              $cacheQuery = "";
              $clickData = "";
         }

         $sessionID = $data[0];
         #$preCacheMsg = $line."\n";
     }

     if($data[2] eq "Q"){
           #$preCacheMsg = $preCacheMsg.$cacheQuery.$clickData;
           $cacheQuery = $line."\n";
           $clickData = "";
     }

     #if($data[2] eq "C"){
     #      $clickData = $clickData.$line."\n";
     #}
}

if($cacheQuery ne ""){
      my @qData = split(/\s+/, $cacheQuery);
      foreach my $i (6..$#qData)
      {
         my $line = $qData[$i];
         my @rank = split(/,/, $line);
         print  TESTFILE $sessionID , "," , $rank[0], "\n";
      }

      #$preCacheMsg = "";
      $cacheQuery = "";
      $clickData = "";
}

close (RUN);
close (TESTFILE);