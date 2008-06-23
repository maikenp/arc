package LogUtils;

use POSIX;
use FileHandle;

use strict;

our ($ERROR, $WARNING, $INFO, $DEBUG) = (0, 1, 2, 3);
our @lnames = qw(ERROR WARNING INFO DEBUG);

our %opts = (
    logfiles => { '' => undef },    # default logging goes to STDERR
    levels   => { '' => $WARNING }  # default level is WARNING
);

# constructor

sub getLogger {
    my $class = shift;
    my $self = {name => (shift || '')};
    bless $self, $class;
    return $self;
}

# getters and setters

sub level() {
    my ($self,$level) = @_;
    return $self->_searchopt($opts{levels}) unless defined $level;
    return $opts{levels}{$self->{name}} = $level;
}

sub logfile() {
    my ($self,$logfile) = @_;
    return $self->_searchopt($opts{logfiles}) unless defined $logfile;
    return $opts{logfiles}{$self->{name}} = $logfile;
}

# convenience functions

sub error($$) {
    my ($self, $msg) = @_;
    $self->log($ERROR,$msg);
}

sub warning($$) {
    my ($self, $msg) = @_;
    $self->log($WARNING,$msg);
}

sub info($$) {
    my ($self, $msg) = @_;
    $self->log($INFO,$msg);
}

sub debug($$) {
    my ($self, $msg) = @_;
    $self->log($DEBUG,$msg);
}

# real work is done here

sub log($$$) {
    my ($self, $level, $msg) = @_;
    return if $level > $self->_searchopt($opts{levels});

    my $logfile = $self->_searchopt($opts{logfiles});

    unless ($logfile) {
        print STDERR $self->_format($level,$msg);
    } else {
        my $fh = new FileHandle ">>$logfile"
            or die "Error opening $logfile: $!";
        print $fh $self->_format($level,$msg);
        close $fh;
    }
}

sub _format {
    my ($self,$level,$msg) = @_;
    my $name = $self->{name};
    my ($sec,$min,$hour,$mday,$mon,$year,$wday,$yday,$isdst) = localtime (time);
    my $timestamp = POSIX::strftime("%Y-%m-%d %H:%M:%S", $sec,$min,$hour,$mday,
                                    $mon,$year,$wday,$yday,$isdst);
    $name = $name ? "$name: " : "";
    return "$timestamp $name$lnames[$level]: $msg\n";
}

# find settings which apply to this log object

sub _searchopt {
    my ($self, $opt) = @_;
    my $name = $self->{name};
    while (1) {
        return $opt->{$name} if exists $opt->{$name};
        $name =~ s/\.[^.]*$// or $name = '';
    }
}


sub test {
    my $log = LogUtils->getLogger();
    $log->warning("Hi");
    $log = LogUtils->getLogger("main");
    $log->warning("Hi");
    $log = LogUtils->getLogger("main.sub");
    print "LEvEl "  . $log->level()."\n";
    print "LEvEl "  . $log->level($LogUtils::INFO)."\n";
    print "Logfile ". $log->logfile("log.$$")."\n";
    $log->error("Hi");
    $log = LogUtils->getLogger("main.sub.one");
    $log->error("Hi");
    LogUtils->getLogger("main.sub.too")->info("Boo");
    LogUtils->getLogger("main.sub.too")->debug("Hoo");
    print "Contents of log.$$:\n". `cat log.$$; rm log.$$`;
}

#test();

1;
