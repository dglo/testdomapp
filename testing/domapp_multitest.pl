#!/usr/bin/perl

# upload_and_test.pl
# John Jacobsen, NPX Designs, Inc., jacobsen\@npxdesigns.com
# Started: Sat Nov 20 13:18:25 2004

package MY_PACKAGE;
use strict;
use Getopt::Long;

sub testDOM;
sub mydie { die "FAIL: ".shift; }

sub usage { return <<EOF;
Usage: $0 [options] <dom0> <dom1> ...
Options: 
    -u <image>
    Upload <image> rather than use domapp already in flash.

    DOMs can be \"all\" or, e.g., \"01a, 10b, 31a\"
    Must power on and be in iceboot first.

EOF
;
}

my $help;
my $image;
GetOptions("help|h"          => \$help,
	   "upload|u=s"      => \$image) || die usage;

die usage if $help;

my $dat = "/usr/local/bin/domapptest";
mydie "Can't find domapptest program $dat.\n" unless -e $dat;
my @doms   = @ARGV;
if(@doms == 0) { $doms[0] = "all"; }

if(defined $image) {
    mydie "Can't find domapp image (\"$image\")!  $0 -h for help.\n" 
	unless -f $image;
}

my %card;
my %pair;
my %aorb;

print "Available DOMs: ";
if($doms[0] eq "all") {
    my @iscomstr = 
	`cat /proc/driver/domhub/card*/pair*/dom*/is-communicating|grep "is communicating"`;
    my $found = 0;
    for(@iscomstr) {
	if(/Card (\d+) Pair (\d+) DOM (\S+) is communicating/) {
	    my $dom = "$1$2$3"; $dom =~ tr/A-Z/a-z/; 
	    $card{$dom} = $1;
	    $pair{$dom} = $2;
	    $aorb{$dom} = $3; $aorb{$dom} =~ tr/A-Z/a-z/;
	    print "$dom ";
	    $doms[$found] = $dom;
	    $found++;
	}
    }
    mydie "No DOMs are communicating - did you power on?\n"
	."(Don't forget to put DOMs in Iceboot after powering on!\n" unless $found;
} else {
    foreach my $dom (@doms) {
	if($dom =~ /(\d)(\d)(\w)/) {
	    $card{$dom} = $1;
            $pair{$dom} = $2;
            $aorb{$dom} = $3; $aorb{$dom} =~ tr/A-Z/a-z/;
            print "$dom ";
	} else {
	    mydie "Bad dom argument $dom.  $0 -h for help.\n";
	}
    }
}
print "\n";

# implement serially now, but think about parallelizing later


foreach my $dom (@doms) {
    mydie "Test of domapp (image ".(defined $image?"$image":"in flash").") on $dom failed!\n"
	unless testDOM($dom);
}

print "$0: SUCCESS\n";

exit;

sub softboot { 
    my $dom = shift;
    print "Softbooting $dom... ";
    my $result = `/usr/local/bin/sb.pl $dom`;
    return 0 if $result !~ /ok/i;
    print "OK.\n";
    return 1;
}

sub upload {
    my $dom = shift;
    my $image = shift;
    print "Uploading $image to $dom... ";
    my $uploadcmd = "/usr/local/bin/upload_domapp.pl $card{$dom} $pair{$dom} $aorb{$dom} $image";
    my $tmpfile = ".tmp_ul_$dom"."_$$";
    system "$uploadcmd 2>&1 > $tmpfile";
    my $result = `cat $tmpfile`;
    unlink $tmpfile || mydie "Can't unlink $tmpfile!\n";
    if($result !~ /Done, sayonara./) {
        print "upload failed: session text:\n$uploadcmd\n\n$result\n\n";
        return 0;
    } else {
        print "OK.\n";
    }
    return 1;
}

sub versionTest {
    my $dom = shift;
    print "Checking version with domapptest... ";
    my $cmd = "$dat -V $dom 2>&1";
    my $result = `$cmd`;
    if($result !~ /DOMApp version is \'(.+?)\'/) {
	print "Version retrieval from domapp failed:\ncommand: $cmd\nresult:\n$result\n\n";
	return 0;
    } else {
	print "OK.  Version is $1.\n";
    } 
    return 1;
}

sub shortEchoTest {
    my $dom = shift;
    print "Performing short domapp echo message test... ";
    my $cmd = "$dat -d2 -E1 $dom 2>&1";
    my $result = `$cmd`;
    if($result =~ /Done \((\d+) usec\)\./) {
	print "OK ($1 usec).\n";
    } else {
	print "Short echo test failed:\n";
	print "Command: $cmd\n";
	print "Result:\n$result\n\n";
	return 0;
    }
    return 1;
}

sub shortMoniTest {
    my $dom = shift;
    print "Performing short test of monitoring... ";
    my $moniFile = "short_$dom.moni";
    my $cmd = "$dat -d2 -M1 -m $moniFile $dom 2>&1";
    my $result = `$cmd`;
    if($result =~ /Done \((\d+) usec\)\./) {
        print "done ($1 usec)... check contents: ";
    } else {
        print "Short echo test failed:\n";
        print "Command: $cmd\n";
        print "Result:\n$result\n\n";
        return 0;
    }
    my $dmtext = `/usr/local/bin/decodemoni -v $moniFile 2>&1`;
    if($dmtext !~ /MONI SELF TEST OK/) {
	print "Test failed: desired monitoring string was not present.\n";
	print "Monitoring output:\n$dmtext\n";
	return 0;
    } else {
	print "OK.\n";
    }
    return 1;
}

sub domappmode { 
    my $dom = shift;
    print "Putting DOM in domapp mode... ";
    my $cmd = "/usr/local/bin/se.pl $dom domapp domapp 2>&1";
    my $result = `$cmd`;
    if($result !~ /SUCCESS/) {
	print "Change state of DOM $dom to domapp failed:\n";
	print "Command: $cmd\n";
	print "Result:\n$result\n\n";
	return 0;
    } else {
	print "OK.\n";
    }
    return 1;
}

sub testDOM {
# Upload DOM software and test.  Return 1 if success, else 0.
    my $dom = shift;
    return 0 unless softboot($dom);
    if(defined $image) {
	return 0 unless upload($dom, $image);
    } else {
	return 0 unless domappmode($dom);
    }
    return 0 unless versionTest($dom);
    return 0 unless shortEchoTest($dom);
    return 0 unless shortMoniTest($dom);
    return 1;
}

__END__

