$usage = "Usage: perl pomdp_weighted.pl\n";

$arg = 0;
my $gamma = 0.9;
my $w1 = 1;
my $w2  = 0.5;
my $w3  = 0.95;
#print "w1.w2.w3 is: " , "$w1  $w2  $w3 \n";

my $namada = 0.5;


my $filename = "pdq";
my %qrelPDQ = (); #queryID:docID:score(0~1)
open (RUN, $filename) || die "$0: cannot open \"$filename \": !$\n";
while (<RUN>) {
     s/[\r\n]//g;

     my @data = split(/\s+/, $_);
     $qrelPDQ{$data[0]}{$data[1]} =  $data[2];
}
close (RUN);

$filename = "pd";
my %qrelPD = (); #docID:score(0~1)
open (RUN, $filename) || die "$0: cannot open \"$filename \": !$\n";
while (<RUN>) {
     s/[\r\n]//g;

     my @data = split(/\s+/, $_);
     $qrelPD{$data[0]} =  $data[1];
}
close (RUN);


$filename = "rslt/baseline";
my %yandex = (); #session:docID:score
my $preSession="";
open (RUN, $filename) || die "$0: cannot open \"$filename \": !$\n";
my $score=10;
while (<RUN>) {
     s/[\r\n]//g;

     my @data = split(/,/, $_);
     if($data[0] ne $preSession){
        $score = 10;
        $preSession = $data[0];
     }
     $yandex{$data[0]}{$data[1]} = $score / 10;
     $score--;
}
close (RUN);

my %longtermReward=();
#my @msg=();
foreach my $session (keys %yandex){
   my %docs = %{$yandex{$session}};
   foreach my $doc (keys %docs){
       my $value = 0;
       if(exists $qrelPDQ{$session}{$doc}){
          $value += $w1 * $qrelPDQ{$session}{$doc};
       }
        if(exists $qrelPD{$doc}){
          $value += $w2 * $qrelPD{$doc} * 100;
       }
       $value += $w3 * $yandex{$session}{$doc};
       $longtermReward{$session}{$doc} = $value; 
   }
}

$filename = "testReal";
my $cacheQuery="";
my $clickData = "";
my $line = "";
my $sessionID="";
my $preClickID = "";
my $preTime = 0;
my $dwell=0;
my @queries=();
my @clicks=();

my %sessionReward=();
open (RUN, $filename) || die "$0: cannot open \"$filename \": !$\n";
while (<RUN>) {
     s/[\r\n]//g;
     $line = $_;

     my @data = split(/\s+/ ,$_);

     if($data[1] eq "M"){
         if($cacheQuery ne ""){
              push(@queries,$cacheQuery);
              push(@clicks,$clickData);

              foreach my $i (1..$#queries)
              {
                  my @qData = split(/\s+/,$queries[$#queries - $i]);
                  $preTime = $qData[1];

                  if($clicks[$i] eq ""){
                     next;
                  }

                  my @clicks = split(/\n/,$clicks[$#queries - $i]);
                  foreach my $click (@clicks)
                  {
                     my @clickData = split(/\s+/, $click);
                     my $dwellTime = $clickData[1] -  $preTime;
                     my $rel=0;
                     if($dwellTime>=50){$rel=1;}
                     if($dwellTime>=400){$rel=2;}
                     $preTime = $clickData[1];
                     if(exists $sessionReward{$sessionID}{$clickData[4]}){
                        $sessionReward{$sessionID}{$clickData[4]} += ($gamma ** $i) * $rel;
                     }else{
                        $sessionReward{$sessionID}{$clickData[4]} = ($gamma ** $i) * $rel;
                     }
                  }

                  my $maxV = 0;
                  my %sessionTmp = %{$sessionReward{$sessionID}};
                  foreach my $urlTmp (keys %sessionTmp){
                      if($sessionReward{$sessionID}{$urlTmp} > $maxV) {$maxV = $sessionReward{$sessionID}{$urlTmp}; }
                  }
                  foreach my $urlTmp (keys %sessionTmp){
                     if($maxV > 0){
                        $sessionReward{$sessionID}{$urlTmp} = $sessionReward{$sessionID}{$urlTmp}/ $maxV;
                     }
                  }
              }

              $cacheQuery = "";
              $clickData = "";
              @queries=();
              @clicks=();
         }

         $sessionID = $data[0];
     }

     if($data[2] eq "Q" || $data[2] eq "T" ){
         if($cacheQuery ne ""){
              push(@queries,$cacheQuery);
              push(@clicks,$clickData);
         }
         $cacheQuery = $line;
         $clickData = "";
     }

     if($data[2] eq "C"){
         $clickData = $clickData.$line."\n";
     }
}

if($cacheQuery ne ""){
        push(@queries,$cacheQuery);
        push(@clicks,$clickData);

        foreach my $i (1..$#queries)
        {
              my @qData = split(/\s+/,$queries[$#queries - $i]);
              $preTime = $qData[1];

              if($clicks[$i] eq ""){
                 next;
              }

              my @clicks = split(/\n/,$clicks[$#queries - $i]);
              foreach my $click (@clicks)
              {
                 my @clickData = split(/\s+/, $click);
                 my $dwellTime = $clickData[1] -  $preTime;
                 my $rel=0;
                 if($dwellTime>=50){$rel=1;}
                 if($dwellTime>=400){$rel=2;}
                 $preTime = $clickData[1];
                 if(exists $sessionReward{$sessionID}{$clickData[4]}){
                    $sessionReward{$sessionID}{$clickData[4]} += ($gamma ** $i) * $rel;
                 }else{
                    $sessionReward{$sessionID}{$clickData[4]} = ($gamma ** $i) * $rel;
                 }
              }

              my $maxV = 0;
              my %sessionTmp = %{$sessionReward{$sessionID}};
              foreach my $urlTmp (keys %sessionTmp){
                 if($sessionReward{$sessionID}{$urlTmp} > $maxV) {$maxV = $sessionReward{$sessionID}{$urlTmp}; }
              }
              foreach my $urlTmp (keys %sessionTmp){
                 if($maxV > 0){
                    $sessionReward{$sessionID}{$urlTmp} = $sessionReward{$sessionID}{$urlTmp}/ $maxV;
                 }
              }
        }

        $cacheQuery = "";
        $clickData = "";
        @queries=();
        @clicks=();
}
close(RUN);

my @actions = ("StaySame"); #, "ClickTop", "ClickDown", "Novelty"
my %states = ();
$states{"RR"} = (1 - 1.5/1.885)* 0.057;
$states{"NRR"} = (1-1.5/1.885)*(1 - 0.057);
$states{"RT"} = (1.5/1.885)* 0.057;
$states{"NRT"} = (1.5/1.885)*(1 - 0.057);

my %rf = ();
$rf{"RR"}{"ClickTop"} = -1;
$rf{"RR"}{"ClickDown"} = -1;
$rf{"RR"}{"StaySame"} = -1;
$rf{"RR"}{"Novelty"} = 1;
$rf{"NRR"}{"ClickTop"} = -1;
$rf{"NRR"}{"ClickDown"} = 1;
$rf{"NRR"}{"StaySame"} = -1;
$rf{"NRR"}{"Novelty"} = -1;
$rf{"RT"}{"ClickTop"} = 1;
$rf{"RT"}{"ClickDown"} = -1;
$rf{"RT"}{"StaySame"} = -1;
$rf{"RT"}{"Novelty"} = -1;
$rf{"NRT"}{"ClickTop"} = -1;
$rf{"NRT"}{"ClickDown"} = -1;
$rf{"NRT"}{"StaySame"} = 1;
$rf{"NRT"}{"Novelty"} = -1;

$filename = "finalTrans";
my %sessionStates = (); #sessionID:state
open (RUN, $filename) || die "$0: cannot open \"$filename \": !$\n";
while (<RUN>) {
     s/[\r\n]//g;

     my @data = split(/\s+/, $_);
     $sessionStates{$data[0]} =  $data[1];
}
close (RUN);

# "-1 -1 -1 -1" , "-1 -1 -1 1" , "-1 -1 1 -1" , "-1 -1 1 1" , "-1 1 -1 -1" , "-1 1 -1 1" , "-1 1 1 -1" , "-1 1 1 1" , "1 -1 -1 -1" , "1 -1 -1 1" , "1 -1 1 -1" , "1 -1 1 1" , "1 1 -1 -1" , "1 1 -1 1" , "1 1 1 -1" ,"1 1 1 1"
my @combine = ("-1 -1 -1 1");

my $theta = 1;
foreach my $label (8..8){
$namada = $label/10;

  foreach $com (@combine){
    my @spin = split(/\s+/, $com);
    my $a = $spin[0];
    my $b = $spin[1];
    my $c = $spin[2];
    my $d = $spin[3];

    open (TESTFILE, ">rslt/pomdpv7_$a$b$c$d\_nama$namada");
    open (TESTFILE1, ">rslt/pomdpv7_global");
    open (TESTFILE2, ">rslt/pomdpv7_local");
    open (EVALFILE, ">>eval.sh");
    print EVALFILE "java evaluation rslt/pomdpv7_$a$b$c$d\_nama$namada \n";
    close (EVALFILE);

    foreach my $session (keys %yandex){
         if(exists $sessionStates{$session}){
             my $sessionState = $sessionStates{$session};
             if($sessionState eq "NonRelExplore"){
                $theta = $a;
             }
             if($sessionState eq "RelExplore"){
                $theta = $b;
             }
             if($sessionState eq "NonRelExploit"){
                $theta = $c;
             }
             if($sessionState eq "RelExploit"){
                $theta = $d;
             }
         }

         my @optRslt = ();
         my $optAct = "";
         my $optValue = -90000000;
         my @optGlobal = ();
         my @optLocal = ();

         foreach my $act (@actions){
            my @actRslt = &actionSub($session, $act);
            my $value = 0;
            my $index = 0;
            my @tmpRslt = ();
            my @tmpGlobal = ();
            my @tmpLocal = ();
            foreach my $line (@actRslt){
                 $index++;
                 my @datas = split(/\s+/ , $line);
                 $value = 0;
                 $value += ($namada * $longtermReward{$session}{$datas[1]}) / (log($index + 1)/log(2));

                 my $v1 = ($namada * $longtermReward{$session}{$datas[1]}) / (log($index + 1)/log(2));
                 push(@tmpGlobal, $session."\t".$datas[1]."\t".$v1);

                 $value += (1-$namada)*$theta*$sessionReward{$session}{$datas[1]}/ (log($index + 1)/log(2));
                 $v1 = (1-$namada)*$theta*$sessionReward{$session}{$datas[1]}/ (log($index + 1)/log(2));
                 push(@tmpLocal, $session."\t".$datas[1]."\t".$v1);
                 
                 push(@tmpRslt, $session."\t".$datas[1]."\t".$value);
            }

            if($value > $optValue || $optValue == -90000000){
               $optValue = $value;
               $optAct = $act;
               @optRslt = sort byPd @tmpRslt;
               @optGlobal = sort byPd @tmpGlobal;
               @optLocal = sort byPd @tmpLocal;
            }       
         }

         foreach my $line (@optRslt){
            my @tmp = split(/\s+/, $line);
            print TESTFILE $tmp[0].",".$tmp[1].",".$tmp[2]."\n";
         }
         foreach my $line (@optGlobal){
            my @tmp = split(/\s+/, $line);
            print TESTFILE1 $tmp[0].",".$tmp[1].",".$tmp[2]."\n";
         }
         foreach my $line (@optLocal){
            my @tmp = split(/\s+/, $line);
            print TESTFILE2 $tmp[0].",".$tmp[1].",".$tmp[2]."\n";
         }
    }
    close (TESTFILE);
    close (TESTFILE1);
    close (TESTFILE2);
  }

}

sub actionSub{
   my $sessionID = $_[0];
   my $action = $_[1];
   
   my @clickedDocs = ();
   my @clickedPosDocs = ();
   my @unclickedPosDocs = ();
   my @unclickedDocs = ();

   my %docs = %{$yandex{$sessionID}};
   my @msg=();
   foreach my $doc(keys %docs){
      push(@msg, $sessionID."\t".$doc."\t".$yandex{$sessionID}{$doc});
   }
   @tmpsorted = sort byPd @msg;
   @sorted = ();
   foreach my $line(@tmpsorted ){
       my @tmp = split(/\s+/, $line);
       push(@sorted, $tmp[0]."\t".$tmp[1]."\t"."0");
   }

   @sorted;
}

sub byPd{
    @dataA=split(/\s+/,$a);
    @dataB=split(/\s+/,$b);
    $dataB[2] <=> $dataA[2];
}

sub byIncrease{
    @dataA=split(/\s+/,$a);
    @dataB=split(/\s+/,$b);
    $dataA[2] <=> $dataB[2];
}