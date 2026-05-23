Technical Notes
===============

This document describes additional technical information of aria2. The
expected audience is developers.

Control File (\*.aria2) Format
------------------------------

The control file uses a binary format to store progress information of
a download. Here is the diagram for each field:

.. code-block:: text

     0                   1                   2                   3
     0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
    +---+-------+-------+-------------------------------------------+
    |VER|  EXT  |INFO   |INFO HASH ...                              |
    |(2)|  (4)  |HASH   | (INFO HASH LENGTH)                        |
    |   |       |LENGTH |                                           |
    |   |       |  (4)  |                                           |
    +---+---+---+-------+---+---------------+-------+---------------+
    |PIECE  |TOTAL LENGTH   |UPLOAD LENGTH  |BIT-   |BITFIELD ...   |
    |LENGTH |     (8)       |     (8)       |FIELD  | (BITFIELD     |
    |  (4)  |               |               |LENGTH |  LENGTH)      |
    |       |               |               |  (4)  |               |
    +-------+-------+-------+-------+-------+-------+---------------+
    |NUM    |INDEX  |LENGTH |PIECE  |PIECE BITFIELD ...             |
    |IN-    |  (4)  |  (4)  |BIT-   | (PIECE BITFIELD LENGTH)       |
    |FLIGHT |       |       |FIELD  |                               |
    |PIECE  |       |       |LENGTH |                               |
    |  (4)  |       |       |  (4)  |                               |
    +-------+-------+-------+-------+-------------------------------+

            ^                                                       ^
            |                                                       |
            +-------------------------------------------------------+
                    Repeated in (NUM IN-FLIGHT) PIECE times

``VER`` (VERSION): 2 bytes
   Should be either version 0(0x0000) or version 1(0x0001).  In
   version 1, all multi-byte integers are saved in network byte
   order(big endian).  In version 0, all multi-byte integers are saved
   in host byte order.  aria2 1.4.1 can read both versions and only
   writes a control file in version 1 format.  version 0 support will
   be disappear in the future version.

``EXT`` (EXTENSION): 4 bytes
   If LSB is 1(i.e. ``EXT[3]&1 == 1``), aria2 checks whether the saved
   InfoHash and current downloading one are the same. If they are not
   the same, an exception is thrown. This is called "infoHashCheck"
   extension.

``INFO HASH LENGTH``: 4 bytes
   The length of InfoHash that is located after this field. If
   "infoHashCheck" extension is enabled, if this value is 0, then an
   exception is thrown. For http/ftp downloads, this value should be
   0.

``INFO HASH``: ``(INFO HASH LENGTH)`` bytes
   BitTorrent InfoHash.

``PIECE LENGTH``: 4 bytes
   The length of the piece.

``TOTAL LENGTH``: 8 bytes
   The total length of the download.

``UPLOAD LENGTH``: 8 bytes
   The uploaded length in this download.

``BITFIELD LENGTH``: 4 bytes
   The length of bitfield.

``BITFIELD``: ``(BITFIELD LENGTH)`` bytes
   This is the bitfield which represents current download progress.

``NUM IN-FLIGHT PIECE``: 4 bytes
   The number of in-flight pieces. These piece is not marked
   'downloaded' in the bitfield, but it has at least one downloaded
   chunk.

The following 4 fields are repeated in ``(NUM IN-FLIGHT PIECE)``
times.

``INDEX``: 4 bytes
   The index of the piece.

``LENGTH``: 4 bytes
   The length of the piece.

``PIECE BITFIELD LENGTH``: 4 bytes
   The length of bitfield of this piece.

``PIECE BITFIELD``: ``(PIECE BITFIELD LENGTH)`` bytes
   The bitfield of this piece. The each bit represents 16KiB chunk.

BitTorrent DHT state
--------------------

aria2-next uses libtorrent-rasterbar as its BitTorrent backend.
BitTorrent DHT routing state is owned by libtorrent and is not stored in
the removed native ``dht.dat`` or ``dht6.dat`` formats.
