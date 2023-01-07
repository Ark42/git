Gamer's Internet Tunnel by Ark

------------
What is GIT?
------------
GIT is a utility to link two LANs together for network gameplay by tunneling
packets over the internet via UDP, TCP, or even through a socks proxy.
It is very configurable, allowing only certain frame types, IPX packet types,
IPv4 protocols, IPX socket destinations, or TCP/UDP ports to be forwarded.
Unlike many other tunnel programs that send all packets over the internet, GIT
is very friendly to low-bandwidth users, by not forwarding NetBIOS or other
unneeded sockets by default.


---------------------
How do I install GIT?
---------------------
GIT's installer is short and straightforward, but GIT does require that the
WinPcap library is installed.  WinPcap_3_0.exe is included with GIT. You should
be prompted to install WinPcap at the end of the GIT installation if you do not
have WinPcap v3.0 or higher installed already.  There is a shortcut to the
WinPcap installer located in the Start Menu, under Program Files\GIT, in case
you need to reinstall it at any time.

As of March 9, 2004, v3.0 is the latest version of WinPcap, and should work
with Windows XP.  If you have trouble with GIT and are using a operating system
that is newer then the latest WinPcap, you can try downloading the latest
version of WinPcap from http://winpcap.polito.it/

If you experience crashes, freezes, or problems finding your packet device in
GIT, follow these instructions:
1) Reboot your computer.
2) Uninstall WinPcap from control panel.
3) Reboot your computer.
4) Install WinPcap v3.0 or higher.
5) Reboot your computer.


------
Wizard
------
New in GIT v0.99 is the configuration wizard, which allows you to answer a few
questions to easily set up GIT for a particular program.
.git files can be run and will start GIT or talk to an existing GIT to display
the wizard pages needed for that script.
Please note that the Auto Configure button for the scripts will re-use all of
the saved settings from the last time you ran that script, so if your IP address
or any remote IP addresses have changed, you will need to run through the wizard
again instead of using the Auto Configure button.
See the file "Wizard Scripts.txt" for information on creating your own scripts.
There is also a Save Profile option that will save all your current settings to
a wizard script that you can easily restore using the wizard later. The host
list can be stored as-in or the profile can create a textbox that will prompt
to edit the host list when you run the profile script.


----------------------
Quick configution tips
----------------------
These steps will work assuming no firewalls or NAT gets in the way.

For TCP or UDP client-server based games:
	Right click the GIT icon in the system tray and select Configuration.
	Select the game you wish to play under TCP/UDP ports and click Add.

	If you are the server:
		Check "Be TCP listen server and accept all connections on
		the same port from any host instead."
		Enter the maximum number of clients you expect in the
		Max connections box.

	If you are a client:
		Enter the hostname of the server in the Host or IP box.
		Select method "TCP connect" then click Add.

For IPX based games:
	Right click the GIT icon in the system tray and select Configuration.
	Select the game you wish to play under IPX sockets and click Add.
	Enter the hostname or IP of the other player in the Host or IP box.
	Select method "UDP" and click Add.

-----------------
How does it work?
-----------------
GIT puts your ethernet adaptor into promiscuous mode in order to sniff all
ethernet packets and analyze which ones are part of your network game traffic.
GIT then sends these packets to another instance of GIT running on the
opposite LAN, which will receive the packet and broadcast it onto that LAN
as if the two were physically connected.
Only one computer in each LAN needs to be running GIT for the entire LANs to
be linked together.


-------------------
How do I set it up?
-------------------
Right click on the tray icon and pick Configuration.  Add the host on the
opposite LAN that is running GIT and the port it is running on, then pick a
method.  UDP is the best choice of methods, since it is the fastest, but
in order to pierce a firewall, TCP connect may be required for the LAN behind
a firewall.  When any of the TCP connect methods are used, the opposite side
must be using TCP listen in order to accept the connection.  Please note that
TCP listen will only accept connections from the host listed, and will deny
connections on the port to any other host.  This means the hostname must be
the address of the socks proxy if a TCP connect via socks method is used.

It is also required to pick one or more IPX socket ranges, or TCP/UDP port
ranges to forward, the simplest method being to just add '0000-ffff : All
sockets' for IPX games, or '1025-9999 : Common ports' for TCP/UDP games, so
that all sockets or ports are forwarded, but if the game you plan to play is
listed, pick that game to avoid sending traffic you dont need to.
You may also log (un)forwarded packets to see which sockets or ports a game is
using. Please email me with known sockets of games if you are sure the
information is correct.


-------------------------------------
Notes on setting up hosts connections
-------------------------------------
Each host must be connecting using a different port number. For example, if
you wish to connect 3 hosts together to play an IPX based game, host A would
connect to host B on port 213 and host C on port 214.  Host B would connect to
host A on port 213 and host C on port 215. Host C would connect to host A on
port 214 and host B on port 215.  This will allow for all traffic from any
host to reach both of the other hosts without conflicts.
Note that hostnames may be entered as names or IP addresses.

New in GIT v0.96 - for games that only require a connection to a single server,
it is much easier to have the computer that will be running the game server
pick "Be TCP listen server and accept all connections on the same port from any
host instead." in the configuration window instead. This will allow all other
clients to simply connect to the server by adding 1 host connection to the
server of type TCP connect.
Also new for regular TCP listen connections, if the IP specified has 0's in it,
then the connection will be accepted from any IP in that subnet, instead of
requiring a connection from the exact IP. For example:
111.222.33.44 - requires connection from 111.222.33.44
111.222.33.0 - accepts connections from 111.222.33.*
111.222.0.0 - accepts connections from 111.222.*.*
111.0.0.0 - accepts connections from 111.*.*.*
0.0.0.0 - accepts connections from any IP


--------------------------------
Notes specific to TCP/IP and UDP
--------------------------------
Since TCP, UDP, and ICMP packets are all routable over the Internet, it is
normally not necessary to forward anything but broadcast UDP packets, which
are not routed over the Internet.  The default option under Advanced
Configuration will have "Don't Send Unicast" checked, which will make GIT only
forward packets sent to addresses like 255.255.255.255.  Some games will use a
mix of TCP and UDP, where the clients locate the game via UDP broadcast
packets, but then connect directly to the game server via TCP.  This will work
fine with GIT as long as the game server is not firewalled or using NAT.
If the game server is firewalled, you must open up the game port on the
firewall to allow incomming traffic to the server.  If the game server is
using NAT, you can adjust for this in the Advanced Configuration window by
selecting the "Alter Source IP" checkbox, and filling in the hostname or IP
address of both your internal address and external address.

For example, I have seen successful setups for Warcraft 3 under all of the
above connections at once.  The game server was both behind a firewall and
using NAT, while the clients were also behind a firewall. The firewall on
the server side was configured to allow incoming connections for port 213 (for
GIT) and port 6112 (for WC3) and was set to map its internal address of
10.1.1.2 to its real external address using GIT.  Only UDP broadcast packets
were tunneled with GIT. Once the clients located the WC3 server address,
they connected directly to it via TCP and played normally from there.

One other thing to note in the Advanced Configuration window is the types of
frames to look at.  IPv4 is almost only used in Ethernet II frames. It is
possible, but highly unlikely, to use 802.2 and SNAP as well.  Older versions
of GIT did not have Ethernet II checked by default, since IPX packets are
typically only found in 802.2 or 802.3 frames.  You may need to go in to
Advanced Configuration and check Ethernet II (as well as "Don't Send Unicast")
if you had an older version of GIT.

New in GIT v0.98 are several options described here:
Don't Send Unicast - This replaces the old option named "Only if broadcast" but
is only enforced on the sending side of the tunnel.  Previously the option could
reject packets on the receiving end of the GIT tunnel as well, but this is no
longer the case.
Don't Send Broadcast - This option compliments the previous option, and when
combined with the previous option replaces the old option named "Receive only".
This option is useful on a switched network with 2 or more copies of GIT running
where GIT is forwarding unicast packets.  Check this option on all but 1 copy
of GIT in order to prevent duplicate forwarding of broadcast packets from the
same network.
Don't Send Routable - This option prevents any packets that should be routable
over the internet from being tunneled.  The only non-routable addresses that
will still be tunneled are 192.168.x.x, 172.16-31.x.x, 10.x.x.x, and
169.254.x.x
Also Match Source Port - This option allows the TCP/UDP port lists to forward
packets if the source port matches a port in the list. Previously only the
destination ports were matched, as is still the case for IPX sockets.
This option should be checked on both sides of the tunnel if used.
Forward ARP - This allows GIT to tunnel ARP requests and replies, in case some
program requires it.
Use Old Reforward Prevention Method for IPX/IPv4 Frames - Checking this option
will resort to the method used up to GIT v0.97, which is to alter packets with
a flag in order to not reforward them.  Not checking this option will keep
track of the source MAC addresses of all packets received from the tunnel and
sent to the local network.  Each MAC address is remembered for 30 seconds in
order to prevent re-forwarding the same packet back that GIT had just created.
Up to 256 MAC addresses can be remembered at once, otherwise packets may start
getting duplicated once the table fills up.  This should be unlikely though
since it requires more than 256 unique computers to all have their packets
tunneled at once in less then 30 seconds.  ARP packets always use the MAC
address method regardless of this setting.


--------------------
Log file information
--------------------
The lines in the logfiles (located in the directory GIT is in) may look like this:

(sample from forwarded.log with 0000-ffff in socket list and Log Forwarded Packets checked)
[Sat May 26 12:03:17 2001] network: IEEE802.2 IPX to:00000000.ffffffffffff:87c2 from:00000000.00aa00112233:87c2 'ok'
[] contains the date/time
network: means the packet was sniffed from the network (incoming.log would list the host the packet was received from)
IEEE802.2 is the frame type (Win9x 'auto' is usually 802.2, Novell Netware 4 and 5 default to 802.2)
IPX means the packet was an IPX packet (usually only IPX and NetBIOS types are seen on a network)
to: lists the IPX destination address in the form network.node:socket (in this case 87c2 is a Warcraft II IPX socket)
from: lists the source of the packet
'ok' is a message stating the packet was forwarded with no problems.

(samples from unforwarded.log socket list empty and Log Unforwarded Packets checked)
[Sat May 26 12:04:46 2001] network: IEEE802.2 IPX to:00000000.ffffffffffff:87c2 from:00000000.00aa00112233:87c2 'wrong socket number'
[Sat May 26 12:04:46 2001] network: IEEE802.2 NetBIOS to:00000000.ffffffffffff:0553 from:00000000.00aa00112233:0553 'wrong packet type'
In this case, the message is 'wrong socket number' which means that GIT is configured to forward IEEE802.2 frames and IPX packets, but not socket 87c2


-------
Warning
-------
Although I have not seen it happen on any of my test machines, some ethernet
cards do not like, or do not support promiscuous mode.  Some may actually
lock up the system or simply cause GIT to not work even though GIT thinks it
is working, since it will not be receiving all network packets.

What I have seen happen, however, is the blue screen of death very frequently,
which was caused by upgrading winpcap without uninstalling a older version
first.  If you experience very frequent crashes, only when running GIT and
playing games, uninstall ALL versions of winpcap from control panel's
add/remove programs, then reboot even if it doesn't tell you to, then install
the latest version of winpcap, and reboot again, even if it doesn't tell you
to! That solved the crashes, but just uninstalling and reinstalling winpcap
without the reboots did not!

