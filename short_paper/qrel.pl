$usage = "Usage: perl qrel.pl\n";

$arg = 0;
$filename = "subdata/test";

my $preCacheMsg = "";
my $cacheQuery="";
my $clickData = "";
my %qrel = ();
my $line = "";
my $sessionID="";

my $preClickID = "";
my $preTime = 0;
my $dwell=0;

#open (TESTFILE, '>subdata/rslt/baseline');
open (TRAINFILE, '>subdata/qrel');
open (MAPFILE, '>subdata/topicMap');

open (RUN, $filename) || die "$0: cannot open \"$filename \": !$\n";
while (<RUN>) {
     s/[\r\n]//g;
     $line = $_;

     my @data = split(/\s+/ ,$_);

     if($data[1] eq "M"){
         if($cacheQuery ne ""){
              my @qData = split(/\s+/, $cacheQuery);
              $preTime = $qData[1];
              my @cData = split(/\n/, $clickData);

              foreach my $i (0..$#cData)
              {
                 my $cLine = $cData[$i];
                 my @cDetail = split(/\s+/, $cLine);

                 $dwell = $cDetail[1] - $preTime;
                 $preTime = $cDetail[1];
                 my $urlID = $cDetail[4];
                 my $rel = 0;
                 if($dwell >= 50)
                 {
                    $rel = 1;
                 }
                 if($dwell >= 400)
                 {
                    $rel = 2;
                 }
                 if($i == $#cData)
                 {
                    $rel = 2;
                 }

                 if(exists $qrel{$sessionID}{$urlID})
                 {
                    if($rel > $qrel{$sessionID}{$urlID})
                    {
                       $qrel{$sessionID}{$urlID} = $rel;
                    }
                 }
                 else
                 {
                    $qrel{$sessionID}{$urlID} = $rel;
                 }
              }

              $preCacheMsg = "";
              $cacheQuery = "";
              $clickData = "";
         }

         $sessionID = $data[0];
         $preCacheMsg = $line."\n";
     }

     if($data[2] eq "Q"){
           $preCacheMsg = $preCacheMsg.$cacheQuery.$clickData;
           $cacheQuery = $line."\n";
           $clickData = "";
     }

      if($data[2] eq "C"){
           $clickData = $clickData.$line."\n";
      }
}

if($preCacheMsg ne ""){
   my @qData = split(/\s+/, $cacheQuery);
   $preTime = $qData[1];
   my @cData = split(/\n/, $clickData);

   foreach my $i (0..$#cData)
   {
      my $cLine = $cData[$i];
      my @cDetail = split(/\s+/, $cLine);

      $dwell = $cDetail[1] - $preTime;
      $preTime = $cDetail[1];
      my $urlID = $cDetail[4];
      my $rel = 0;
      if($dwell >= 50)
      {
         $rel = 1;
      }
      if($dwell >= 400)
      {
         $rel = 2;
      }
      if($i == $#cData)
      {
         $rel = 2;
      }

      if(exists $qrel{$sessionID}{$urlID})
      {
         if($rel > $qrel{$sessionID}{$urlID})
         {
            $qrel{$sessionID}{$urlID} = $rel;
         }
      }
      else
      {
         $qrel{$sessionID}{$urlID} = $rel;
      }
   }

   $preCacheMsg = "";
   $cacheQuery = "";
   $clickData = "";
}

foreach my $session (keys %qrel){
   my %sessionQrel = %{$qrel{$session}};
   foreach my $url(keys %sessionQrel){
      print TRAINFILE $session, "\t", $url, "\t", $sessionQrel{$url}, "\n";
   }
}

close (RUN);
close (TRAINFILE);
#close (TESTFILE);
close (MAPFILE);