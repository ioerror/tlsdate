Install
=======

Building and install of tlsdate should be as easy as:

    ./autogen.sh
    ./configure
    make
    make install

Cleaning is the usual:

    make clean

To make an unsigned Debian package:

    git checkout debian-master
    make deb

To make a Debian source package:

    git checkout master
    ./autogen.sh
    ./configure && make debian_orig
    git checkout debian-master
    fakeroot debian/rules clean
    cd ../
    dpkg-source -i'.*' -b tlsdate

Example of how to build a package for Debian:

    # First build the source package above
    scp tlsdate_* dixie.torproject.org:~/src/debian-builds/
    ~/bin/sbuild-stuff tlsdate_0.0.1-1.dsc
    # Download or build the package locally
    # and sign the .changes or .dsc file
    debsign -k0xD81D840E tlsdate_0.0.1-1.dsc

Example of how to upload it (after a Debian sponsor signs off on it):

    dget http://www.example.com/tlsdate_0.0.1-1_amd64.changes
    dput tlsdate_0.0.1-1_amd64.changes
