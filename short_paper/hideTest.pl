$usage = "Usage: perl hideTest.pl inputFile\n";

$arg = 0;
$filename = "subdata/test";

my $preCacheMsg = "";
my $cacheQuery="";
my $clickData = "";
my %qrel = ();
my $line = "";
my $sessionCount= 0;
my $sessionID="";

my $preClickID = "";
my $preTime = 0;
my $dwell=0;

open (TESTFILE, '>subdata/testReal');
#open (TRAINFILE, '>subdata/qrel');
#open (MAPFILE, '>subdata/topicMap');

open (RUN, $filename) || die "$0: cannot open \"$filename \": !$\n";
while (<RUN>) {
     s/[\r\n]//g;
     $line = $_;

     my @data = split(/\s+/ ,$_);

     if($data[1] eq "M"){
           $sessionCount++;
           $sessionID = $data[0];

         if($cacheQuery ne ""){
            print TESTFILE $preCacheMsg;
            $cacheQuery =~ s/Q/T/;
            print TESTFILE $cacheQuery;

            $preCacheMsg = "";
            $cacheQuery = "";
            $clickData = "";
         }
         #$preTime = 0;
         $preCacheMsg = $line."\n";
     }

      if($data[2] eq "Q"){
           $preCacheMsg = $preCacheMsg.$cacheQuery.$clickData;
           $cacheQuery = $line."\n";
           $clickData = "";

           #$preTime = $data[1];
      }

      if($data[2] eq "C"){
           $clickData = $clickData.$line."\n";

           #$dwell = $data[1] - $preTime;
           #my $urlID = $data[4];
           #my $rel = 0;
           #if($dwell >= 50){
           #   $rel = 1;
           #}
           #if($dwell >= 400){
           #   $rel = 2;
           #}

           #if(exists $qrel{$urlID})

           #$preTime = $data[1];
      }
}

if($preCacheMsg ne ""){
   print TESTFILE $preCacheMsg;
   $cacheQuery =~ s/Q/T/;
   print TESTFILE $cacheQuery;

   $preCacheMsg = "";
   $cacheQuery = "";
   $clickData = "";
}

close (RUN);
#close (TRAINFILE);
close (TESTFILE);
#close (MAPFILE);

print "session Num:" , $sessionCount, "\n";