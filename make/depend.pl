#!/usr/bin/perl
while (<>) {
    chomp;
    @depends = split;
    foreach $item (@depends) {
        if ($item =~ /(\w+)\.o/) {
            if (-e "src/$1.cpp" or -e "src/$1.c") {
                print " $item";
            }
        } else {
            if ($item =~ /\w+:/) {
                print $item;
            } else {
                print " $item"
            }
        }
    }
    print "\n";
}
