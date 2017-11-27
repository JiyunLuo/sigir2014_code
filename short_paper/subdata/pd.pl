$usage = "Usage: perl qrel.pl\n";

$arg = 0;
$filename = "QueryClickCount-2.txt";

my %qrel = ();
my $allClicks = 0;

open (TESTFILE, '>pd');

open (RUN, $filename) || die "$0: cannot open \"$filename \": !$\n";
while (<RUN>) {
     s/[\r\n]//g;

     my @data = split(/\s+/, $_);
      $allClicks = $allClicks + $data[1];
      $qrel{$data[0]} =  $data[1];
}

print "allClick is:", $allClicks, "\n";

my @msg=();
foreach my $doc (keys %qrel){
   my $value = $doc."\t".($qrel{$doc} / $allClicks);
   push(@msg,$value); 
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
    $dataA[1] <=> $dataB[1];
}