aria2 - The ultra fast download utility
=======================================

Aria2 Next maintenance note
---------------------------

Aria2 Next is maintained by AnInsomniacy since 2026 as the aria2-compatible
``aria2-next`` engine for Motrix Next and other consumers. Maintenance focuses on
reliability fixes, current dependency baselines, and reproducible
cross-platform releases. CMake is the only supported build system for this
repository. Ninja is the default generator used by local development and
release automation.

Disclaimer
----------
This program comes with no warranty.
You must use this program at your own risk.

Introduction
------------

aria2 is a utility for downloading files. The supported protocols are
HTTP(S), FTP, SFTP, BitTorrent, Metalink, and ED2K file links. aria2 can download a
file from multiple sources/protocols and tries to utilize your maximum
download bandwidth. It supports downloading a file from
HTTP(S)/FTP/SFTP and BitTorrent at the same time, while the data
downloaded from HTTP(S)/FTP/SFTP is uploaded to the BitTorrent
swarm. Using Metalink's chunk checksums, aria2 automatically validates
chunks of data while downloading a file like BitTorrent.

Aria2 Next includes native ED2K/eMule support reimplemented inside aria2's
existing engine architecture from authoritative eMule, aMule, MLDonkey,
Wireshark, and protocol documentation references. The reference-alignment work
is tracked in ``docs/maintenance/ed2k-refactor/``. Core ED2K/eMule behavior has
been ported where it fits aria2-next, while obsolete legacy structures were
removed or replaced with existing compatible integration surfaces.

The maintained fork is located at https://github.com/AnInsomniacy/aria2-next.
It preserves aria2 command-line, configuration, session, JSON-RPC, and
libaria2 compatibility.

See the upstream `aria2 Online Manual
<https://aria2.github.io/manual/en/html/>`_ to learn how to use aria2.

Features
--------

Here is a list of features:

* Command-line interface
* Download files through HTTP(S)/FTP/SFTP/BitTorrent/ED2K
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


Versioning
----------

Aria2 Next uses semantic versions. ``CMakeLists.txt`` is the version source of
truth, and release tags use ``v{PROJECT_VERSION}``. Official release artifacts
are built by the GitHub release workflow after a matching GitHub Release is
published.

How to get source code
----------------------

The canonical source location for this maintained fork is the Aria2 Next
repository that contains this file. The original upstream project is hosted at
https://github.com/aria2/aria2.

Clone this repository and build from the checkout root with CMake.

Dependency
----------


======================== ========================================
features                  dependency
======================== ========================================
HTTPS                    OpenSSL or Windows TLS (Schannel)
SFTP                     libssh2
BitTorrent               None (OpenSSL is used when present)
ED2K                     None
Metalink                 Expat
gzip, deflate in HTTP    zlib
Async DNS                c-ares
Firefox3/Chromium cookie libsqlite3
XML-RPC                  Expat
JSON-RPC over WebSocket  None (bundled wslay)
======================== ========================================


.. note::

  On Windows, the native Schannel-based TLS implementation (WinTLS) is
  preferred, so OpenSSL is not required on that platform. To use OpenSSL
  instead, configure CMake with ``-DARIA2_WITH_WINTLS=OFF``. On every
  other platform OpenSSL 3.0 or newer provides TLS, message digests, and
  the DH key exchange.

  Without OpenSSL (WinTLS builds), a bundled implementation provides the
  message digests and bignum arithmetic, so BitTorrent, ED2K, and
  checksum validation work on every configuration.

A user can have one of the following configurations for SSL and crypto
libraries:

* OpenSSL
* Windows TLS (Windows only)

You can disable BitTorrent and Metalink support with
``-DARIA2_ENABLE_BITTORRENT=OFF`` and ``-DARIA2_ENABLE_METALINK=OFF``.

To enable async DNS support, you need c-ares 1.34.5 or newer.

* c-ares: https://c-ares.org/

How to build
------------

aria2-next is written in C++17 with a small amount of C11; any current
GCC, Clang, or llvm-mingw toolchain works.

To build from source, you need the following development packages
(package names may vary depending on the distribution you use):

* libssl-dev       (Required for HTTPS, BitTorrent, Checksum support)
* libssh2-1-dev    (Required for SFTP support)
* libc-ares-dev    (Required for async DNS support)
* libexpat1-dev    (Required for Metalink and XML-RPC support)
* zlib1g-dev       (Required for gzip, deflate decoding support in HTTP)
* libsqlite3-dev   (Required for Firefox3/Chromium cookie support)
* pkg-config       (Required to detect installed libraries)

The unit test framework (doctest) and the WebSocket library (wslay) are
bundled under ``third_party/``, so no extra packages are needed for
tests or JSON-RPC over WebSocket.

Source builds require CMake 3.25+, Ninja, and pkg-config. Maintained
OpenSSL builds require OpenSSL 3.0 or newer.
Install the documentation toolchain if you want to build the manual and man
page::

    $ python3 -m pip install 'sphinx>=8.2,<9' 'sphinx-rtd-theme>=3.0,<4'

The quickest local build uses the default preset::

    $ cmake --preset default
    $ cmake --build --preset default
    $ ctest --preset default

A plain CMake invocation is also supported::

    $ cmake -S . -B build/default -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
    $ cmake --build build/default
    $ ctest --test-dir build/default --output-on-failure

To request static dependency metadata from pkg-config, use::

    $ cmake -S . -B build/static -G Ninja -DARIA2_STATIC_DEPENDENCIES=ON
    $ cmake --build build/static

The executable is located at ``build/default/aria2-next`` when using the default
preset.

The CMake configure step checks available libraries and enables as many
features as possible except experimental features not enabled by default.

Since 1.1.0, aria2 checks the certificate of HTTPS servers by default. OpenSSL
builds load the system-wide CA certificate store at startup; if the library
cannot locate usable system trust, provide a PEM CA file explicitly with
``--ca-certificate``.

Using WinTLS automatically uses the system certificate store, so an explicit CA
bundle is not necessary for that backend.

By default, the bash completion file named ``aria2-next`` is installed to the
default documentation directory. To change that directory, set
``-DARIA2_BASH_COMPLETION_DIR=/path/to/directory``.

Unit tests use the bundled doctest framework and always build; CTest runs the
tests executable. Run a subset with
``build/default/aria2_tests --test-case='UtilTest1.*'``.

See `Cross-compiling Windows binary`_ to create a Windows binary.
See `Cross-compiling Android binary`_ to create an Android binary.

Cross-compiling Windows binary
------------------------------

In this section, we describe how to build a Windows binary using a
mingw-w64 (http://mingw-w64.org/doku.php) cross-compiler on Debian
Linux. The MinGW (http://www.mingw.org/) may not be able to build
aria2.

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

The maintained Android NDK baseline is recorded in
``packaging/dependencies.env``.

The maintained release workflow is the reference Android cross-build
implementation. It assumes the following libraries have been built for
cross-compilation:

* c-ares
* openssl
* expat
* zlib
* libssh2

Build the dependency libraries as static libraries and install them under a
single Android prefix. Then configure aria2 with CMake using the Android NDK
toolchain variables. The maintained release workflow reads the NDK baseline
from ``packaging/dependencies.env`` and passes
``CMAKE_SYSTEM_NAME=Android``, ``CMAKE_ANDROID_NDK``,
``CMAKE_ANDROID_ARCH_ABI=arm64-v8a``, ``CMAKE_SYSTEM_VERSION``,
``CMAKE_PREFIX_PATH``, and ``PKG_CONFIG_LIBDIR``.

Official Android releases are published as bare ARM64 executable assets named
``aria2-next-<version>-android-arm64``.

Building documentation
----------------------

`Sphinx <http://sphinx-doc.org/>`_ is used to build the documentation.
Install the documentation dependencies first::

    $ python3 -m pip install 'sphinx>=8.2,<9' 'sphinx-rtd-theme>=3.0,<4'

aria2 man pages will be built when you run ``make`` if they are not
up-to-date.  You can also build an HTML version of the aria2 man page by
``make html`` from the relevant ``docs/manual/<language>`` directory.
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
    "tests.torrent", then file name is "tests.torrent.file".  The
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

The current Metalink implementation supports HTTP(S)/FTP/SFTP/BitTorrent.
Other protocols in Metalink documents are ignored. Both Metalink4 (RFC 5854) and
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
``-DARIA2_STATIC_DEPENDENCIES=ON`` CMake option as well. See libaria2
documentation to know how to use API.

References
----------

* `aria2 Online Manual <https://aria2.github.io/manual/en/html/>`_
* https://github.com/AnInsomniacy/aria2-next
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
