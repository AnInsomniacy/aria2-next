aria2 - The ultra fast download utility
=======================================

aria2-next maintenance note
---------------------------

aria2-next is a maintained aria2 fork focused on reliability fixes, current
dependency baselines, and reproducible cross-platform releases. CMake is the
only supported build system for this repository. Ninja is the default generator
used by local development and release automation.

Disclaimer
----------
This program comes with no warranty.
You must use this program at your own risk.

Introduction
------------

aria2 is a utility for downloading files. The supported protocols are
HTTP(S), FTP, SFTP, BitTorrent, and Metalink. aria2 can download a
file from multiple sources/protocols and tries to utilize your maximum
download bandwidth. It supports downloading a file from
HTTP(S)/FTP/SFTP and BitTorrent at the same time, while the data
downloaded from HTTP(S)/FTP/SFTP is uploaded to the BitTorrent
swarm. Using Metalink's chunk checksums, aria2 automatically validates
chunks of data while downloading a file like BitTorrent.

The project page is located at https://aria2.github.io/.

See the `aria2 Online Manual
<https://aria2.github.io/manual/en/html/>`_ to learn how to use aria2.

Features
--------

Here is a list of features:

* Command-line interface
* Download files through HTTP(S)/FTP/SFTP/BitTorrent
* Segmented downloading
* Metalink version 4 (RFC 5854) support(HTTP/FTP/SFTP/BitTorrent)
* Metalink version 3.0 support(HTTP/FTP/SFTP/BitTorrent)
* Metalink/HTTP (RFC 6249) support
* HTTP/1.1 implementation
* HTTP Proxy support
* HTTP BASIC authentication support
* HTTP Proxy authentication support
* Well-known environment variables for proxy: ``http_proxy``,
  ``https_proxy``, ``ftp_proxy``, ``all_proxy`` and ``no_proxy``
* HTTP gzip, deflate content encoding support
* Verify peer using given trusted CA certificate in HTTPS
* Client certificate authentication in HTTPS
* Chunked transfer encoding support
* Load Cookies from the file using the Firefox3 format, Chromium/Google Chrome
  and the Mozilla/Firefox
  (1.x/2.x)/Netscape format.
* Save Cookies in the Mozilla/Firefox (1.x/2.x)/Netscape format.
* Custom HTTP Header support
* Persistent Connections support
* FTP/SFTP through HTTP Proxy
* Download/Upload speed throttling
* BitTorrent extensions: Fast extension, DHT, PEX, MSE/PSE,
  Multi-Tracker, UDP tracker
* BitTorrent `WEB-Seeding <http://getright.com/seedtorrent.html>`_.
  aria2 requests chunk more than piece size to reduce the request
  overhead. It also supports pipelined requests with piece size.
* BitTorrent Local Peer Discovery
* Rename/change the directory structure of BitTorrent downloads
  completely
* JSON-RPC (over HTTP and WebSocket)/XML-RPC interface
* Run as a daemon process
* Selective download in multi-file torrent/Metalink
* Chunk checksum validation in Metalink
* Can disable segmented downloading in Metalink
* Netrc support
* Configuration file support
* Download URIs found in a text file or stdin and the destination
  directory and output file name can be specified optionally
* Parameterized URI support
* IPv6 support with Happy Eyeballs
* Disk cache to reduce disk activity


Versioning and release schedule
-------------------------------

We use 3 numbers for the aria2 version: MAJOR.MINOR.PATCH.  We will ship
MINOR updates on the 15th of every month.  We may skip a release if we have
had no changes since the last release.  The feature and documentation
freeze happens 10 days before the release day (the 5th day of the month)
for translation teams.  We will raise an issue about the upcoming
release around that day.

We may release PATCH releases between regular releases if we have
security issues.

The MAJOR version will stay at 1 for the time being.

How to get source code
----------------------

The canonical source location for this maintained fork is the aria2-next
repository that contains this file. The original upstream project is hosted at
https://github.com/aria2/aria2.

Clone this repository and build from the checkout root with CMake.

Dependency
----------


======================== ========================================
features                  dependency
======================== ========================================
HTTPS                    OSX or GnuTLS or OpenSSL or Windows
SFTP                     libssh2
BitTorrent               None. Optional: libnettle+libgmp or libgcrypt
                         or OpenSSL (see note)
Metalink                 libxml2 or Expat.
Checksum                 None. Optional: OSX or libnettle or libgcrypt
                         or OpenSSL or Windows (see note)
gzip, deflate in HTTP    zlib
Async DNS                C-Ares
Firefox3/Chromium cookie libsqlite3
XML-RPC                  libxml2 or Expat.
JSON-RPC over WebSocket  libnettle or libgcrypt or OpenSSL
======================== ========================================


.. note::

  libxml2 has precedence over Expat if both libraries are installed.
  If you prefer Expat, configure CMake with ``-DARIA2_WITH_LIBXML2=OFF -DARIA2_WITH_EXPAT=ON``.

.. note::

  On Apple OSX, OS-level SSL/TLS support will be preferred. Hence
  neither GnuTLS nor OpenSSL is required on that platform. If you would like to disable this behavior, configure CMake with
  ``-DARIA2_WITH_APPLETLS=OFF``.

  GnuTLS has precedence over OpenSSL if both libraries are installed.
  If you prefer OpenSSL, configure CMake with
  ``-DARIA2_WITH_GNUTLS=OFF -DARIA2_WITH_OPENSSL=ON``.

  On Windows, there is SSL implementation available that is based on
  the native Windows SSL capabilities (Schannel) and it will be
  preferred.  Hence neither GnuTLS nor OpenSSL is required on that
  platform.  If you would like to disable this behavior, configure CMake with
  ``-DARIA2_WITH_WINTLS=OFF``.

.. note::

  On Apple OSX, the OS-level checksum support will be preferred,
  unless aria2 is configured with ``-DARIA2_WITH_APPLETLS=OFF``.

  libnettle has precedence over libgcrypt if both libraries are
  installed.  If you prefer libgcrypt, configure CMake with
  ``-DARIA2_WITH_LIBNETTLE=OFF -DARIA2_WITH_LIBGCRYPT=ON``. If OpenSSL is selected over
  GnuTLS, neither libnettle nor libgcrypt will be used.

  If none of the optional dependencies are installed, an internal
  implementation that only supports md5 and sha1 will be used.

  On Windows, there is SSL implementation available that is based on
  the native Windows capabilities and it will be preferred, unless aria2 is configured with ``-DARIA2_WITH_WINTLS=OFF``.

A user can have one of the following configurations for SSL and crypto
libraries:

* OpenSSL
* GnuTLS + libgcrypt
* GnuTLS + libnettle
* Apple TLS (OSX only)
* Windows TLS (Windows only)

You can disable BitTorrent and Metalink support with
``-DARIA2_ENABLE_BITTORRENT=OFF`` and ``-DARIA2_ENABLE_METALINK=OFF``.

To enable async DNS support, you need c-ares.

* c-ares: http://c-ares.haxx.se/

How to build
------------

aria2 is primarily written in C++. Initially, it was written based on
C++98/C++03 standard features. We are now migrating aria2 to the C++11
standard. The current source code requires a C++11 aware compiler. For
well-known compilers, such as g++ and clang, the ``-std=c++11`` or
``-std=c++0x`` flag must be supported.

To build aria2 from the source package, you need the following
development packages (package name may vary depending on the
distribution you use):

* libgnutls-dev    (Required for HTTPS, BitTorrent, Checksum support)
* nettle-dev       (Required for BitTorrent, Checksum support)
* libgmp-dev       (Required for BitTorrent)
* libssh2-1-dev    (Required for SFTP support)
* libc-ares-dev    (Required for async DNS support)
* libxml2-dev      (Required for Metalink support)
* zlib1g-dev       (Required for gzip, deflate decoding support in HTTP)
* libsqlite3-dev   (Required for Firefox3/Chromium cookie support)
* pkg-config       (Required to detect installed libraries)

You can use libgcrypt-dev instead of nettle-dev and libgmp-dev:

* libgpg-error-dev (Required for BitTorrent, Checksum support)
* libgcrypt-dev    (Required for BitTorrent, Checksum support)

You can use libssl-dev instead of
libgnutls-dev, nettle-dev, libgmp-dev, libgpg-error-dev and libgcrypt-dev:

* libssl-dev       (Required for HTTPS, BitTorrent, Checksum support)

You can use libexpat1-dev instead of libxml2-dev:

* libexpat1-dev    (Required for Metalink support)

On Fedora you need the following packages: gcc, gcc-c++, kernel-devel,
libgcrypt-devel, libxml2-devel, openssl-devel, cppunit

Source builds require CMake, Ninja, pkg-config, a C++11 compiler, and the
development packages for the features you want to enable. Install the pinned
documentation toolchain if you want to build the manual and man page::

    $ python3 -m pip install -r doc/requirements.txt

The quickest local build uses the default preset::

    $ cmake --preset default
    $ cmake --build --preset default
    $ ctest --preset default

A plain CMake invocation is also supported::

    $ cmake -S . -B build/default -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
    $ cmake --build build/default
    $ ctest --test-dir build/default --output-on-failure

To request a static release-style build, use::

    $ cmake -S . -B build/static -G Ninja -DARIA2_ENABLE_STATIC=ON
    $ cmake --build build/static

The executable is located at ``build/default/aria2c`` when using the default
preset.

The CMake configure step checks available libraries and enables as many
features as possible except experimental features not enabled by default.

Since 1.1.0, aria2 checks the certificate of HTTPS servers by default. If you
build with OpenSSL or a recent GnuTLS version that has
``gnutls_certificate_set_x509_system_trust()``, and the library is properly
configured to locate the system-wide CA certificate store, aria2 loads those
certificates at startup. If not, pass the CA bundle path with
``-DARIA2_CA_BUNDLE=/path/to/ca-bundle``.

Using native AppleTLS or WinTLS automatically uses the system certificate store,
so an explicit CA bundle is not necessary for those backends.

By default, the bash completion file named ``aria2c`` is installed to the
default documentation directory. To change that directory, set
``-DARIA2_BASH_COMPLETION_DIR=/path/to/directory``.

aria2 uses CppUnit for automated unit testing. CTest runs the test executable.

See `Cross-compiling Windows binary`_ to create a Windows binary.
See `Cross-compiling Android binary`_ to create an Android binary.

Cross-compiling Windows binary
------------------------------

In this section, we describe how to build a Windows binary using a
mingw-w64 (http://mingw-w64.org/doku.php) cross-compiler on Debian
Linux. The MinGW (http://www.mingw.org/) may not be able to build
aria2.

The easiest way to build a Windows binary is using
``packaging/docker/Dockerfile.mingw``. If you cannot use Dockerfile, then
continue to read the following paragraphs.

After compiling and installing dependency libraries, cross-compile with a CMake
toolchain file or explicit ``CMAKE_SYSTEM_NAME``, compiler, prefix, and
``PKG_CONFIG_LIBDIR`` settings. The maintained release workflow is the reference
Windows cross-build implementation. It assumes the following libraries have been
built for cross-compilation:

* c-ares
* expat
* sqlite3
* zlib
* libssh2
* cppunit

Some environment variables can be adjusted to change build settings:

``HOST``
  cross-compile to build programs to run on ``HOST``. It defaults to
  ``i686-w64-mingw32``. To build a 64bit binary, specify
  ``x86_64-w64-mingw32``.

``PREFIX``
  Prefix to the directory where dependent libraries are installed.  It
  defaults to ``/usr/local/$HOST``. ``-I$PREFIX/include`` will be
  added to ``CPPFLAGS``. ``-L$PREFIX/lib`` will be added to
  ``LDFLAGS``. ``$PREFIX/lib/pkgconfig`` will be set to
  ``PKG_CONFIG_LIBDIR``.

For example, a 64-bit Windows build uses ``-DCMAKE_SYSTEM_NAME=Windows`` with
``x86_64-w64-mingw32-gcc`` and ``x86_64-w64-mingw32-g++``. If you want an
installable libaria2 build, enable ``-DARIA2_ENABLE_LIBARIA2=ON`` and prepare
matching shared or static external libraries.

Cross-compiling Android binary
------------------------------

In this section, we describe how to build Android binary using Android
NDK cross-compiler on Debian Linux.

At the time of this writing, Android NDK r29 should compile aria2
without errors.

The maintained release workflow and Android Dockerfile are the reference Android
cross-build implementations. They assume the following libraries have been built
for cross-compilation:

* c-ares
* openssl
* expat
* zlib
* libssh2

Build the dependency libraries as static libraries and install them under a
single Android prefix. Then configure aria2 with CMake using the Android NDK
toolchain variables. The maintained Dockerfile uses Android NDK r29 and passes
``CMAKE_SYSTEM_NAME=Android``, ``CMAKE_ANDROID_NDK``,
``CMAKE_ANDROID_ARCH_ABI=arm64-v8a``, ``CMAKE_SYSTEM_VERSION``,
``CMAKE_PREFIX_PATH``, and ``PKG_CONFIG_LIBDIR``.

Building documentation
----------------------

`Sphinx <http://sphinx-doc.org/>`_ is used to build the documentation.
Install the pinned documentation dependencies first::

    $ python3 -m pip install -r doc/requirements.txt

aria2 man pages will be built when you run ``make`` if they are not
up-to-date.  You can also build an HTML version of the aria2 man page by
``make html`` from the relevant ``doc/manual-src/<language>`` directory.
The HTML version manual is also available
`online <https://aria2.github.io/manual/en/html/>`_.

BitTorrent
-----------

About file names
~~~~~~~~~~~~~~~~
The file name of the downloaded file is determined as follows:

single-file mode
    If "name" key is present in .torrent file, the file name is the value
    of "name" key. Otherwise, the file name is the base name of .torrent
    file appended by ".file". For example, .torrent file is
    "test.torrent", then file name is "test.torrent.file".  The
    directory to store the downloaded file can be specified by -d
    option.

multi-file mode
    The complete directory/file structure mentioned in .torrent file
    is created.  The directory to store the top directory of
    downloaded files can be specified by -d option.

Before download starts, a complete directory structure is created if
needed. By default, aria2 opens at most 100 files mentioned in
.torrent file, and directly writes to and reads from these files.
The number of files to open simultaneously can be controlled by
``--bt-max-open-files`` option.

DHT
~~~

aria2 supports mainline compatible DHT. By default, the routing table
for IPv4 DHT is saved to ``$XDG_CACHE_HOME/aria2/dht.dat`` and the
routing table for IPv6 DHT is saved to
``$XDG_CACHE_HOME/aria2/dht6.dat`` unless files exist at
``$HOME/.aria2/dht.dat`` or ``$HOME/.aria2/dht6.dat``. aria2 uses the
same port number to listen on for both IPv4 and IPv6 DHT.

UDP tracker
~~~~~~~~~~~

UDP tracker support is enabled when IPv4 DHT is enabled.  The port
number of the UDP tracker is shared with DHT. Use ``--dht-listen-port``
option to change the port number.

Other things should be noted
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

* ``-o`` option is used to change the file name of .torrent file itself,
  not a file name of a file in .torrent file. For this purpose, use
  ``--index-out`` option instead.
* The port numbers that aria2 uses by default are 6881-6999 for TCP
  and UDP.
* aria2 doesn't configure port-forwarding automatically. Please
  configure your router or firewall manually.
* The maximum number of peers is 55. This limit may be exceeded when
  the download rate is low. This download rate can be adjusted using
  ``--bt-request-peer-speed-limit`` option.
* As of release 0.10.0, aria2 stops sending request messages after
  selective download completes.

Metalink
--------

The current implementation supports HTTP(S)/FTP/SFTP/BitTorrent.  The
other P2P protocols are ignored. Both Metalink4 (RFC 5854) and
Metalink version 3.0 documents are supported.

For checksum verification, md5, sha-1, sha-224, sha-256, sha-384, and
sha-512 are supported. If multiple hash algorithms are provided, aria2
uses a stronger one. If whole file checksum verification fails, aria2
doesn't retry the download and just exits with a non-zero return code.

The supported user preferences are version, language, location,
protocol, and os.

If chunk checksums are provided in the Metalink file, aria2 automatically
validates chunks of data during download. This behavior can be turned
off by a command-line option.

If a signature is included in a Metalink file, aria2 saves it as a file
after the completion of the download.  The file name is download
file name + ".sig". If the same file already exists, the signature file is
not saved.

In Metalink4, a multi-file torrent could appear in metalink:metaurl
element.  Since aria2 cannot download 2 same torrents at the same
time, aria2 groups files in metalink:file element which has the same
BitTorrent metaurl, and downloads them from a single BitTorrent swarm.
This is a basically multi-file torrent download with file selection, so
the adjacent files which are not in Metalink document but share the same
piece with the selected file are also created.

If relative URI is specified in metalink:url or metalink:metaurl
element, aria2 uses the URI of Metalink file as base URI to resolve
the relative URI. If relative URI is found in the Metalink file which is
read from the local disk, aria2 uses the value of ``--metalink-base-uri``
option as base URI. If this option is not specified, the relative URI
will be ignored.

Metalink/HTTP
-------------

The current implementation only uses rel=duplicate links.  aria2
understands Digest header fields and check whether it matches the
digest value from other sources. If it differs, drop the connection.
aria2 also uses this digest value to perform checksum verification
after the download is finished. aria2 recognizes geo value. To tell aria2
which location you prefer, you can use ``--metalink-location`` option.

netrc
-----

netrc support is enabled by default for HTTP(S)/FTP/SFTP.  To disable
netrc support, specify -n command-line option.  Your .netrc file
should have correct permissions(600).

WebSocket
---------

The WebSocket server embedded in aria2 implements the specification
defined in RFC 6455. The supported protocol version is 13.

libaria2
--------

The libaria2 is a C++ library that offers aria2 functionality to the
client code. Currently, libaria2 is not built by default. To enable
libaria2, use ``-DARIA2_ENABLE_LIBARIA2=ON`` CMake option.  By default,
only the shared library is built. To build a static library, use
``-DARIA2_ENABLE_STATIC=ON`` CMake option as well. See libaria2
documentation to know how to use API.

References
----------

* `aria2 Online Manual <https://aria2.github.io/manual/en/html/>`_
* https://aria2.github.io/
* `RFC 959 FILE TRANSFER PROTOCOL (FTP) <http://tools.ietf.org/html/rfc959>`_
* `RFC 1738 Uniform Resource Locators (URL) <http://tools.ietf.org/html/rfc1738>`_
* `RFC 2428 FTP Extensions for IPv6 and NATs <http://tools.ietf.org/html/rfc2428>`_
* `RFC 2616 Hypertext Transfer Protocol -- HTTP/1.1 <http://tools.ietf.org/html/rfc2616>`_
* `RFC 3659 Extensions to FTP <http://tools.ietf.org/html/rfc3659>`_
* `RFC 3986 Uniform Resource Identifier (URI): Generic Syntax <http://tools.ietf.org/html/rfc3986>`_
* `RFC 4038 Application Aspects of IPv6 Transition <http://tools.ietf.org/html/rfc4038>`_
* `RFC 5854 The Metalink Download Description Format <http://tools.ietf.org/html/rfc5854>`_
* `RFC 6249 Metalink/HTTP: Mirrors and Hashes <http://tools.ietf.org/html/rfc6249>`_
* `RFC 6265 HTTP State Management Mechanism <http://tools.ietf.org/html/rfc6265>`_
* `RFC 6266 Use of the Content-Disposition Header Field in the Hypertext Transfer Protocol (HTTP) <http://tools.ietf.org/html/rfc6266>`_
* `RFC 6455 The WebSocket Protocol <http://tools.ietf.org/html/rfc6455>`_
* `RFC 6555 Happy Eyeballs: Success with Dual-Stack Hosts <http://tools.ietf.org/html/rfc6555>`_

* `The BitTorrent Protocol Specification <http://www.bittorrent.org/beps/bep_0003.html>`_
* `BitTorrent: DHT Protocol <http://www.bittorrent.org/beps/bep_0005.html>`_
* `BitTorrent: Fast Extension <http://www.bittorrent.org/beps/bep_0006.html>`_
* `BitTorrent: IPv6 Tracker Extension <http://www.bittorrent.org/beps/bep_0007.html>`_
* `BitTorrent: Extension for Peers to Send Metadata Files <http://www.bittorrent.org/beps/bep_0009.html>`_
* `BitTorrent: Extension Protocol <http://www.bittorrent.org/beps/bep_0010.html>`_
* `BitTorrent: Multitracker Metadata Extension <http://www.bittorrent.org/beps/bep_0012.html>`_
* `BitTorrent: UDP Tracker Protocol for BitTorrent <http://www.bittorrent.org/beps/bep_0015.html>`_
  and `BitTorrent udp-tracker protocol specification <http://www.rasterbar.com/products/libtorrent/udp_tracker_protocol.html>`_.
* `BitTorrent: WebSeed - HTTP/FTP Seeding (GetRight style) <http://www.bittorrent.org/beps/bep_0019.html>`_
* `BitTorrent: Private Torrents <http://www.bittorrent.org/beps/bep_0027.html>`_
* `BitTorrent: BitTorrent DHT Extensions for IPv6 <http://www.bittorrent.org/beps/bep_0032.html>`_
* `BitTorrent: Message Stream Encryption <http://wiki.vuze.com/w/Message_Stream_Encryption>`_
* `Kademlia: A Peer-to-peer Information System Based on the  XOR Metric <https://pdos.csail.mit.edu/~petar/papers/maymounkov-kademlia-lncs.pdf>`_
