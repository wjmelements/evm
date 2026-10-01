#!/usr/bin/perl
# diocmp.pl calls.json generated.json [expected.json]
#
# Checker for dio-generated configs.
# The generated config must replay exactly the input calls, in order:
# replay applies each entry in turn, running its constructTest and then its tests.
# With expected.json, it must also partially match the generated config.
# Arrays must have equal length and entries are matched positionally.
# For each entry pair: all keys present in expected must appear in generated
# with equal values. Extra keys in generated are ignored.

use strict;
use warnings;
use JSON;

my ($calls_file, $gen_file, $exp_file) = @ARGV;
die "usage: diocmp.pl calls.json generated.json [expected.json]\n" unless $gen_file;

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

sub tx {
    my ($to, $input) = @_;
    $input = '0x' unless defined $input;
    $input = "0x$input" unless $input =~ /^0x/i;
    return lc(($to // 'create') . ' ' . $input);
}

my $json      = JSON->new->utf8;
my $generated = $json->decode(slurp($gen_file));
die "generated must be a JSON array\n" unless ref($generated) eq 'ARRAY';

my $calls = $json->decode(slurp($calls_file));
$calls = [$calls] unless ref($calls) eq 'ARRAY';
my @called = map { tx($_->{to}, $_->{data} // $_->{input}) } @$calls;
my @replayed;
for my $entry (@$generated) {
    push @replayed, tx(undef, $entry->{initcode}) if $entry->{constructTest};
    push @replayed, tx($_->{to} // $entry->{address}, $_->{input}) for @{$entry->{tests} // []};
}
for my $i (0 .. ($#called > $#replayed ? $#called : $#replayed)) {
    my $want = $called[$i] // '(none)';
    my $got  = $replayed[$i] // '(none)';
    fail("replay $i: expected call \"$want\", got \"$got\"") unless $want eq $got;
}

if (defined $exp_file) {
    my $expected = $json->decode(slurp($exp_file));
    die "expected must be a JSON array\n" unless ref($expected) eq 'ARRAY';
    if (@$expected != @$generated) {
        fail("top level: expected " . scalar(@$expected) . " entries, got " . scalar(@$generated));
    } else {
        match_node("[$_]", $expected->[$_], $generated->[$_]) for 0 .. $#$expected;
    }
}

exit($failures > 0 ? 1 : 0);
