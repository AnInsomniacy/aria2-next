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
HTTP(S), FTP, SFTP, BitTorrent, and ED2K file links. aria2 can download a
file from multiple sources/protocols and tries to utilize your maximum
download bandwidth. It supports downloading a file from HTTP(S)/FTP/SFTP and
BitTorrent at the same time, while the data downloaded from HTTP(S)/FTP/SFTP is
uploaded to the BitTorrent swarm.

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
* JSON-RPC over HTTP and WebSocket interface
* Run as a daemon process
* Selective download in multi-file torrents
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
HTTPS                    OpenSSL through libcurl and aria2-next
SFTP and SCP             libcurl with its selected SSH backend
BitTorrent               libtorrent-rasterbar and Boost headers
ED2K                     Native aria2-next protocol code
Checksum                 OpenSSL plus zlib for adler32 when enabled
gzip and deflate         libcurl and zlib
Async DNS                C-Ares when enabled
JSON-RPC over WebSocket  Boost.Beast and Boost.JSON
======================== ========================================

aria2-next uses OpenSSL as the only direct TLS and crypto backend. ED2K keeps
its narrow native MD4 implementation for protocol compatibility and uses
OpenSSL-backed MD5, SHA-1, and RC4 paths for remaining hash and obfuscation
needs.

You can disable BitTorrent support with ``-DARIA2_ENABLE_BITTORRENT=OFF``.

To enable async DNS support, you need c-ares.

* c-ares: http://c-ares.haxx.se/

How to build
------------

aria2 is primarily written in C++. aria2-next currently requires a C++17-aware
compiler because the modernized runtime and dependency integration use C++17.

To build aria2 from the source package, install CMake, Ninja, pkg-config, a
C++17 compiler, and the development packages for the maintained dependency set:

* libcurl-dev      (Required for HTTP, HTTPS, FTP, FTPS, SFTP, and SCP)
* libssl-dev       (Required for TLS, crypto, checksums, and ED2K obfuscation)
* libtorrent-rasterbar-dev (Required for BitTorrent support)
* libboost-dev     (Required for Boost.JSON, Boost.Beast, Boost.Asio, and BitTorrent)
* libc-ares-dev    (Required for async DNS support when enabled)
* zlib1g-dev       (Required for compression and adler32 support when enabled)
* pkg-config       (Required to detect installed libraries)
* cppunit          (Required to build the test suite)

The executable is located at ``build/default/aria2-next`` when using the default
preset.

The CMake configure step checks the maintained dependency set and fails with a
clear error if a required dependency is missing.

Since 1.1.0, aria2 checks the certificate of HTTPS servers by default. OpenSSL
builds use system CA loading first. If system lookup fails, aria2 uses a
detected CA bundle path as the backend fallback. Set
``-DARIA2_CA_BUNDLE=/path/to/ca-bundle`` to choose that fallback explicitly.

By default, the bash completion file named ``aria2-next`` is installed to the
default documentation directory. To change that directory, set
``-DARIA2_BASH_COMPLETION_DIR=/path/to/directory``.

aria2 uses CppUnit for automated unit testing. CTest runs the tests executable.

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

The maintained Android NDK baseline is recorded in
``packaging/dependencies.env``.

The maintained release workflow and Android Dockerfile are the reference Android
cross-build implementations. They assume the following libraries have been built
for cross-compilation:

* c-ares
* openssl
* zlib
* libssh2

Build the dependency libraries as static libraries and install them under a
single Android prefix. Then configure aria2 with CMake using the Android NDK
toolchain variables. The maintained Dockerfile reads the NDK baseline from
``packaging/dependencies.env`` and passes
``CMAKE_SYSTEM_NAME=Android``, ``CMAKE_ANDROID_NDK``,
``CMAKE_ANDROID_ARCH_ABI=arm64-v8a``, ``CMAKE_SYSTEM_VERSION``,
``CMAKE_PREFIX_PATH``, and ``PKG_CONFIG_LIBDIR``.

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

aria2-next uses libtorrent-rasterbar as its only BitTorrent backend.
Torrent files and magnet links are routed through libtorrent for peer
protocol, DHT, PEX, local peer discovery, UDP trackers, piece picking,
endgame behavior, disk I/O, metadata exchange, and resume state.

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

DHT and UDP tracker
~~~~~~~~~~~~~~~~~~~

libtorrent-rasterbar owns the DHT and UDP tracker implementation. Use
``--enable-dht`` and ``--dht-listen-port`` to control the DHT session
and UDP listening ports exposed through aria2-next.

Other things should be noted
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

* ``-o`` option is used to change the file name of .torrent file itself,
  not a file name of a file in .torrent file. For this purpose, use
  ``--index-out`` option instead.
* The port numbers that aria2 uses by default are 6881-6999 for TCP
  and UDP.
* aria2 doesn't configure port-forwarding automatically. Please
  configure your router or firewall manually.
* The default maximum number of peers per torrent is 55.
* As of release 0.10.0, aria2 stops sending request messages after
  selective download completes.

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
* https://github.com/AnInsomniacy/aria2-next
* `RFC 959 FILE TRANSFER PROTOCOL (FTP) <http://tools.ietf.org/html/rfc959>`_
* `RFC 1738 Uniform Resource Locators (URL) <http://tools.ietf.org/html/rfc1738>`_
* `RFC 2428 FTP Extensions for IPv6 and NATs <http://tools.ietf.org/html/rfc2428>`_
* `RFC 2616 Hypertext Transfer Protocol -- HTTP/1.1 <http://tools.ietf.org/html/rfc2616>`_
* `RFC 3659 Extensions to FTP <http://tools.ietf.org/html/rfc3659>`_
* `RFC 3986 Uniform Resource Identifier (URI): Generic Syntax <http://tools.ietf.org/html/rfc3986>`_
* `RFC 4038 Application Aspects of IPv6 Transition <http://tools.ietf.org/html/rfc4038>`_
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
* `libtorrent-rasterbar <https://www.libtorrent.org/>`_
* `BitTorrent: WebSeed - HTTP/FTP Seeding (GetRight style) <http://www.bittorrent.org/beps/bep_0019.html>`_
* `BitTorrent: Private Torrents <http://www.bittorrent.org/beps/bep_0027.html>`_
* `BitTorrent: BitTorrent DHT Extensions for IPv6 <http://www.bittorrent.org/beps/bep_0032.html>`_
* `BitTorrent: Message Stream Encryption <http://wiki.vuze.com/w/Message_Stream_Encryption>`_
* `Kademlia: A Peer-to-peer Information System Based on the  XOR Metric <https://pdos.csail.mit.edu/~petar/papers/maymounkov-kademlia-lncs.pdf>`_
