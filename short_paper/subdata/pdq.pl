$usage = "Usage: perl pdq.pl\n";

$arg = 0;
$filename = "pdq.txt";

my %qrel = ();
my %sumQrel = ();

open (TESTFILE, '>pdq');

open (RUN, $filename) || die "$0: cannot open \"$filename \": !$\n";
while (<RUN>) {
     s/[\r\n]//g;

     my @data = split(/\s+/, $_);
     $allClicks = $allClicks + $data[1];
     $qrel{$data[0]}{$data[1]} =  $data[2];
      if(exists $sumQrel{$data[0]}){
         $sumQrel{$data[0]} = $sumQrel{$data[0]} + $data[2];
      }else{
         $sumQrel{$data[0]} = $data[2];
      }
}

my @msg=();
foreach my $query (keys %qrel){
   my %docs = %{$qrel{$query}};
   foreach my $doc (keys %docs){
      my $value = $query."\t".$doc."\t".($docs{$doc} / $sumQrel{$query});
      push(@msg,$value); 
   }
}

my @sorted = sort byPd @msg;

foreach my $doc (@sorted){
   print TESTFILE $doc, "\n";
}

close (RUN);
close (TESTFILE);

sub byPd{
    @dataA=split(/\s+/,$a);
    @dataB=split(/\s+/,$b);
     if($dataA[0] != $dataB[0]){
        $dataA[0] <=> $dataB[0]
     }else{
       $dataA[2] <=> $dataB[2];
     }
}