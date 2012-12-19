tlsdate: secure parasitic rdate replacement
===========================================

> tlsdate sets the local clock by securely connecting with TLS to remote
> servers and extracting the remote time out of the secure handshake. Unlike
> ntpdate, tlsdate uses TCP, for instance connecting to a remote HTTPS or TLS
> enabled service, and provides some protection against adversaries that try to
> feed you malicious time information.

On Debian GNU/Linux and related systems, we provide an init.d script that
controls the tlsdated daemon. It will notice network changes and regularly
invoke tlsdate to keep the clock in sync. Start it like so:

    /etc/init.d/tlsdate start

Here is an example an unprivileged user fetching the remote time:

    % tlsdate -v -V -n
    V: tlsdate version 0.0.1
    V: We were called with the following arguments:
    V: validate SSL certificates host = www.ptb.de:443
    V: time is currently 1342197117.577381
    V: using TLSv1_client_method()
    V: SSL certificate verification passed
    V: server time 1342197117 (difference is about 0 s) was fetched in 705 ms
    Fri Jul 13 18:31:57 CEST 2012


This is an example run - starting as root and dropping to nobody:

    % sudo ./tlsdate -v
    V: tlsdate version 0.0.1
    V: We were called with the following arguments:
    V: validate SSL certificates host = www.ptb.de:443
    V: time is currently 1342197222.273552
    V: using TLSv1_client_method()
    V: SSL certificate verification passed
    V: server time 1342197222 (difference is about 0 s) was fetched in 520 ms
    V: setting time succeeded

Here is an example with a custom host and custom port without verification:

    % sudo tlsdate -v --skip-verification -p 80 -H rgnx.net
    V: tlsdate version 0.0.1
    V: We were called with the following arguments:
    V: disable SSL certificate check host = rgnx.net:80
    WARNING: Skipping certificate verification!
    V: time is currently 1342197285.298607
    V: using TLSv1_client_method()
    V: Certificate verification skipped!
    V: server time 1342197286 (difference is about -1 s) was fetched in 765 ms
    V: setting time succeeded

Here is an example of a false ticker that is detected and rejected:

    % sudo tlsdate -v -H facebook.com
    V: tlsdate version 0.0.1
    V: We were called with the following arguments:
    V: validate SSL certificates host = facebook.com:443
    V: time is currently 1342197379.931852
    V: using TLSv1_client_method()
    V: SSL certificate verification passed
    V: server time 2693501503 (difference is about -1351304124 s) was fetched in 724 ms
    remote server is a false ticker from the future!

Here is an example where a system may not have any kind of RTC at boot. Do the
time warp to restore sanity and do so with a leap of faith:

    % sudo tlsdate -v -V -l -t
    V: tlsdate version 0.0.1
    V: We were called with the following arguments:
    V: validate SSL certificates host = www.ptb.de:443
    V: RECENT_COMPILE_DATE is 1342407042.000000
    V: time is currently 1342488229.659967
    V: time is greater than RECENT_COMPILE_DATE
    V: using TLSv1_client_method()
    V: freezing time for x509 verification
    V: remote peer provided: 1342488230, prefered over compile time: 1342407042
    V: freezing time with X509_VERIFY_PARAM_set_time
    V: SSL certificate verification passed
    V: server time 1342488230 (difference is about -1 s) was fetched in 791 ms
    Mon Jul 16 18:23:50 PDT 2012
    V: setting time succeeded

