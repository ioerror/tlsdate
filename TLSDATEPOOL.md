"Lets parasitically pool TLS resources into a single location!"

ntp has pool.ntp.org which currently hosts around ~3000 machines.
tlsdate has only the wild internet's pool of TLS/SSL machines.

It is believed that there are around ~185,000 reasonable SSL/TLS servers in the
genepool that is the internet.

To discover the relevant systems in the genepool we will conduct scans and
collect data of SSL/TLS services for the entire internet. When a server is
discovered and it is confirmed to have a reasonably accurate clock, we will
store it in the genepool list.

The genepool list will first be a text file included with tlsdate and tlsdate
will have an option to use the local genepool; it will randomly select an entry
from the list and use it for timing information.

The genepool list will be in the following CSV format:

    hostname,port,last known IP address, protocol

Currently, the default protocol is TLSv1 unless otherwise specified. Fields may
include sslv2, sslv3, tlsv1, tlsv1.1, tlsv1.2, xmpp, pop3, imap and other
STARTTLS enabled protocols.

Eventually, we propose that a simple DNS query interface located at
genepool.tlsdate.net should return random entries from the genepool list. It
should only host records of machines that have correct timing information in
their SSL/TLS handshakes. The data returned will optionally be a TXT record
containing a line from a regularly updated genepool cache file or an A/AAAA
record for the host.
