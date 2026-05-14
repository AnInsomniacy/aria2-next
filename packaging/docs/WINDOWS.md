aria2 Windows build
===================

This file is copied into Windows release packages. Development and release
automation for Windows lives under ``packaging/`` in the source tree.

aria2 Windows build is provided in 2 flavors: 32bit version and 64bit
version. The executable was compiled using mingw-w64 cross compiler on
Ubuntu Linux.

The executable is statically linked, so no extra DLLs are
necessary. Maintained dependency versions are defined in
``packaging/dependencies.env`` in the source tree. The linked libraries are:

* gmp 6.3.0
* expat 2.8.1
* sqlite 3.53.1
* zlib 1.3.2
* c-ares 1.34.6
* libssh2 1.11.1

This build has the following difference from the original release:

* 32bit version only: ``--disable-ipv6`` is enabled by default. (In
  other words, IPv6 support is disabled by default).

Known Issues
------------

* TLSv1.3 does not work.

* --file-allocation=falloc uses SetFileValidData function to allocate
    disk space without filling zero.  But it has security
    implications.  Refer to
    https://msdn.microsoft.com/en-us/library/windows/desktop/aa365544%28v=vs.85%29.aspx
    for more details.

* When Ctrl-C is pressed, aria2 shows "Shutdown sequence
  commencing... Press Ctrl-C again for emergency shutdown." But
  mingw32 build cannot handle second Ctrl-C properly. The second
  Ctrl-C just kills aria2 instantly without proper shutdown sequence
  and you may lose data. So don't press Ctrl-C twice.

* --daemon option doesn't work.

* 32bit version only: When ``--disable-ipv6=false`` is given,
  BitTorrent DHT may not work properly.

* 32bit version only: Most of the IPv6 functionality does not work
even if ``--disable-ipv6=false`` is given.

References
----------

* http://smithii.com/aria2
* http://kemovitra.blogspot.com/2009/12/download-aria2-163.html
