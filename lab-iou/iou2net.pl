#!/usr/bin/perl

use strict;
use warnings;
use Getopt::Long;
use Net::Pcap;
use IO::Select;
use IO::Socket;
use IO::File;
use Time::HiRes qw(gettimeofday);

my $version      = "v0.5";
my $version_date = "20-Sep-2011";

###################################################################################
# CHANGES
# =======
#
# v0.5, 20-Sep-2011
# -----------------
# - changed capture filter to allow for l2iou MAC addresses (more generic approach)
# - added -m option to bypass MAC filtering completely or to allow to specify own
#   MAC address
#
# v0.4, 22-Apr-2011
# -----------------
# - for frame read, switched from fork to select
# - added bridging via udp links and tap interfaces
# - encoding issues hopefully fixed
# - more sane construction of IOU header and MAC address
# - verbose output
# - packet trace output (MAC headers)
# - writing traffic to pcap file
#
# v0.31, 28-Jan-2011
# -----------------
# - MAC address is now in "ether" format (bytes separated with ":") for building
#   the capture filter
#
# v0.3, 27-Jan-2011
# -----------------
# - better capture filter handling, after understanding how IOU generates
#   MAC addresses (related code is still ugly)
# - hostnames with hyphen are now accepted
#
# v0.21, 26-Jan-2011
# -----------------
# - changed socket_base handling after receiving hint that "1000" is the uid
#   that IOU is started with ;-)
#
# v0.2, 24-Jan-2011
# -----------------
# - added pcap filter to allow for better performance on busy nics
#
# v0.1, 23-Jan-2011
# -----------------
# - first release
#
###################################################################################

my $help = <<EOF;

iou2net.pl: bridge between iou and real networks (IOUlive replacement)

usage:
iou2net.pl 	[-vd] [-f capture file] [-i interface]  [-n netmapfile]
		[-p instance ID] [-t interface] [-u portmap]


NOTE:	-> You _must_ launch IOU before starting this script.
	-> -i, -t  or -u, and -p are required as a bare minimum.
	-> Most operation require super user privileges; use sudo or run as
	   root.

-v	
	Optional, provides verbose output

-d	
	Optional, provides debug output (verbose + prints frame headers)

-f	
	optional, write frames to a capture file that can be opened with
	wireshark

-i interface (PCAP mode)
	Specify the interface you want to bridge to. This makes the script
	to run in PCAP mode.

-n NETMAP file
	Optional. Per default, the script tries to open ./NETMAP. If you
	want to use a NETMAP file located elsewhere, use this argument.

-p instance ID
	IOU requires a pseudo instance. When bridging your IOU router
	interface, specify an unused ID as the target in your NETMAP file,
	like

	1:2/1\@hostname    666:1/0\@hostname

	666 is the pseudo IOU instance ID, hostname is the host where IOU
	and the script runs at. When starting the script, use -p 666 then.
	After launching the IOU router instance #1, use interface 2/1 for
	external connectivity. The Interface 1/0 at the pseudo instance
	does not have any practical meaning for router configuration.

-t interface (TAP mode)
	Specify the tap interface you want to attach to. This makes the script
	to run in TAP mode. If the interface does not exist, the script will
	create it, otherwise it will attach to it. You are responsible to
	have the interface in an "up" state and for any additional bridging
	that may be required.
	
-u portmap (UDP mode)
	Will establish communication through UDP links (dynamips, qemu).
	Portmap has the following format:

	(1) source-port:dest-port	or
	(2) source-port:remote_host:dest-port

	The first variant is used for UDP communication at the local host
	only (target runs at the same host).
	The second variant allows to communicate with a target that runs at
	a remote system. <remote_host> must be an IP address or a resolveable
	hostname/FQDN.
	Port numbers are always from a local (IOU) perspective, therefore
	they are the reverse of what gets defined at the target system.

-m <MAC address> (PCAP mode only)
	If <MAC address> is supplied, this address is used to build the
	capture filter. If <MAC address> is not specified, no capture filter
	will applied. This option should only be used for testing/debugging,
	because the default capture filter should work for any l3/l2iou
	instance out of the box.

CAVEATS: For now, you need to use x/y interface format in the NETMAP file, at 
least for the mapping this script requires. Also, for bridging multiple router
interfaces, separate instances of this script must be launched, and you need
an unique pseudo IOU ID per instance.

EOF

my $err;
my $verbose;
my $debug;
my $pcap_recv_data;
my $iou_recv_data;
my $iou_header;
my $iface;
my $netmap_file = "./NETMAP";
my $netmap_handle;
my $uid;
my $socket_base;
my $iou_pseudo_sock;
my $iou_router_sock;
my $pseudo_instance;
my $pseudo_instance_interface_major;
my $pseudo_instance_interface_minor;
my $iou_instance;
my $iou_interface_major;
my $iou_interface_minor;
my $select_handle;
my $pcap;
my $pcap_filter;
my $udp_conn;
my $udp_shost;
my $udp_dhost;
my $udp_spt;
my $udp_dpt;
my $udp_listener;
my $tap;
my $tap_handle;
my $cap_file;
my $cap_handle;
my $cap_dumper;
my $user_mac;

GetOptions(
    'help' => sub { print "$help"; exit(0); },
    'v+'   => \$verbose,
    'd+'   => \$debug,
    'i=s'  => \$iface,
    'n=s'  => \$netmap_file,
    'p=i'  => \$pseudo_instance,
    'u=s'  => \$udp_conn,
    't=s'  => \$tap,
    'f=s'  => \$cap_file,
    'm:s'  => \$user_mac
);

print "iou2net.pl, Version $version, $version_date.\n";

die "\nPlease provide -i, -t or -u, and -p!\n$help"
  unless ( ( $iface || $udp_conn || $tap ) && $pseudo_instance );

$verbose = 1 if $debug;

# socket directory is a directory below $TMPDIR (/tmp), composed of "netio" plus
# uid of the user that runs the iou binary
# since we assume this script gets invoked with sudo by most people:
# try to be smart about getting real UID, $< does not (always?) return real uid when using sudo

$uid         = $ENV{SUDO_UID};
$uid         = $< unless ( defined $uid );    # apparently not started with sudo
$socket_base = "/tmp/netio$uid";
mkdir $socket_base, 0755;
print "UID: $uid\n"                           if $verbose;
print "Socket base directory: $socket_base\n" if $verbose;

open( netmap_handle, $netmap_file )
  or die "Can't open netmap file $netmap_file\n";

# walk through NETMAP file and try to determine the source IOU instance
while (<netmap_handle>) {

    # stop when there is a match for our pseudo instance ID as the destination
    next
      if !( $_ =~
        m/^\d+:\d+\/\d+@[\w-]+[ \t]+$pseudo_instance:\d+\/\d+@[\w-]+(\s|\t)*$/
      );
    my $inputline = $_;
    chomp($inputline);

    print "Found valid mapping line in NETMAP: $inputline\n" if $verbose;

    # ignore any hostname statements
    $inputline =~ s/\@[\w-]+//g;

    my @connline = split( /[ \t]+/, $inputline );
    $connline[0] =~ s/(\s\t)*//g;
    $connline[1] =~ s/(\s\t)*//g;
    my @iou_src = split( /:/, $connline[0] );
    my @iou_dst = split( /:/, $connline[1] );
    $iou_instance = $iou_src[0];
    ( $iou_interface_major, $iou_interface_minor ) = split( /\//, $iou_src[1] );
    ( $pseudo_instance_interface_major, $pseudo_instance_interface_minor ) =
      split( /\//, $iou_dst[1] );
}
close(netmap_handle);
print
"Using pseudoinstance $pseudo_instance, interface $pseudo_instance_interface_major/$pseudo_instance_interface_minor\n"
  if $verbose;

die
"Could not find any valid mapping for IOU pseudo instance $pseudo_instance in NETMAP file"
  unless ( ( defined $iou_instance )
    && ( defined $iou_interface_major )
    && ( defined $iou_interface_minor )
    && ( defined $pseudo_instance_interface_major )
    && ( defined $pseudo_instance_interface_minor ) );

# unlink socket for IOU pseudo instance
unlink "$socket_base/$pseudo_instance";

# create socket for IOU pseudo instance
$iou_pseudo_sock = IO::Socket::UNIX->new(
    Type   => SOCK_DGRAM,
    Listen => 5,
    Local  => "$socket_base/$pseudo_instance"
) or die "Can't create IOU pseudo socket\n";

# availability to read shall be queried through select()
$select_handle = IO::Select->new();
$select_handle->add($iou_pseudo_sock);

# allow anyone to read and write
chmod 0666, "$socket_base/$pseudo_instance";

print "Created pseudo IOU socket at $socket_base/$pseudo_instance\n"
  if $verbose;
# attach to real IOU instance
$iou_router_sock = IO::Socket::UNIX->new(
    Type => SOCK_DGRAM,
    Peer => "$socket_base/$iou_instance"
) or die "Can't connect to IOU socket at $socket_base/$iou_instance\n";
print "Attached to real IOU socket at $socket_base/$iou_instance\n" if $verbose;

# precompute IOU header
# IOU header format
# Pos (byte)    value
# ==============================================================
# 00 - 01       destination (receiving) IOU instance ID
# 02 - 03       source (sending) IOU instance ID
# 04            receiving interface ID
# 05            sending interface ID
# 06 - 07       fixed delimiter, looks like its always 0x01 0x00
#
#               interface ID = <major int number> + (<minor int number> * 16)

$iou_header = pack( "nnCCH4",
    $iou_instance,
    $pseudo_instance,
    ( $iou_interface_minor << 4 ) | $iou_interface_major,
    ( $pseudo_instance_interface_minor << 4 ) |
      $pseudo_instance_interface_major,
    "0100" );

print "Precomputed IOU Header: ", unpack( "H*", $iou_header ), "\n" if $verbose;

# provide a clean exit
$SIG{INT} = \&caught_sigint;

# Open capture file
if ( defined $cap_file ) {
    $cap_handle = Net::Pcap::pcap_open_dead( DLT_EN10MB, 1500 );
    $cap_dumper = Net::Pcap::pcap_dump_open( $cap_handle, $cap_file )
      or die "Cant open capture file: $!";
    print "Opened file $cap_file for packet dump.\n" if $verbose;
}

# Determine Mode and setup sender and receiver logic
if ( defined $iface ) {
    print "Working in pcap mode.\n" if $verbose;

    # bind to network interface, promiscuous mode
    $pcap = Net::Pcap::open_live( $iface, 1522, 1, 1, \$err );
    die "pcap: can't open device $iface: $err (are you root?)\n"
      unless ( defined $pcap );

    # construction of IOU MAC address for external connectivity (L3IOU)
    # Pos (byte)            value
    # ==============================================================
    # 0 (high nibble)       from IOU instance ID (2 bytes, only 10 bits used),
    #                       the two least significant bits from the high byte
    #                       are taken and shifted one bit left
    # 0 (low nibble)        always 0xE
    # 1 - 3                 UID of the user that runs the IOU instance
    # 4                     low byte of the IOU instance ID
    # 5                     interface ID
    #
    # for x64 systems, binary math works well, like
    # $mac = (((($iou_instance & 0x0300) << 1 ) << 36 ) + 0xE0000000000 );
    # $mac += $uid << 16;
    # $mac += ($iou_instance & 0xFF) << 8;
    # $mac += ($iou_interface_minor << 4) + $iou_interface_major;

    my $macstring;

    if ( defined $user_mac ) {
        if ($user_mac) {
            $macstring = $user_mac;
        }
        else {
            $macstring = "";
        }
    }
    else {
        $macstring = pack( "CH6CC",
            ( ( $iou_instance >> 7 & 6 ) << 8 ) + 0xE,
            unpack( "xH6", pack( "N", 0xFF000000 ^ $uid ) ),
            $iou_instance & 0xFF,
            ( $iou_interface_minor << 4 ) | $iou_interface_major );

        $macstring = uc( join( ":", unpack( "(H2)*", $macstring ) ) );
    }

    if ($macstring) {
        print "Using MAC $macstring.\n" if $verbose;

        # build a capture filter for IOU interface MAC address
        # this will match only what is destined to $macstring, plus multicasts
        # and broadcoasts
        # for L2IOU, traffic destined to OIDs 02:<UID>:<UID> and AA:BB:CC is
        # included in the filter, too

        Net::Pcap::compile(
            $pcap, \$pcap_filter,
            '(ether[0] & 1 = 1) or 
		(ether dst ' . $macstring . ') or 	
		(ether[0] = 0x02 and ether[1:2] = 0x'
              . unpack( "H4", pack( "n", $uid ) ) . ') or 
		(ether[0] = 0xaa and ether[1:2] = 0xbbcc)',
            0, 0xFFFFFFFF
        ) && die 'Unable to compile capture filter';

        Net::Pcap::setfilter( $pcap, $pcap_filter )
          && die 'Unable to assign capture filter';

        print "Capture filter set: (ether[0] & 1 = 1) or (ether dst '"
          . $macstring . "') or
	            (ether[0] = 0x02 and ether[1:2] = 0x"
          . unpack( "H4", pack( "n", $uid ) ) . ") or 
	            (ether[0] = 0xaa and ether[1:2] = 0xbbcc)\n"
          if $verbose;

    }
    else {
        print "No capture filter set (empty -m option)\n" if $verbose;
    }

    print
"Forwarding frames between interface $iface and IOU instance $iou_instance, int $iou_interface_major/$iou_interface_minor (MAC: $macstring) -  press ^C to exit\n";

    while (1) {

        if ( grep { $_ eq $iou_pseudo_sock } $select_handle->can_read(0.001) ) {

            # IOU frame received via pseudo ID socket
            $iou_pseudo_sock->recv( $iou_recv_data, 1522 );
            log_iou_frame( "R:I->P", $iou_recv_data ) if $debug;

            $iou_recv_data = unpack( "x8a*", $iou_recv_data );

            # send IOU generated frame to real network
            Net::Pcap::sendpacket( $pcap, $iou_recv_data );
            write_pcap_dump($iou_recv_data) if $cap_dumper;
            log_frame( "S:I->P", $iou_recv_data ) if $debug;
        }
        else {

            my %pcap_hdr;
            my $return =
              Net::Pcap::pcap_next_ex( $pcap, \%pcap_hdr, \$pcap_recv_data );
            if ( $return eq 1 ) {
                write_pcap_dump($pcap_recv_data) if $cap_dumper;
                log_frame( "R:P->I", $pcap_recv_data ) if $debug;

                # add IOU header in front of the received frame
                # and send frame to IOU socket
                $iou_router_sock->send(
                    pack( "a*a*", $iou_header, $pcap_recv_data ) );
                log_iou_frame( "S:P->I",
                    pack( "a*a*", $iou_header, $pcap_recv_data ) )
                  if $debug;

            }
        }
    }
}
elsif ( defined $udp_conn ) {

    # accept localport:remotehost:remoteport, or localport:remoteport
    if ( $udp_conn =~ m/^\d+:\d+$/ ) {
        ( $udp_spt, $udp_dpt ) = split( /:/, $udp_conn );
        $udp_shost = $udp_dhost = "127.0.0.1";
    }
    elsif ( $udp_conn =~ m/^\d+:[\w\.]+:\d+$/ ) {
        ( $udp_spt, $udp_dhost, $udp_dpt ) = split( /:/, $udp_conn );
        $udp_shost = "";
    }
    else {
        die "UDP port format doesnt match";
    }

    print "Working in UDP mode.\n" if $verbose;

    # bind to udp port
    $udp_listener = IO::Socket::INET->new(
        Proto     => "udp",
        LocalPort => $udp_spt,
        LocalAddr => $udp_shost,
        PeerPort  => $udp_dpt,
        PeerAddr  => $udp_dhost
    ) or die "Can't bind to UDP port.\n";

    print
"Forwarding frames between UDP ports local:$udp_spt, $udp_dhost:$udp_dpt and IOU instance $iou_instance, int $iou_interface_major/$iou_interface_minor -  press ^C to exit\n";

    $select_handle->add($udp_listener);

    while (1) {
        my ($readable) =
          IO::Select->select( $select_handle, undef, undef, 0.001 );

        foreach my $socket (@$readable) {
            if ( $socket == $iou_pseudo_sock ) {

                # IOU frame received via pseudo ID socket
                $iou_pseudo_sock->recv( $iou_recv_data, 1580 );
                log_iou_frame( "R:I->U", $iou_recv_data ) if $debug;

                $iou_recv_data = unpack( "x8a*", $iou_recv_data );

                # send IOU generated frame via udp
                $udp_listener->send($iou_recv_data);
                write_pcap_dump($iou_recv_data) if $cap_dumper;
                log_frame( "S:I->U", $iou_recv_data ) if $debug;
            }
            else {
                $udp_listener->recv( $iou_recv_data, 1580 );
                write_pcap_dump($iou_recv_data) if $cap_dumper;
                log_frame( "R:U->I", $iou_recv_data ) if $debug;

                $iou_router_sock->send(
                    pack( "a*a*", $iou_header, $iou_recv_data ) );
                log_iou_frame( "S:U->I",
                    pack( "a*a*", $iou_header, $iou_recv_data ) )
                  if $debug;
            }
        }
    }
}
elsif ( defined $tap ) {

    print "Working in TAP mode.\n" if $verbose;

    # get file handle
    $tap_handle = IO::File->new( "/dev/net/tun", O_RDWR )
      or die "Cannot open /dev/net/tun";

    # make it tap (not tun)
    my $ifr = pack( 'Z16s', $tap, 0x1002 );
    ioctl $tap_handle, 0x400454ca, $ifr
      or die "Can't ioctl() on device $tap: $!";

    print
"Forwarding frames between TAP interface $tap and IOU instance $iou_instance, int $iou_interface_major/$iou_interface_minor -  press ^C to exit\n";

    $select_handle->add($tap_handle);

    while (1) {
        my ($readable) =
          IO::Select->select( $select_handle, undef, undef, 0.001 );

        foreach my $socket (@$readable) {
            if ( $socket == $iou_pseudo_sock ) {

                # IOU frame received via pseudo ID socket
                $iou_pseudo_sock->recv( $iou_recv_data, 1522 );
                log_iou_frame( "R:I->T", $iou_recv_data ) if $debug;

                $iou_recv_data = unpack( "x8a*", $iou_recv_data );

                # send IOU generated frame via udp
                $tap_handle->syswrite($iou_recv_data);

                write_pcap_dump($iou_recv_data) if $cap_dumper;
                log_frame( "S:I->T", $iou_recv_data ) if $debug;
            }
            else {
                $tap_handle->sysread( $iou_recv_data, 1522 );
                write_pcap_dump($iou_recv_data) if $cap_dumper;
                log_frame( "R:T->I", $iou_recv_data ) if $debug;

                $iou_router_sock->send(
                    pack( "a*a*", $iou_header, $iou_recv_data ) );
                log_iou_frame( "S:T->I",
                    pack( "a*a*", $iou_header, $iou_recv_data ) )
                  if $debug;
            }
        }
    }

}
else {

    # catchall, we really shouldnt land here
    print "No valid mode of operation selected.\n\n$help";
    caught_sigint();
}

sub caught_sigint {
    print "\n...stopped.\n";
    print "Cleaning up.\n";
    $select_handle->remove( $select_handle->handles );

    if ( defined $pcap ) {
        Net::Pcap::breakloop($pcap);
        Net::Pcap::close($pcap);
        print "Closed pcap receiver loop.\n" if $verbose;
    }
    if ( defined $udp_listener ) {
        $udp_listener->close;
        print "Closed udp listener.\n" if $verbose;
    }
    if ($tap_handle) {
        $tap_handle->close;
        print "Closed tap handle.\n" if $verbose;
    }
    if ($cap_handle) {
        Net::Pcap::pcap_dump_flush($cap_dumper);
        Net::Pcap::pcap_dump_close($cap_dumper);
        print "Closed dump file.\n" if $verbose;
    }

    $iou_pseudo_sock->close;
    $iou_router_sock->close;

    exit(0);
}

sub log_frame {
    my ( $direction, $frame ) = @_;
    return if ( length($frame) < 14 );

    # Print direction, source mac, destination mac and ethertype
    print "$direction                      S ",
      join( ":", unpack( "x6(H2)6", $frame ) ),
      " D ",
      join( ":", unpack( "(H2)6", $frame ) ),
      " T ",
      unpack( "x12H4", $frame ),
      "\n";
}

sub log_iou_frame {
    my ( $direction, $frame ) = @_;
    return if ( length($frame) < 22 );

    # Print direction, IOU header, source mac, destination mac and ethertype
    print "$direction IOU ",
      unpack( "H16", $frame ),
      " S ",
      join( ":", unpack( "x14(H2)6", $frame ) ),
      " D ",
      join( ":", unpack( "x8(H2)6", $frame ) ),
      " T ",
      unpack( "x20H4", $frame ),
      "\n";
}

sub write_pcap_dump {
    my $frame = shift @_;
    my %header;
    $header{len} = $header{caplen} = length($frame);
    ( $header{tv_sec}, $header{tv_usec} ) = gettimeofday();
    Net::Pcap::pcap_dump( $cap_dumper, \%header, $frame );
    Net::Pcap::pcap_dump_flush($cap_dumper);
}
