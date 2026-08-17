#!/usr/bin/perl
# diocmp.pl expected.json generated.json
#
# Partial-match checker for dio-generated configs.
# Arrays must have equal length and entries are matched positionally.
# For each entry pair: all keys present in expected must appear in generated
# with equal values. Extra keys in generated are ignored.

use strict;
use warnings;
use JSON;

my ($exp_file, $gen_file) = @ARGV;
die "usage: diocmp.pl expected.json generated.json\n" unless $gen_file;

sub slurp {
    open my $fh, '<', $_[0] or die "cannot open $_[0]: $!\n";
    local $/; <$fh>;
}

my $failures = 0;
sub fail { print STDERR "FAIL $_[0]\n"; $failures++ }

sub match_node {
    my ($path, $exp, $act) = @_;
    my $et = ref $exp;
    if ($et eq 'HASH') {
        fail("$path: expected object, got " . (ref($act) || 'scalar')) and return
            unless ref($act) eq 'HASH';
        for my $k (keys %$exp) {
            fail("$path.$k: key not found in generated") and next unless exists $act->{$k};
            match_node("$path.$k", $exp->{$k}, $act->{$k});
        }
    } elsif ($et eq 'ARRAY') {
        fail("$path: expected " . scalar(@$exp) . " elements, got " . scalar(@$act)) and return
            unless ref($act) eq 'ARRAY' && @$exp == @$act;
        match_node("$path\[$_]", $exp->[$_], $act->[$_]) for 0 .. $#$exp;
    } else {
        my $av = defined $act ? (ref($act) || $act) : 'undef';
        fail("$path: expected \"$exp\", got \"$av\"")
            unless !ref($act) && defined $act && $exp eq $act;
    }
}

my $json      = JSON->new->utf8;
my $expected  = $json->decode(slurp($exp_file));
my $generated = $json->decode(slurp($gen_file));

die "expected must be a JSON array\n"  unless ref($expected)  eq 'ARRAY';
die "generated must be a JSON array\n" unless ref($generated) eq 'ARRAY';

if (@$expected != @$generated) {
    fail("top level: expected " . scalar(@$expected) . " entries, got " . scalar(@$generated));
} else {
    match_node("[$_]", $expected->[$_], $generated->[$_]) for 0 .. $#$expected;
}

exit($failures > 0 ? 1 : 0);
