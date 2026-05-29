/* <!-- copyright */
/*
 * aria2 - The high speed download utility
 *
 * Copyright (C) 2006 Tatsuhiro Tsujikawa
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 * In addition, as a special exception, the copyright holders give
 * permission to link the code of portions of this program with the
 * OpenSSL library under certain conditions as described in each
 * individual source file, and distribute linked combinations
 * including the two.
 * You must obey the GNU General Public License in all respects
 * for all of the code used other than OpenSSL.  If you modify
 * file(s) with this exception, you may extend this exception to your
 * version of the file(s), but you are not obligated to do so.  If you
 * do not wish to do so, delete this exception statement from your
 * version.  If you delete this exception statement from all source
 * files in the program, then also delete it here.
 */
/* copyright --> */

// clang-format off

#define TEXT_DIR                                                        \
  _(" -d, --dir=DIR                The directory to store the downloaded file.")
#define TEXT_OUT                                                        \
  _(" -o, --out=FILE               The file name of the downloaded file. It is\n" \
    "                              always relative to the directory given in -d\n" \
    "                              option. When the -Z option is used, this option\n" \
    "                              will be ignored.")
#define TEXT_LOG_FILE                                                   \
  _(" --log-file=PATH|auto|off     Set the rotating log file path. 'auto' uses\n" \
    "                              the platform state/log directory. Default: off.")
#define TEXT_DAEMON                                                     \
  _(" -D, --daemon[=true|false]    Run as daemon. The current working directory will\n" \
    "                              be changed to \"/\" and standard input, standard\n" \
    "                              output and standard error will be redirected to\n" \
    "                              \"/dev/null\".")
#define TEXT_SPLIT                                                      \
  _(" -s, --split=N                Download a file using N connections. If more\n" \
    "                              than N URIs are given, first N URIs are used and\n" \
    "                              remaining URLs are used for backup. If less than\n" \
    "                              N URIs are given, those URLs are used more than\n" \
    "                              once so that N connections total are made\n" \
    "                              simultaneously. The number of connections to the\n" \
    "                              same host is restricted by the \n"        \
    "                              --max-connection-per-server option. See also the\n" \
    "                              --min-split-size option.")
#define TEXT_RETRY_WAIT                                                 \
  _(" --retry-wait=SEC             Set the seconds to wait between retries. \n" \
    "                              With SEC > 0, Aria2 Next will retry the download when the\n" \
    "                              HTTP server returns 503 response.")
#define TEXT_TIMEOUT                                            \
  _(" -t, --timeout=SEC            Set timeout in seconds.")
#define TEXT_MAX_TRIES                                                  \
  _(" -m, --max-tries=N            Set number of tries. 0 means unlimited.")
#define TEXT_HTTP_PROXY                                                 \
  _(" --http-proxy=PROXY           Use a proxy server for HTTP. To override a\n"\
    "                              previously defined proxy, use \"\".\n"   \
    "                              See also the --all-proxy option.\n"     \
    "                              This affects all http downloads.")
#define TEXT_HTTPS_PROXY                                                \
  _(" --https-proxy=PROXY          Use a proxy server for HTTPS. To override a \n"  \
    "                              previously defined proxy, use \"\".\n" \
    "                              See also the --all-proxy option.\n"     \
    "                              This affects all https downloads.")
#define TEXT_FTP_PROXY                                                  \
  _(" --ftp-proxy=PROXY            Use a proxy server for FTP. To override a \n" \
    "                              previously defined proxy, use \"\".\n" \
    "                              See also the --all-proxy option.\n"     \
    "                              This affects all ftp downloads.")
#define TEXT_ALL_PROXY                                                  \
  _(" --all-proxy=PROXY            Use a proxy server for all protocols. To override\n" \
    "                              a previously defined proxy, use \"\".\n" \
    "                              You also can override this setting and specify a\n" \
    "                              proxy server for a particular protocol using the\n" \
    "                              --http-proxy, --https-proxy and --ftp-proxy\n" \
    "                              options.\n"                          \
    "                              This affects all downloads.")
#define TEXT_HTTP_USER                                                  \
  _(" --http-user=USER             Set HTTP user. This affects all URLs.")
#define TEXT_HTTP_PASSWD                                                \
  _(" --http-passwd=PASSWD         Set HTTP password. This affects all URLs.")
#define TEXT_PROXY_METHOD                                               \
  _(" --proxy-method=METHOD        Set the method to use in proxy request.")
#define TEXT_PROXY_MODE                                                 \
  _(" --proxy-mode=MODE            Set proxy resolution mode: auto, direct or\n" \
    "                              manual. auto uses configured and environment\n" \
    "                              proxy options. direct disables proxy use.\n" \
    "                              manual only uses explicitly configured proxy\n" \
    "                              options.")
#define TEXT_REFERER                                                    \
  _(" --referer=REFERER            Set an http referrer (Referer). This affects\n" \
    "                              all http/https downloads. If \"*\" is given,\n" \
    "                              the download URI is also used as the referrer.\n" \
    "                              This may be useful when used together with\n" \
    "                              the -P option.")
#define TEXT_FTP_USER                                                   \
  _(" --ftp-user=USER              Set FTP user. This affects all URLs.")
#define TEXT_FTP_PASSWD                                                 \
  _(" --ftp-passwd=PASSWD          Set FTP password. This affects all URLs.")
#define TEXT_FTP_TYPE                                           \
  _(" --ftp-type=TYPE              Set FTP transfer type.")
#define TEXT_FTP_PASV                                                   \
  _(" -p, --ftp-pasv[=true|false]  Use the passive mode in FTP. If false is given,\n" \
    "                              the active mode will be used.")
#define TEXT_LOWEST_SPEED_LIMIT                                         \
  _(" --lowest-speed-limit=SPEED   Close connection if download speed is lower than\n" \
    "                              or equal to this value(bytes per sec).\n" \
    "                              0 means Aria2 Next does not have a lowest speed limit.\n" \
    "                              You can append K or M(1K = 1024, 1M = 1024K).\n" \
    "                              This option does not affect BitTorrent downloads.")
#define TEXT_MAX_OVERALL_DOWNLOAD_LIMIT                                 \
  _(" --max-overall-download-limit=SPEED Set max overall download speed in bytes/sec.\n" \
    "                              0 means unrestricted.\n"             \
    "                              You can append K or M(1K = 1024, 1M = 1024K).\n" \
    "                              To limit the download speed per download, use\n" \
    "                              --max-download-limit option.")
#define TEXT_MAX_DOWNLOAD_LIMIT                                         \
  _(" --max-download-limit=SPEED   Set max download speed per each download in\n" \
    "                              bytes/sec. 0 means unrestricted.\n"  \
    "                              You can append K or M(1K = 1024, 1M = 1024K).\n" \
    "                              To limit the overall download speed, use\n" \
    "                              --max-overall-download-limit option.")
#define TEXT_FILE_ALLOCATION                                            \
  _(" --file-allocation=METHOD     Specify file allocation method.\n"   \
    "                              'none' doesn't pre-allocate file space. 'prealloc'\n" \
    "                              pre-allocates file space before download begins.\n" \
    "                              This may take some time depending on the size of\n" \
    "                              the file.\n"                         \
    "                              If you are using newer file systems such as ext4\n" \
    "                              (with extents support), btrfs, xfs or NTFS\n" \
    "                              (MinGW build only), 'falloc' is your best\n"   \
    "                              choice. It allocates large(few GiB) files\n" \
    "                              almost instantly. Don't use 'falloc' with legacy\n" \
    "                              file systems such as ext3 and FAT32 because it\n" \
    "                              takes almost the same time as 'prealloc' and it\n" \
    "                              blocks Aria2 Next entirely until allocation finishes.\n" \
    "                              'falloc' may not be available if your system\n" \
    "                              doesn't have posix_fallocate() function.\n" \
    "                              'trunc' uses ftruncate() system call or\n" \
    "                              platform-specific counterpart to truncate a file\n" \
    "                              to a specified length.")
#define TEXT_NO_FILE_ALLOCATION_LIMIT                                   \
  _(" --no-file-allocation-limit=SIZE No file allocation is made for files whose\n" \
    "                              size is smaller than SIZE.\n"        \
    "                              You can append K or M(1K = 1024, 1M = 1024K).")
#define TEXT_ENABLE_DIRECT_IO                                          \
  _(" --enable-direct-io[=true|false] Enable directI/O, which lowers cpu usage while\n" \
    "                              allocating files.\n"                 \
    "                              Turn off if you encounter any error")
#define TEXT_ALLOW_OVERWRITE                                            \
  _(" --allow-overwrite[=true|false] Restart download from scratch if the\n" \
    "                              corresponding control file doesn't exist.  See\n" \
    "                              also --auto-file-renaming option.")
#define TEXT_ALLOW_PIECE_LENGTH_CHANGE                                  \
  _(" --allow-piece-length-change[=true|false] If false is given, Aria2 Next aborts\n" \
    "                              download when a piece length is different from\n" \
    "                              one in a control file. If true is given, you can\n" \
    "                              proceed but some download progress will be lost.")
#define TEXT_FORCE_SEQUENTIAL                                           \
  _(" -Z, --force-sequential[=true|false] Fetch URIs in the command-line sequentially\n" \
    "                              and download each URI in a separate session, like\n" \
    "                              the usual command-line download utilities.")
#define TEXT_AUTO_FILE_RENAMING                                         \
  _(" --auto-file-renaming[=true|false] Rename file name if the same file already\n" \
    "                              exists. This option works only in http(s)/ftp\n" \
    "                              download.\n"                         \
    "                              The new file name has a dot and a number(1..9999)\n" \
    "                              appended after the name, but before the file\n" \
    "                              extension, if any.")
#define TEXT_PARAMETERIZED_URI                                          \
  _(" -P, --parameterized-uri[=true|false] Enable parameterized URI support.\n" \
    "                              You can specify set of parts:\n"     \
    "                              http://{sv1,sv2,sv3}/foo.iso\n"      \
    "                              Also you can specify numeric sequences with step\n" \
    "                              counter:\n"                          \
    "                              http://host/image[000-100:2].img\n"  \
    "                              A step counter can be omitted.\n"    \
    "                              If all URIs do not point to the same file, such\n" \
    "                              as the second example above, -Z option is\n" \
    "                              required.")
#define TEXT_CHECK_INTEGRITY                                            \
  _(" -V, --check-integrity[=true|false] Check file integrity by validating piece\n" \
    "                              hashes or a hash of entire file. This option has\n" \
    "                              effect only in BitTorrent downloads with\n" \
    "                              checksums or HTTP(S)/FTP downloads with\n" \
    "                              --checksum option. If piece hashes are provided,\n" \
    "                              this option can detect damaged portions of a file\n" \
    "                              and re-download them. If a hash of entire file is\n" \
    "                              provided, hash check is only done when file has\n" \
    "                              been already download. This is determined by file\n" \
    "                              length. If hash check fails, file is\n" \
    "                              re-downloaded from scratch. If both piece hashes\n" \
    "                              and a hash of entire file are provided, only\n" \
    "                              piece hashes are used.")
#define TEXT_REALTIME_CHUNK_CHECKSUM                                    \
  _(" --realtime-chunk-checksum[=true|false]  Validate chunk of data by calculating\n" \
    "                              checksum while downloading a file if chunk\n" \
    "                              checksums are provided.")
#define TEXT_CONTINUE                                                   \
  _(" -c, --continue[=true|false]  Continue downloading a partially downloaded\n" \
    "                              file. Use this option to resume a download\n" \
    "                              started by a web browser or another program\n" \
    "                              which downloads files sequentially from the\n" \
    "                              beginning. Currently this option is only\n" \
    "                              applicable to http(s)/ftp downloads.")
#define TEXT_USER_AGENT                                                 \
  _(" -U, --user-agent=USER_AGENT  Set user agent for http(s) downloads.")
#define TEXT_NO_NETRC                                           \
  _(" -n, --no-netrc[=true|false]  Disables netrc support.")
#define TEXT_NETRC_PATH                                           \
  _(" --netrc-path=FILE            Specify the path to the netrc file.")
#define TEXT_INPUT_FILE                                                 \
  _(" -i, --input-file=FILE        Downloads URIs found in FILE. You can specify\n" \
    "                              multiple URIs for a single entity: separate\n" \
    "                              URIs on a single line using the TAB character.\n" \
    "                              Reads input from stdin when '-' is specified.\n" \
    "                              Additionally, options can be specified after each\n" \
    "                              line of URI. This optional line must start with\n" \
    "                              one or more white spaces and have one option per\n" \
    "                              single line. See INPUT FILE section of man page\n" \
    "                              for details. See also --deferred-input option.")
#define TEXT_MAX_CONCURRENT_DOWNLOADS                                   \
  _(" -j, --max-concurrent-downloads=N Set maximum number of parallel downloads for\n" \
    "                              every static (HTTP/FTP) URL and torrent.")
#define TEXT_LOAD_COOKIES                                               \
  _(" --load-cookies=FILE          Load cookies from FILE using libcurl.")
#define TEXT_SAVE_COOKIES                                               \
  _(" --save-cookies=FILE          Save cookies to FILE using libcurl.")
#define TEXT_SHOW_FILES                                                 \
  _(" -S, --show-files[=true|false] Print file listing of .torrent file and exit.")
#define TEXT_SELECT_FILE                                                \
  _(" --select-file=INDEX...       Set file to download by specifying its index.\n" \
    "                              You can find the file index using the\n" \
    "                              --show-files option. Multiple indexes can be\n" \
    "                              specified by using ',', for example: \"3,6\".\n" \
    "                              You can also use '-' to specify a range: \"1-5\".\n" \
    "                              ',' and '-' can be used together.")
#define TEXT_TORRENT_FILE                                               \
  _(" -T, --torrent-file=TORRENT_FILE  The path to the .torrent file.")
#define TEXT_TORRENT_METADATA                                           \
  _(" --torrent-metadata=MODE    Control remote .torrent metadata URLs.\n" \
    "                              Possible values are save, start and memory.\n" \
    "                              Default: start.")
#define TEXT_LISTEN_PORT                                                \
  _(" --listen-port=PORT...        Set TCP port number for BitTorrent downloads.\n" \
    "                              Multiple ports can be specified by using ',',\n" \
    "                              for example: \"6881,6885\". You can also use '-'\n" \
    "                              to specify a range: \"6881-6999\". ',' and '-' can\n" \
    "                              be used together.")
#define TEXT_MAX_OVERALL_UPLOAD_LIMIT                                   \
  _(" --max-overall-upload-limit=SPEED Set max overall upload speed in bytes/sec.\n" \
    "                              0 means unrestricted.\n"             \
    "                              You can append K or M(1K = 1024, 1M = 1024K).\n" \
    "                              To limit the upload speed per torrent, use\n" \
    "                              --max-upload-limit option.")
#define TEXT_MAX_UPLOAD_LIMIT                                           \
  _(" -u, --max-upload-limit=SPEED Set max upload speed per each torrent in\n" \
    "                              bytes/sec. 0 means unrestricted.\n"  \
    "                              You can append K or M(1K = 1024, 1M = 1024K).\n" \
    "                              To limit the overall upload speed, use\n" \
    "                              --max-overall-upload-limit option.")
#define TEXT_SEED_TIME                                                  \
  _(" --seed-time=MINUTES          Specify seeding time in (fractional) minutes.\n" \
    "                              Also see the --seed-ratio option.")
#define TEXT_SEED_RATIO                                                 \
  _(" --seed-ratio=RATIO           Specify share ratio. Seed completed torrents\n" \
    "                              until share ratio reaches RATIO.\n"  \
    "                              You are strongly encouraged to specify equals or\n" \
    "                              more than 1.0 here. Specify 0.0 if you intend to\n" \
    "                              do seeding regardless of share ratio.\n" \
    "                              If --seed-time option is specified along with\n" \
    "                              this option, seeding ends when at least one of\n" \
    "                              the conditions is satisfied.")
#define TEXT_ENABLE_PEER_EXCHANGE                                       \
  _(" --enable-peer-exchange[=true|false] Enable Peer Exchange extension.")
#define TEXT_ENABLE_DHT                                         \
  _(" --enable-dht[=true|false]    Enable IPv4 DHT functionality. It also enables\n" \
    "                              UDP tracker support. If a private flag is set\n" \
    "                              in a torrent, Aria2 Next does not use DHT for that\n" \
    "                              download even if ``true`` is given.")
#define TEXT_DHT_LISTEN_PORT                                            \
  _(" --dht-listen-port=PORT...    Set UDP listening port used by DHT(IPv4, IPv6)\n"   \
    "                              and UDP tracker. Multiple ports can be specified\n" \
    "                              by using ',', for example: \"6881,6885\". You can\n" \
    "                              also use '-' to specify a range: \"6881-6999\".\n" \
    "                              ',' and '-' can be used together.")
#define TEXT_DHT_ENTRY_POINT                                            \
  _(" --dht-entry-point=HOST:PORT  Set host and port as an entry point to IPv4 DHT\n" \
    "                              network.")
#define TEXT_BT_REQUIRE_CRYPTO                                          \
  _(" --bt-require-crypto[=true|false] If true is given, Aria2 Next does not accept and\n" \
    "                              establish connection with legacy BitTorrent\n" \
    "                              handshake. Thus Aria2 Next always uses Obfuscation\n" \
    "                              handshake.")
#define TEXT_BT_MAX_OPEN_FILES                                          \
  _(" --bt-max-open-files=NUM      Specify maximum number of files to open in\n" \
    "                              multi-file BitTorrent downloads globally.")
#define TEXT_BT_MAX_PEERS                                               \
  _(" --bt-max-peers=NUM           Specify the maximum number of peers per torrent.\n" \
    "                              0 means unlimited.")
#define TEXT_VERSION                                                    \
  _(" -v, --version                Print the version number and exit.")
#define TEXT_HELP                                                       \
  _(" -h, --help[=TAG|KEYWORD]     Print usage and exit.\n"             \
    "                              The help messages are classified with tags. A tag\n" \
    "                              starts with \"#\". For example, type \"--help=#http\"\n" \
    "                              to get the usage for the options tagged with\n" \
    "                              \"#http\". If non-tag word is given, print the usage\n" \
    "                              for the options whose name includes that word.")
#define TEXT_NO_CONF                                                    \
  _(" --no-conf[=true|false]       Disable loading aria2.conf file.")
#define TEXT_CONF_PATH                                                  \
  _(" --conf-path=PATH             Change the configuration file path to PATH.")
#define TEXT_STOP                                                       \
  _(" --stop=SEC                   Stop application after SEC seconds has passed.\n" \
    "                              If 0 is given, this feature is disabled.")
#define TEXT_HEADER                                                     \
  _(" --header=HEADER              Append HEADER to HTTP request header. You can use\n" \
    "                              this option repeatedly to specify more than one\n" \
    "                              header:\n"                           \
    "                              aria2-next --header=\"X-A: b78\" --header=\"X-B: 9J1\"\n" \
    "                              http://host/file")
#define TEXT_QUIET                                                      \
  _(" -q, --quiet[=true|false]     Make Aria2 Next quiet(no console output).")
#define TEXT_SUMMARY_INTERVAL                                           \
  _(" --summary-interval=SEC       Set interval to output download progress summary.\n" \
    "                              Setting 0 suppresses the output.")
#define TEXT_LOG_LEVEL                                                  \
  _(" --log-level=LEVEL            Set log level for console and file output.")
#define TEXT_LOG_MAX_SIZE                                               \
  _(" --log-max-size=SIZE          Set maximum rotating log file size.")
#define TEXT_LOG_MAX_FILES                                              \
  _(" --log-max-files=NUM          Set number of rotating log files to keep.")
#define TEXT_REMOTE_TIME                                                \
  _(" -R, --remote-time[=true|false] Retrieve timestamp of the remote file from the\n" \
    "                              remote HTTP/FTP server and if it is available,\n" \
    "                              apply it to the local file.")
#define TEXT_CONNECT_TIMEOUT                                            \
  _(" --connect-timeout=SEC        Set the connect timeout in seconds to establish\n" \
    "                              connection to HTTP/FTP/proxy server. After the\n" \
    "                              connection is established, this option makes no\n" \
    "                              effect and --timeout option is used instead.")
#define TEXT_MAX_FILE_NOT_FOUND                                         \
  _(" --max-file-not-found=NUM     If Aria2 Next receives `file not found' status from the\n" \
    "                              remote HTTP/FTP servers NUM times without getting\n" \
    "                              a single byte, then force the download to fail.\n" \
    "                              Specify 0 to disable this option.\n" \
    "                              This options is effective only when using\n" \
    "                              HTTP/FTP servers. The number of retry attempt is\n" \
    "                              counted toward --max-tries, so it should be\n" \
    "                              configured too.")
#define TEXT_ED2K_SERVER                                                \
  _(" --ed2k-server=HOST:PORT[,..] Use ED2K servers to discover file sources.")
#define TEXT_ED2K_SERVER_LIST                                           \
  _(" --ed2k-server-list=FILE      Load ED2K servers from a local server.met file.")
#define TEXT_ED2K_NODE_LIST                                             \
  _(" --ed2k-node-list=FILE        Load ED2K Kad bootstrap nodes from a local nodes.dat file.")
#define TEXT_ED2K_LISTEN_PORT                                           \
  _(" --ed2k-listen-port=PORT     Set TCP port number for incoming ED2K peer connections.")
#define TEXT_ED2K_UDP_LISTEN_PORT                                       \
  _(" --ed2k-udp-listen-port=PORT Set UDP port number for ED2K Kad and peer reask packets.")
#define TEXT_ED2K_UPLOAD_SLOTS                                          \
  _(" --ed2k-upload-slots=NUM     Set maximum active ED2K upload slots.")
#define TEXT_ED2K_SHARE_FILE                                            \
  _(" --ed2k-share-file=FILE      Import a completed local file into the ED2K shared store.")
#define TEXT_AUTO_SAVE_INTERVAL                                         \
  _(" --auto-save-interval=SEC     Save a control file(*.aria2) every SEC seconds.\n" \
    "                              If 0 is given, a control file is not saved during\n" \
    "                              download. Aria2 Next saves a control file when it stops\n" \
    "                              regardless of the value.")
#define TEXT_CERTIFICATE                                                \
  _(" --certificate=FILE           Use the client certificate in FILE.\n" \
    "                              The certificate must be in PEM format.\n" \
    "                              You may use --private-key option to specify the\n" \
    "                              private key.")
#define TEXT_PRIVATE_KEY                                                \
  _(" --private-key=FILE           Use the private key in FILE.\n"      \
    "                              The private key must be decrypted and in PEM\n" \
    "                              format. See also --certificate option.")
#define TEXT_CA_CERTIFICATE                                             \
  _(" --ca-certificate=FILE        Use the certificate authorities in FILE to verify\n" \
    "                              the peers. The certificate file must be in PEM\n" \
    "                              format and can contain multiple CA certificates.\n" \
    "                              Use --check-certificate option to enable\n" \
    "                              verification.")
#define TEXT_CHECK_CERTIFICATE                                          \
  _(" --check-certificate[=true|false] Verify the peer using certificates specified\n" \
    "                              in --ca-certificate option.")
#define TEXT_NO_PROXY                                                   \
  _(" --no-proxy=DOMAINS           Specify comma separated hostnames, domains or\n" \
    "                              network address with or without CIDR block where\n" \
    "                              proxy should not be used.")
#define TEXT_USE_HEAD                                                   \
  _(" --use-head[=true|false]      Use HEAD method for the first request to the HTTP\n" \
    "                              server.")
#define TEXT_NO_WANT_DIGEST_HEADER                                      \
  _(" --no-want-digest-header[=true|false] Whether to disable Want-Digest header \n" \
    "                              when doing requests.")
#define TEXT_CONTENT_DISPOSITION_DEFAULT_UTF8                          \
  _(" --content-disposition-default-utf8[=true|false] Handle quoted string in\n" \
    "                              Content-Disposition header as UTF-8 instead of\n" \
    "                              ISO-8859-1, for example, the filename parameter,\n" \
    "                              but not the extended version filename*.")
#define TEXT_EVENT_POLL                                                 \
  _(" --event-poll=POLL            Specify the method for polling events.")
#define TEXT_INDEX_OUT                                                  \
  _(" -O, --index-out=INDEX=PATH   Set file path for file with index=INDEX. You can\n" \
    "                              find the file index using the --show-files option.\n" \
    "                              PATH is a relative path to the path specified in\n" \
    "                              --dir option. You can use this option multiple\n" \
    "                              times.")
#define TEXT_DRY_RUN                                                    \
  _(" --dry-run[=true|false]       If true is given, Aria2 Next just checks whether the\n" \
    "                              remote file is available and doesn't download\n" \
    "                              data. This option has effect on HTTP/FTP download.\n" \
    "                              BitTorrent downloads are canceled if true is\n" \
    "                              specified.")
#define TEXT_ON_DOWNLOAD_COMPLETE                                       \
  _(" --on-download-complete=COMMAND Set the command to be executed after download\n" \
    "                              completed.\n"                        \
    "                              See --on-download-start option for the\n" \
    "                              requirement of COMMAND.\n"           \
    "                              See also --on-download-stop option.")
#define TEXT_ON_DOWNLOAD_START                                          \
  _(" --on-download-start=COMMAND  Set the command to be executed after download\n" \
    "                              got started. Aria2 Next passes 3 arguments to COMMAND:\n" \
    "                              GID, the number of files and file path. See Event\n" \
    "                              Hook in man page for more details.")
#define TEXT_ON_DOWNLOAD_PAUSE                                          \
  _(" --on-download-pause=COMMAND  Set the command to be executed after download\n" \
    "                              was paused.\n"\
    "                              See --on-download-start option for the\n" \
    "                              requirement of COMMAND.")
#define TEXT_ON_DOWNLOAD_ERROR                                          \
  _(" --on-download-error=COMMAND  Set the command to be executed after download\n" \
    "                              aborted due to error.\n"              \
    "                              See --on-download-start option for the\n" \
    "                              requirement of COMMAND.\n"           \
    "                              See also --on-download-stop option.")
#define TEXT_ON_DOWNLOAD_STOP                                           \
  _(" --on-download-stop=COMMAND   Set the command to be executed after download\n" \
    "                              stopped. You can override the command to be\n" \
    "                              executed for particular download result using\n" \
    "                              --on-download-complete and --on-download-error. If\n" \
    "                              they are specified, command specified in this\n" \
    "                              option is not executed.\n"           \
    "                              See --on-download-start option for the\n" \
    "                              requirement of COMMAND.")
#define TEXT_BT_STOP_TIMEOUT                                            \
  _(" --bt-stop-timeout=SEC        Stop BitTorrent download if download speed is 0 in\n" \
    "                              consecutive SEC seconds. If 0 is given, this\n" \
    "                              feature is disabled.")
#define TEXT_INTERFACE                                                  \
  _(" --interface=INTERFACE        Bind sockets to given interface. You can specify\n" \
    "                              interface name, IP address and hostname.")
#define TEXT_MULTIPLE_INTERFACE                                         \
  _(" --multiple-interface=INTERFACES Comma separated list of interfaces to bind\n" \
    "                              sockets to. Requests will be split among the\n" \
    "                              interfaces to achieve link aggregation. You can\n" \
    "                              specify interface name, IP address and hostname.\n" \
    "                              If --interface is used, this option will be\n" \
    "                              ignored.")
#define TEXT_DISABLE_IPV6                               \
  _(" --disable-ipv6[=true|false]  Disable IPv6.")
#define TEXT_HTTP_NO_CACHE                      \
  _(" --http-no-cache[=true|false] Send Cache-Control: no-cache and Pragma: no-cache\n" \
    "                              header to avoid cached content.  If false is\n" \
    "                              given, these headers are not sent and you can add\n" \
    "                              Cache-Control header with a directive you like\n" \
    "                              using --header option.")
#define TEXT_HUMAN_READABLE                     \
  _(" --human-readable[=true|false] Print sizes and speed in human readable format\n" \
    "                              (e.g., 1.2Ki, 3.4Mi) in the console readout.")
#define TEXT_BT_ENABLE_LPD                      \
  _(" --bt-enable-lpd[=true|false] Enable Local Peer Discovery.")
#define TEXT_ALL_PROXY_USER                                             \
  _(" --all-proxy-user=USER        Set user for --all-proxy.")
#define TEXT_ALL_PROXY_PASSWD                                           \
  _(" --all-proxy-passwd=PASSWD    Set password for --all-proxy.")
#define TEXT_HTTP_PROXY_USER                                            \
  _(" --http-proxy-user=USER       Set user for --http-proxy.")
#define TEXT_HTTP_PROXY_PASSWD                                          \
  _(" --http-proxy-passwd=PASSWD   Set password for --http-proxy.")
#define TEXT_HTTPS_PROXY_USER                                           \
  _(" --https-proxy-user=USER      Set user for --https-proxy.")
#define TEXT_HTTPS_PROXY_PASSWD                                         \
  _(" --https-proxy-passwd=PASSWD  Set password for --https-proxy.")
#define TEXT_FTP_PROXY_USER                                             \
  _(" --ftp-proxy-user=USER        Set user for --ftp-proxy.")
#define TEXT_FTP_PROXY_PASSWD                                           \
  _(" --ftp-proxy-passwd=PASSWD    Set password for --ftp-proxy.")
#define TEXT_REMOVE_CONTROL_FILE                \
  _(" --remove-control-file[=true|false] Remove control file before download. Using\n" \
    "                              with --allow-overwrite=true, download always\n" \
    "                              starts from scratch. This will be useful for\n" \
    "                              users behind proxy server which disables resume.")
#define TEXT_ALWAYS_RESUME                      \
  _(" --always-resume[=true|false] Always resume download. If true is given, Aria2 Next\n" \
    "                              always tries to resume download and if resume is\n" \
    "                              not possible, aborts download. If false is given,\n" \
    "                              when all given URIs do not support resume or\n" \
    "                              Aria2 Next encounters N URIs which does not support\n" \
    "                              resume (N is the value specified using\n"   \
    "                              --max-resume-failure-tries option), Aria2 Next\n" \
    "                              downloads file from scratch.\n"       \
    "                              See --max-resume-failure-tries option.")
#define TEXT_MAX_RESUME_FAILURE_TRIES                                   \
  _(" --max-resume-failure-tries=N When used with --always-resume=false, Aria2 Next\n" \
    "                              downloads file from scratch when Aria2 Next detects N\n" \
    "                              number of URIs that does not support resume. If N\n" \
    "                              is 0, Aria2 Next downloads file from scratch when all\n" \
    "                              given URIs do not support resume.\n" \
    "                              See --always-resume option.")
#define TEXT_HTTP_ACCEPT_GZIP                   \
  _(" --http-accept-gzip[=true|false] Send 'Accept-Encoding: deflate, gzip' request\n" \
    "                              header and inflate response if remote server\n" \
    "                              responds with 'Content-Encoding: gzip' or\n"  \
    "                              'Content-Encoding: deflate'.")
#define TEXT_SAVE_SESSION                       \
  _(" --save-session=FILE          Save error/unfinished downloads to FILE on exit.\n" \
    "                              You can pass this output file to aria2-next with -i\n" \
    "                              option on restart. Please note that downloads\n" \
    "                              added by aria2.addTorrent RPC method and whose\n" \
    "                              metadata could not be saved\n" \
    "                              as a file will not be saved. Downloads removed\n" \
    "                              using aria2.remove and aria2.forceRemove will not\n" \
    "                              be saved.")
#define TEXT_MAX_CONNECTION_PER_SERVER          \
  _(" -x, --max-connection-per-server=NUM The maximum number of connections to one\n" \
    "                              server for each download.")
#define TEXT_MIN_SPLIT_SIZE                     \
  _(" -k, --min-split-size=SIZE    Aria2 Next does not split less than 2*SIZE byte range.\n" \
    "                              For example, let's consider downloading 20MiB\n" \
    "                              file. If SIZE is 10M, Aria2 Next can split file into 2\n" \
    "                              range [0-10MiB) and [10MiB-20MiB) and download it\n" \
    "                              using 2 sources(if --split >= 2, of course).\n" \
    "                              If SIZE is 15M, since 2*15M > 20MiB, Aria2 Next does\n" \
    "                              not split file and download it using 1 source.\n" \
    "                              You can append K or M(1K = 1024, 1M = 1024K).")
#define TEXT_CONDITIONAL_GET                    \
  _(" --conditional-get[=true|false] Download file only when the local file is older\n" \
    "                              than remote file. Currently, this function has\n" \
    "                              many limitations. See man page for details.")
#define TEXT_ON_BT_DOWNLOAD_COMPLETE            \
  _(" --on-bt-download-complete=COMMAND For BitTorrent, a command specified in\n" \
    "                              --on-download-complete is called after download\n" \
    "                              completed and seeding is over. On the other hand,\n" \
    "                              this option sets the command to be executed after\n" \
    "                              download completed but before seeding.\n" \
    "                              See --on-download-start option for the\n" \
    "                              requirement of COMMAND.")
#define TEXT_BT_TRACKER                                                 \
  _(" --bt-tracker=URI[,...]       Comma separated list of additional BitTorrent\n" \
    "                              tracker's announce URI. These URIs are not\n" \
    "                              affected by --bt-exclude-tracker option because\n" \
    "                              they are added after URIs in --bt-exclude-tracker\n" \
    "                              option are removed.")
#define TEXT_BT_EXCLUDE_TRACKER                                         \
  _(" --bt-exclude-tracker=URI[,...] Comma separated list of BitTorrent tracker's\n" \
    "                              announce URI to remove. You can use special value\n" \
    "                              '*' which matches all URIs, thus removes all\n" \
    "                              announce URIs. When specifying '*' in shell\n" \
    "                              command-line, don't forget to escape or quote it.\n" \
    "                              See also --bt-tracker option.")
#define TEXT_MAX_DOWNLOAD_RESULT                \
  _(" --max-download-result=NUM    Set maximum number of download result kept in\n" \
    "                              memory. The download results are completed/error/\n" \
    "                              removed downloads. The download results are stored\n" \
    "                              in FIFO queue and it can store at most NUM\n" \
    "                              download results. When queue is full and new\n" \
    "                              download result is created, oldest download result\n" \
    "                              is removed from the front of the queue and new one\n" \
    "                              is pushed to the back. Setting big number in this\n" \
    "                              option may result high memory consumption after\n" \
    "                              thousands of downloads. Specifying 0 means no\n" \
    "                              download result is kept. Note that unfinished\n" \
    "                              downloads are kept in memory regardless of this\n" \
    "                              option value. See\n" \
    "                              --keep-unfinished-download-result option.")
#define TEXT_ENABLE_RPC                                               \
  _(" --enable-rpc[=true|false]    Enable JSON-RPC server.\n" \
    "                              It is strongly recommended to set secret\n" \
    "                              authorization token using --rpc-secret option.\n" \
    "                              See also --rpc-listen-port option.")
#define TEXT_RPC_MAX_REQUEST_SIZE                                   \
  _(" --rpc-max-request-size=SIZE  Set max size of JSON-RPC request. If Aria2 Next\n" \
    "                              detects the request is more than SIZE bytes, it\n" \
    "                              drops connection.")
#define TEXT_RPC_LISTEN_ALL                                         \
  _(" --rpc-listen-all[=true|false] Listen incoming JSON-RPC requests on all\n" \
    "                              network interfaces. If false is given, listen only\n" \
    "                              on local loopback interface.")
#define TEXT_RPC_LISTEN_PORT                                        \
  _(" --rpc-listen-port=PORT       Specify a port number for JSON-RPC server\n" \
    "                              to listen to.")
#define TEXT_SHOW_CONSOLE_READOUT                                       \
  _(" --show-console-readout[=true|false] Show console readout.")
#define TEXT_STREAM_PIECE_SELECTOR              \
  _(" --stream-piece-selector=SELECTOR Specify piece selection algorithm\n" \
    "                              used in HTTP/FTP download. Piece means fixed\n" \
    "                              length segment which is downloaded in parallel\n" \
    "                              in segmented download. If 'default' is given,\n" \
    "                              Aria2 Next selects piece so that it reduces the\n" \
    "                              number of establishing connection. This is\n" \
    "                              reasonable default behaviour because\n" \
    "                              establishing connection is an expensive\n" \
    "                              operation.\n"                        \
    "                              If 'inorder' is given, Aria2 Next selects piece which\n" \
    "                              has minimum index. Index=0 means first of the\n" \
    "                              file. This will be useful to view movie while\n" \
    "                              downloading it. Please note that Aria2 Next honors\n"     \
    "                              --min-split-size option, so it will be necessary\n" \
    "                              to specify a reasonable value to\n"  \
    "                              --min-split-size option.\n"          \
    "                              If 'random' is given, Aria2 Next selects piece\n" \
    "                              randomly. Like 'inorder', --min-split-size\n" \
    "                              option is honored.\n"                \
    "                              If 'geom' is given, at the beginning Aria2 Next\n" \
    "                              selects piece which has minimum index like\n" \
    "                              'inorder', but it exponentially increasingly\n" \
    "                              keeps space from previously selected piece. This\n" \
    "                              will reduce the number of establishing connection\n" \
    "                              and at the same time it will download the\n" \
    "                              beginning part of the file first. This will be\n" \
    "                              useful to view movie while downloading it.")
#define TEXT_TRUNCATE_CONSOLE_READOUT                                   \
  _(" --truncate-console-readout[=true|false] Truncate console readout to fit in\n"\
    "                              a single line.")
#define TEXT_PAUSE                              \
  _(" --pause[=true|false]         Pause download after added. This option is\n" \
    "                              effective only when --enable-rpc=true is given.")
#define TEXT_RPC_ALLOW_ORIGIN_ALL                                       \
  _(" --rpc-allow-origin-all[=true|false] Add Access-Control-Allow-Origin header\n" \
    "                              field with value '*' to the RPC response.")
#define TEXT_DOWNLOAD_RESULT                    \
  _(" --download-result=OPT        This option changes the way \"Download Results\"\n" \
    "                              is formatted. If OPT is 'default', print GID,\n" \
    "                              status, average download speed and path/URI. If\n" \
    "                              multiple files are involved, path/URI of first\n" \
    "                              requested file is printed and remaining ones are\n" \
    "                              omitted.\n"                          \
    "                              If OPT is 'full', print GID, status, average\n" \
    "                              download speed, percentage of progress and\n" \
    "                              path/URI. The percentage of progress and\n" \
    "                              path/URI are printed for each requested file in\n" \
    "                              each row.\n" \
    "                              If OPT is 'hide', \"Download Results\" is hidden.")
#define TEXT_HASH_CHECK_ONLY                    \
  _(" --hash-check-only[=true|false] If true is given, after hash check using\n" \
    "                              --check-integrity option, abort download whether\n" \
    "                              or not download is complete.")
#define TEXT_CHECKSUM                                                   \
  _(" --checksum=TYPE=DIGEST       Set checksum. TYPE is hash type. The supported\n" \
    "                              hash type is listed in \"Hash Algorithms\" in\n" \
    "                              \"aria2-next -v\". DIGEST is hex digest.\n" \
    "                              For example, setting sha-1 digest looks like\n" \
    "                              this:\n"                             \
    "                              sha-1=0192ba11326fe2298c8cb4de616f4d4140213838\n" \
    "                              This option applies only to HTTP(S)/FTP\n" \
    "                              downloads.")
#define TEXT_PIECE_LENGTH                       \
  _(" --piece-length=LENGTH        Set a piece length for HTTP/FTP downloads. This\n" \
    "                              is the boundary when Aria2 Next splits a file. All\n" \
    "                              splits occur at multiple of this length. This\n" \
    "                              option will be ignored in BitTorrent downloads.")
#define TEXT_STOP_WITH_PROCESS                                          \
  _(" --stop-with-process=PID      Stop application when process PID is not running.\n" \
    "                              This is useful if the Aria2 Next process is forked from a\n" \
    "                              parent process. The parent process can fork Aria2 Next\n" \
    "                              with its own pid and when parent process exits\n" \
    "                              for some reason, Aria2 Next can detect it and shutdown\n" \
    "                              itself.")
#define TEXT_DEFERRED_INPUT                     \
  _(" --deferred-input[=true|false] If true is given, Aria2 Next does not read all URIs\n" \
    "                              and options from file specified by -i option at\n" \
    "                              startup, but it reads one by one when it needs\n" \
    "                              later. This may reduce memory usage if input\n" \
    "                              file contains a lot of URIs to download.\n" \
    "                              If false is given, Aria2 Next reads all URIs and\n" \
    "                              options at startup.")
#define TEXT_ENABLE_MMAP                        \
  _(" --enable-mmap[=true|false]   Map files into memory.")
#define TEXT_RPC_SAVE_UPLOAD_METADATA                                   \
  _(" --rpc-save-upload-metadata[=true|false] Save the uploaded torrent\n" \
    "                              metadata in the directory specified\n" \
    "                              by --dir option. The filename consists of\n" \
    "                              SHA-1 hash hex string of metadata plus\n" \
    "                              extension. For torrent, the extension is\n" \
    "                              '.torrent'.\n" \
    "                              If false is given to this option, the\n" \
    "                              downloads added by aria2.addTorrent will not\n" \
    "                              be saved by\n" \
    "                              --save-session option.")
#define TEXT_FORCE_SAVE                         \
  _(" --force-save[=true|false]    Save download with --save-session option even\n" \
    "                              if the download is completed or removed. This\n" \
    "                              option also saves control file in that\n" \
    "                              situations. This may be useful to save\n" \
    "                              BitTorrent seeding which is recognized as\n" \
    "                              completed state.")
#define TEXT_SAVE_NOT_FOUND                         \
  _(" --save-not-found[=true|false] Save download with --save-session option even\n" \
    "                              if the file was not found on the server. This\n" \
    "                              option also saves control file in that\n" \
    "                              situations.")
#define TEXT_DISK_CACHE                         \
  _(" --disk-cache=SIZE            Enable disk cache. If SIZE is 0, the disk cache\n" \
    "                              is disabled. This feature caches the downloaded\n" \
    "                              data in memory, which grows to at most SIZE\n" \
    "                              bytes. The cache storage is created for the Aria2 Next\n" \
    "                              instance and shared by all downloads. The one\n" \
    "                              advantage of the disk cache is reduce the disk\n" \
    "                              I/O because the data are written in larger unit\n" \
    "                              and it is reordered by the offset of the file.\n" \
    "                              If hash checking is involved and the data are\n" \
    "                              cached in memory, we don't need to read them\n" \
    "                              from the disk.\n"                    \
    "                              SIZE can include K or M(1K = 1024, 1M = 1024K).")
#define TEXT_GID                                \
  _(" --gid=GID                    Set GID manually. Aria2 Next identifies each\n" \
    "                              download by the ID called GID. The GID must be\n" \
    "                              hex string of 16 characters, thus [0-9a-fA-F]\n" \
    "                              are allowed and leading zeros must not be\n" \
    "                              stripped. The GID all 0 is reserved and must\n" \
    "                              not be used. The GID must be unique, otherwise\n" \
    "                              error is reported and the download is not added.\n" \
    "                              This option is useful when restoring the\n" \
    "                              sessions saved using --save-session option. If\n" \
    "                              this option is not used, new GID is generated\n" \
    "                              by Aria2 Next.")
#define TEXT_SAVE_SESSION_INTERVAL                                      \
  _(" --save-session-interval=SEC  Save error/unfinished downloads to a file\n" \
    "                              specified by --save-session option every SEC\n" \
    "                              seconds. If 0 is given, file will be saved only\n" \
    "                              when Aria2 Next exits.")
#define TEXT_ENABLE_COLOR                                               \
  _(" --enable-color[=true|false]  Enable color output for a terminal.")
#define TEXT_RPC_SECRET                                                 \
  _(" --rpc-secret=TOKEN           Set RPC secret authorization token.")
#define TEXT_DSCP                                                       \
  _(" --dscp=DSCP                  Set DSCP value in outgoing IP packets of\n" \
    "                              BitTorrent traffic for QoS. This parameter sets\n" \
    "                              only DSCP bits in TOS field of IP packets,\n" \
    "                              not the whole field. If you take values\n" \
    "                              from /usr/include/netinet/ip.h divide them by 4\n" \
    "                              (otherwise values would be incorrect, e.g. your\n" \
    "                              CS1 class would turn into CS4). If you take\n" \
    "                              commonly used values from RFC, network vendors'\n" \
    "                              documentation, Wikipedia or any other source,\n" \
    "                              use them as they are.")
#define TEXT_RLIMIT_NOFILE                                              \
  _(" --rlimit-nofile=NUM          Set the soft limit of open file descriptors.\n" \
    "                              This open will only have effect when:\n" \
    "                                a) The system supports it (posix)\n" \
    "                                b) The limit does not exceed the hard limit.\n" \
    "                                c) The specified limit is larger than the\n" \
    "                                   current soft limit.\n" \
    "                              This is equivalent to setting nofile via ulimit,\n" \
    "                              except that it will never decrease the limit.")
#define TEXT_PAUSE_METADATA                  \
  _(" --pause-metadata[=true|false]\n"       \
    "                              Pause downloads created as a result of metadata\n" \
    "                              download. Remote .torrent metadata and magnet\n" \
    "                              metadata create subsequent downloads from the\n" \
    "                              fetched metadata.\n" \
    "                              These metadata downloads will generate downloads\n" \
    "                              using their metadata. This option pauses these\n" \
    "                              subsequent downloads. This option is effective\n" \
    "                              only when --enable-rpc=true is given.")
#define TEXT_BT_DETACH_SEED_ONLY                \
  _(" --bt-detach-seed-only[=true|false]\n"     \
    "                              Exclude seed only downloads when counting\n" \
    "                              concurrent active downloads (See -j option).\n" \
    "                              This means that if -j3 is given and this option\n" \
    "                              is turned on and 3 downloads are active and one\n" \
    "                              of those enters seed mode, then it is excluded\n" \
    "                              from active download count (thus it becomes 2),\n" \
    "                              and the next download waiting in queue gets\n" \
    "                              started. But be aware that seeding item is still\n" \
    "                              recognized as active download in RPC method.")
#define TEXT_MIN_TLS_VERSION                                            \
  _(" --min-tls-version=VERSION    Specify minimum SSL/TLS version to enable.")
#define TEXT_BT_FORCE_ENCRYPTION                                        \
  _(" --bt-force-encryption[=true|false]\n"                             \
    "                              Requires BitTorrent message payload encryption\n" \
    "                              and requires the encrypted libtorrent peer\n" \
    "                              handshake.\n" \
    "                              If true is given, deny legacy BitTorrent\n" \
    "                              handshake and only use Obfuscation handshake and\n" \
    "                              always encrypt message payload.")
#define TEXT_SOCKET_RECV_BUFFER_SIZE                                    \
  _(" --socket-recv-buffer-size=SIZE\n"                                 \
    "                              Set the maximum socket receive buffer in bytes.\n" \
    "                              Specifying 0 will disable this option. This value\n" \
    "                              will be set to socket file descriptor using\n" \
    "                              SO_RCVBUF socket option with setsockopt() call.")
#define TEXT_MAX_MMAP_LIMIT                                             \
  _(" --max-mmap-limit=SIZE        Set the maximum file size to enable mmap (see\n" \
    "                              --enable-mmap option). The file size is\n" \
    "                              determined by the sum of all files contained in\n" \
    "                              one download. For example, if a download\n" \
    "                              contains 5 files, then file size is the total\n" \
    "                              size of those files. If file size is strictly\n" \
    "                              greater than the size specified in this option,\n" \
    "                              mmap will be disabled.")
#define TEXT_STDERR \
  _(" --stderr[=true|false]        Redirect all console output that would be\n" \
    "                              otherwise printed in stdout to stderr.")
#define TEXT_KEEP_UNFINISHED_DOWNLOAD_RESULT \
  _(" --keep-unfinished-download-result[=true|false]\n" \
    "                              Keep unfinished download results even if doing\n" \
    "                              so exceeds --max-download-result. This is useful\n" \
    "                              if all unfinished downloads must be saved in\n" \
    "                              session file (see --save-session option). Please\n" \
    "                              keep in mind that there is no upper bound to the\n" \
    "                              number of unfinished download result to keep. If\n" \
    "                              that is undesirable, turn this option off.")

// clang-format on
