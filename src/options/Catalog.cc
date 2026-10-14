/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#include "options/Options.h"

#include <algorithm>
#include <array>

namespace aria2 {
namespace {

using enum OptionType;
using namespace scope;

constexpr unsigned TG = Task | Global;
constexpr unsigned TLG = Task | Live | Global;

// clang-format off
constexpr std::array catalog{
  // Basic
  OptionSpec{"dir", 'd', Path, ".", TG, "basic", "Directory to store downloaded files."},
  OptionSpec{"out", 'o', Path, "", Task, "basic", "File name of the downloaded file, relative to --dir."},
  OptionSpec{"input-file", 'i', Path, "", 0, "basic", "Read URIs and per-task options from FILE ('-' for stdin)."},
  OptionSpec{"max-concurrent-downloads", 'j', Integer, "5", Global, "basic", "Maximum number of parallel downloads.", 1, 1000},
  OptionSpec{"continue", 'c', Flag, "false", TG, "basic", "Continue a partially downloaded file that this engine did not start."},
  OptionSpec{"check-integrity", 'V', Flag, "false", TG, "basic", "Verify existing data with piece hashes or --checksum."},
  OptionSpec{"log", 'l', Path, "", Global, "basic", "Write the log to FILE ('-' for stdout)."},
  OptionSpec{"quiet", 'q', Flag, "false", 0, "basic", "Do not print to the console."},
  OptionSpec{"daemon", 'D', Flag, "false", 0, "basic", "Run in the background."},
  OptionSpec{"conf-path", 0, Path, "", 0, "basic", "Configuration file (default: $XDG_CONFIG_HOME/aria2/aria2.conf)."},
  OptionSpec{"no-conf", 0, Flag, "false", 0, "basic", "Do not load a configuration file."},
  OptionSpec{"version", 'v', Flag, "false", 0, "basic", "Print the version and exit."},
  OptionSpec{"help", 'h', Text, "#basic", 0, "basic", "Print help for a tag (#basic, #http, #bittorrent, #ed2k, #media, #rpc, #advanced, #all) or a keyword."},

  // HTTP, HTTPS and SFTP
  OptionSpec{"split", 's', Integer, "16", TG, "http", "Connections used for one download.", 1, 256},
  OptionSpec{"max-connection-per-server", 'x', Integer, "16", TG, "http", "Connections to one host for one download.", 1, 256},
  OptionSpec{"min-split-size", 'k', Size, "1M", TG, "http", "Smallest range that is split for another connection.", 16 << 10, 1 << 30},
  OptionSpec{"timeout", 't', Integer, "60", TG, "http", "Seconds without received data before a connection is retried.", 1, 600},
  OptionSpec{"connect-timeout", 0, Integer, "60", TG, "http", "Seconds allowed for connecting.", 1, 600},
  OptionSpec{"max-tries", 'm', Integer, "5", TG, "http", "Consecutive failures without progress before the task fails (0: unlimited).", 0, INT32_MAX},
  OptionSpec{"retry-wait", 0, Integer, "0", TG, "http", "Seconds to wait before retrying (0: automatic back-off).", 0, 600},
  OptionSpec{"lowest-speed-limit", 0, Size, "0", TG, "http", "Retry a connection slower than this many bytes/s (0: off).", 0},
  OptionSpec{"max-download-limit", 0, Size, "0", TLG, "http", "Download speed limit per task in bytes/s (0: unlimited).", 0},
  OptionSpec{"max-overall-download-limit", 0, Size, "0", Global, "http", "Total download speed limit in bytes/s (0: unlimited).", 0},
  OptionSpec{"user-agent", 'U', Text, "aria2-next", TG, "http", "User-Agent request header."},
  OptionSpec{"referer", 0, Text, "", TG, "http", "Referer request header ('*' uses the download URI)."},
  OptionSpec{"header", 0, List, "", TG, "http", "Additional request header; repeatable."},
  OptionSpec{"http-user", 0, Text, "", TG | Secret, "http", "HTTP user name."},
  OptionSpec{"http-passwd", 0, Text, "", TG | Secret, "http", "HTTP password."},
  OptionSpec{"http-no-cache", 0, Flag, "false", TG, "http", "Send Cache-Control: no-cache."},
  OptionSpec{"http-accept-gzip", 0, Flag, "true", TG, "http", "Accept and decode gzip/deflate responses."},
  OptionSpec{"enable-http-keep-alive", 0, Flag, "true", TG, "http", "Reuse connections."},
  OptionSpec{"load-cookies", 0, Path, "", TG, "http", "Load cookies from a Netscape/Mozilla text or Firefox/Chrome SQLite file."},
  OptionSpec{"save-cookies", 0, Path, "", TG, "http", "Save cookies to FILE in Netscape format."},
  OptionSpec{"check-certificate", 0, Flag, "true", TG, "http", "Verify TLS certificates."},
  OptionSpec{"ca-certificate", 0, Path, "", TG, "http", "Certificate authorities file (PEM)."},
  OptionSpec{"certificate", 0, Path, "", TG, "http", "Client certificate (PEM)."},
  OptionSpec{"private-key", 0, Path, "", TG, "http", "Private key for --certificate or SFTP."},
  OptionSpec{"min-tls-version", 0, Choice, "TLSv1.2", TG, "http", "Minimum TLS version.", 0, 0, "TLSv1.1|TLSv1.2|TLSv1.3"},
  OptionSpec{"all-proxy", 0, Text, "", TG, "http", "Proxy for all protocols: [http://][USER:PASS@]HOST[:PORT], socks5://..."},
  OptionSpec{"all-proxy-user", 0, Text, "", TG | Secret, "http", "User for --all-proxy."},
  OptionSpec{"all-proxy-passwd", 0, Text, "", TG | Secret, "http", "Password for --all-proxy."},
  OptionSpec{"http-proxy", 0, Text, "", TG, "http", "Proxy for HTTP."},
  OptionSpec{"http-proxy-user", 0, Text, "", TG | Secret, "http", "User for --http-proxy."},
  OptionSpec{"http-proxy-passwd", 0, Text, "", TG | Secret, "http", "Password for --http-proxy."},
  OptionSpec{"https-proxy", 0, Text, "", TG, "http", "Proxy for HTTPS."},
  OptionSpec{"https-proxy-user", 0, Text, "", TG | Secret, "http", "User for --https-proxy."},
  OptionSpec{"https-proxy-passwd", 0, Text, "", TG | Secret, "http", "Password for --https-proxy."},
  OptionSpec{"no-proxy", 0, Text, "", TG, "http", "Comma-separated hosts, domains, networks or *.globs that bypass the proxy."},
  OptionSpec{"dns-over-https", 0, Text, "", TG, "http", "Resolve host names through this DNS-over-HTTPS URL."},
  OptionSpec{"interface", 0, Text, "", TG, "http", "Bind connections to an interface, IP address or host name."},
  OptionSpec{"disable-ipv6", 0, Flag, "false", TG, "http", "Do not use IPv6."},
  OptionSpec{"netrc-path", 0, Path, "", TG, "http", "netrc file (default: ~/.netrc)."},
  OptionSpec{"no-netrc", 'n', Flag, "false", TG, "http", "Ignore netrc."},
  OptionSpec{"sftp-user", 0, Text, "", TG | Secret, "http", "SFTP user name."},
  OptionSpec{"sftp-passwd", 0, Text, "", TG | Secret, "http", "SFTP password."},
  OptionSpec{"ssh-host-key-sha256", 0, Text, "", TG, "http", "Expected base64 SHA-256 fingerprint of the SFTP host key."},
  OptionSpec{"remote-time", 'R', Flag, "false", TG, "http", "Apply the server's Last-Modified time to the file."},
  OptionSpec{"checksum", 0, Text, "", Task, "http", "Expected checksum TYPE=DIGEST (sha-1, sha-224, sha-256, sha-384, sha-512, md5)."},
  OptionSpec{"max-file-not-found", 0, Integer, "0", TG, "http", "Fail after this many 404 responses (0: use --max-tries).", 0, INT32_MAX},
  OptionSpec{"filename-hint", 0, Text, "", Task, "http", "Name suggested by the caller."},
  OptionSpec{"filename-hint-source", 0, Choice, "suggested", Task, "http", "Origin of --filename-hint; browser names win over Content-Disposition.", 0, 0, "browser|title|suggested"},
  OptionSpec{"dry-run", 0, Flag, "false", TG, "http", "Check availability without downloading."},

  // Output files
  OptionSpec{"allow-overwrite", 0, Flag, "false", TG, "file", "Restart from scratch when the output file exists."},
  OptionSpec{"auto-file-renaming", 0, Flag, "true", TG, "file", "Use a numbered name when the output file exists."},
  OptionSpec{"file-allocation", 'a', Choice, "none", TG, "file", "none/trunc: sparse file; prealloc/falloc: reserve its space before downloading.", 0, 0, "none|prealloc|falloc|trunc"},
  OptionSpec{"remove-control-file", 0, Flag, "false", TG, "file", "Ignore stored progress and start over."},
  OptionSpec{"force-save", 0, Flag, "false", TLG, "file", "Save the task in the session even when it is finished."},
  OptionSpec{"save-not-found", 0, Flag, "true", TLG, "file", "Save tasks that failed with 404 in the session."},
  OptionSpec{"index-out", 'O', List, "", Task, "file", "INDEX=PATH output path for a torrent file; repeatable."},

  // BitTorrent
  OptionSpec{"torrent-file", 'T', Path, "", 0, "bittorrent", "Torrent file to download."},
  OptionSpec{"follow-torrent", 0, Choice, "true", TG, "bittorrent", "Start downloaded .torrent files as tasks (mem: without saving them).", 0, 0, "true|mem|false"},
  OptionSpec{"select-file", 0, Text, "", Task | Live, "bittorrent", "Indexes of files to download, e.g. 1-5,8."},
  OptionSpec{"show-files", 'S', Flag, "false", 0, "bittorrent", "Print the files of a torrent or metalink and exit."},
  OptionSpec{"pause-metadata", 0, Flag, "false", TG, "bittorrent", "Pause magnet tasks after metadata until files are selected."},
  OptionSpec{"listen-port", 0, Integer, "6881", Global, "bittorrent", "Port for TCP and uTP peers.", 1, 65535},
  OptionSpec{"enable-dht", 0, Flag, "true", TG, "bittorrent", "Use the DHT."},
  OptionSpec{"enable-peer-exchange", 0, Flag, "true", TG, "bittorrent", "Use peer exchange."},
  OptionSpec{"bt-enable-lpd", 0, Flag, "true", TG, "bittorrent", "Use local peer discovery."},
  OptionSpec{"bt-port-mapping", 0, Flag, "true", Global, "bittorrent", "Map listening ports with UPnP and NAT-PMP."},
  OptionSpec{"bt-encryption", 0, Choice, "preferred", TG, "bittorrent", "Peer encryption policy.", 0, 0, "preferred|required|disabled"},
  OptionSpec{"bt-transport", 0, Choice, "both", TG, "bittorrent", "Peer transports.", 0, 0, "tcp|utp|both"},
  OptionSpec{"bt-tracker", 0, Text, "", TG, "bittorrent", "Additional comma-separated tracker URIs."},
  OptionSpec{"bt-exclude-tracker", 0, Text, "", TG, "bittorrent", "Comma-separated tracker URIs to remove, or *."},
  OptionSpec{"bt-max-peers", 0, Integer, "100", TLG, "bittorrent", "Peer connections per torrent (0: unlimited).", 0, 65535},
  OptionSpec{"bt-max-connections", 0, Integer, "500", Global, "bittorrent", "Peer connections in total.", 2, 65535},
  OptionSpec{"bt-max-uploads", 0, Integer, "20", Global, "bittorrent", "Upload slots in total.", 1, 65535},
  OptionSpec{"bt-max-uploads-per-torrent", 0, Integer, "4", TLG, "bittorrent", "Upload slots per torrent.", 1, 65535},
  OptionSpec{"max-upload-limit", 'u', Size, "0", TLG, "bittorrent", "Upload speed limit per task in bytes/s (0: unlimited).", 0},
  OptionSpec{"max-overall-upload-limit", 0, Size, "0", Global, "bittorrent", "Total upload speed limit in bytes/s (0: unlimited).", 0},
  OptionSpec{"seed-ratio", 0, Number, "1.0", TLG, "bittorrent", "Stop seeding at this share ratio (0: no ratio limit)."},
  OptionSpec{"seed-time", 0, Number, "", TLG, "bittorrent", "Stop seeding after this many minutes."},
  OptionSpec{"bt-seed-unverified", 0, Flag, "false", TG, "bittorrent", "Seed existing files without hash checking."},
  OptionSpec{"bt-super-seeding", 0, Flag, "false", TLG, "bittorrent", "Use super seeding."},
  OptionSpec{"bt-first-last-piece-first", 0, Flag, "false", TLG, "bittorrent", "Download the first and last piece of each file first."},
  OptionSpec{"bt-file-priority", 0, Text, "", Task | Live, "bittorrent", "INDEX=off|normal|high|top,... file priorities."},
  OptionSpec{"force-sequential", 'Z', Flag, "false", TLG, "bittorrent", "Command line: one task per URI. BitTorrent: download pieces in order."},
  OptionSpec{"hash-check-only", 0, Flag, "false", TG, "bittorrent", "Check data and stop."},
  OptionSpec{"detach-share-only", 0, Flag, "false", TG, "bittorrent", "Seeding does not count toward --max-concurrent-downloads."},
  OptionSpec{"bt-external-ip", 0, Text, "", Global, "bittorrent", "External IP address reported to trackers."},
  OptionSpec{"bt-external-port", 0, Integer, "0", Global, "bittorrent", "External port reported to trackers (0: listening port).", 0, 65535},
  OptionSpec{"bt-interface", 0, Text, "", Global, "bittorrent", "Comma-separated interfaces or addresses for peers."},
  OptionSpec{"bt-proxy", 0, Text, "", Global, "bittorrent", "Proxy for peers and trackers: http://, socks4://, socks5://."},
  OptionSpec{"bt-anonymous-mode", 0, Flag, "false", Global, "bittorrent", "Hide client identity from peers and trackers."},
  OptionSpec{"bt-user-agent", 0, Text, "qBittorrent/5.2.3", Global, "bittorrent", "User agent sent to trackers."},
  OptionSpec{"bt-peer-id-prefix", 0, Text, "-qB5230-", Global, "bittorrent", "Peer ID prefix."},
  OptionSpec{"bt-peer-blocklist", 0, Path, "", Global, "bittorrent", "Blocklist of peer IP ranges (eMule DAT, P2P or CIDR)."},
  OptionSpec{"bt-blocklist-scope", 0, Choice, "peers", Global, "bittorrent", "Connections checked against the blocklist.", 0, 0, "peers|peers-and-trackers|all"},
  OptionSpec{"bt-rate-limit-overhead", 0, Flag, "false", Global, "bittorrent", "Count protocol overhead against speed limits."},
  OptionSpec{"bt-dht-bootstrap-nodes", 0, Text, "dht.libtorrent.org:25401,dht.transmissionbt.com:6881,router.bittorrent.com:6881,router.utorrent.com:6881", Global, "bittorrent", "DHT bootstrap nodes."},

  // Metalink
  OptionSpec{"metalink-file", 'M', Path, "", 0, "metalink", "Metalink file to download."},
  OptionSpec{"follow-metalink", 0, Choice, "true", TG, "metalink", "Start downloaded .metalink files as tasks.", 0, 0, "true|mem|false"},

  // ED2K
  OptionSpec{"ed2k-server", 0, Text, "", TG, "ed2k", "Comma-separated HOST:PORT servers."},
  OptionSpec{"ed2k-server-list", 0, Path, "", TG, "ed2k", "server.met file or a list of HOST:PORT lines."},
  OptionSpec{"ed2k-node-list", 0, Path, "", TG, "ed2k", "nodes.dat file to bootstrap Kad."},
  OptionSpec{"ed2k-listen-port", 0, Integer, "4662", Global, "ed2k", "TCP port for clients (0: any).", 0, 65535},
  OptionSpec{"ed2k-udp-listen-port", 0, Integer, "4672", Global, "ed2k", "UDP port for Kad and clients (0: any).", 0, 65535},
  OptionSpec{"ed2k-port-mapping", 0, Flag, "true", Global, "ed2k", "Forward the ED2K ports with UPnP."},
  OptionSpec{"ed2k-upload-slots", 0, Integer, "3", Global, "ed2k", "Upload slots in total.", 1, 1000},
  OptionSpec{"ed2k-max-connections", 0, Integer, "100", Global, "ed2k", "Client connections in total.", 1, 4096},
  OptionSpec{"ed2k-preview-priority", 0, Flag, "false", TLG, "ed2k", "Download the first and last parts first."},

  // Media
  OptionSpec{"media", 0, Choice, "auto", TG, "media", "auto detects HLS/DASH; file downloads the raw resource.", 0, 0, "auto|file|hls|dash|collection"},
  OptionSpec{"media-format", 0, Choice, "mp4", TG, "media", "Output container.", 0, 0, "mp4|mkv|vtt"},
  OptionSpec{"media-video", 0, Text, "best", TG, "media", "Video track: best, none, a language or a track ID from media.tracks."},
  OptionSpec{"media-audio", 0, Text, "best", TG, "media", "Audio track: best, none, a language or a track ID from media.tracks."},
  OptionSpec{"media-subtitles", 0, Text, "none", TG, "media", "Subtitle tracks: best, none, a language or a track ID from media.tracks."},
  OptionSpec{"media-pause-after-probe", 0, Flag, "false", TG, "media", "Pause after the presentation is read so tracks can be chosen."},
  OptionSpec{"media-record-time", 0, Integer, "0", TG, "media", "Stop a live recording after this many seconds (0: until the end).", 0, 31536000},
  OptionSpec{"media-start-time", 0, Integer, "0", Task, "media", "Start of the downloaded range in seconds (0: the beginning).", 0, 31536000},
  OptionSpec{"media-end-time", 0, Integer, "0", Task, "media", "End of the downloaded range in seconds (0: the end).", 0, 31536000},
  OptionSpec{"media-request-contexts", 0, Text, "", Task | Secret, "media", "JSON request headers per origin."},
  OptionSpec{"media-input", 0, Text, "", Task | Secret, "media", "JSON manifests captured by the browser."},

  // RPC
  OptionSpec{"enable-rpc", 0, Flag, "false", 0, "rpc", "Start the JSON-RPC server."},
  OptionSpec{"rpc-listen-port", 0, Integer, "6800", 0, "rpc", "RPC port.", 1024, 65535},
  OptionSpec{"rpc-listen-all", 0, Flag, "false", 0, "rpc", "Listen on all interfaces instead of loopback."},
  OptionSpec{"rpc-allow-origin-all", 0, Flag, "false", 0, "rpc", "Send Access-Control-Allow-Origin: *."},
  OptionSpec{"rpc-secret", 0, Text, "", Secret, "rpc", "RPC token secret."},
  OptionSpec{"rpc-max-request-size", 0, Size, "2M", 0, "rpc", "Largest accepted request.", 1024},
  OptionSpec{"rpc-save-upload-metadata", 0, Flag, "true", TG, "rpc", "Keep torrents and metalinks added over RPC for the session."},
  OptionSpec{"pause", 0, Flag, "false", Task, "rpc", "Add the task paused."},
  OptionSpec{"gid", 0, Text, "", Task, "rpc", "GID of the new task (16 hex digits)."},

  // Advanced
  OptionSpec{"state-dir", 0, Path, "", Global, "advanced", "Directory for resume data and engine state."},
  OptionSpec{"save-session", 0, Path, "", Global, "advanced", "Write unfinished tasks to FILE on exit."},
  OptionSpec{"save-session-interval", 0, Integer, "0", Global, "advanced", "Also write the session every SEC seconds (0: only on exit).", 0, INT32_MAX},
  OptionSpec{"state-save-interval", 0, Integer, "10", Global, "advanced", "Checkpoint resume data every SEC seconds.", 1, 600},
  OptionSpec{"max-download-result", 0, Integer, "1000", Global, "advanced", "Finished tasks kept in memory.", 0, INT32_MAX},
  OptionSpec{"keep-unfinished-download-result", 0, Flag, "true", Global, "advanced", "Keep unfinished results even beyond --max-download-result."},
  OptionSpec{"stop", 0, Integer, "0", Global, "advanced", "Exit after SEC seconds (0: never).", 0, INT32_MAX},
  OptionSpec{"stop-with-process", 0, Integer, "0", Global, "advanced", "Exit when process PID ends.", 0, INT32_MAX},
  OptionSpec{"summary-interval", 0, Integer, "60", Global, "advanced", "Seconds between progress summaries (0: off).", 0, INT32_MAX},
  OptionSpec{"show-console-readout", 0, Flag, "true", Global, "advanced", "Show the progress line."},
  OptionSpec{"enable-color", 0, Flag, "true", Global, "advanced", "Use colors on the console."},
  OptionSpec{"log-level", 0, Choice, "debug", Global, "advanced", "File log level.", 0, 0, "debug|info|notice|warn|error"},
  OptionSpec{"console-log-level", 0, Choice, "notice", Global, "advanced", "Console log level.", 0, 0, "debug|info|notice|warn|error"},
  OptionSpec{"log-max-size", 0, Size, "10M", Global, "advanced", "Rotate the log at this size.", 1024, 1 << 30},
  OptionSpec{"log-max-files", 0, Integer, "4", Global, "advanced", "Log files kept, including the active one.", 1, 64},
  OptionSpec{"on-download-start", 0, Path, "", Global, "advanced", "Command run when a download starts."},
  OptionSpec{"on-download-pause", 0, Path, "", Global, "advanced", "Command run when a download pauses."},
  OptionSpec{"on-download-stop", 0, Path, "", Global, "advanced", "Command run when a download stops for any reason."},
  OptionSpec{"on-download-complete", 0, Path, "", Global, "advanced", "Command run when a download completes."},
  OptionSpec{"on-download-error", 0, Path, "", Global, "advanced", "Command run when a download fails."},
  OptionSpec{"on-bt-download-complete", 0, Path, "", Global, "advanced", "Command run when a torrent finishes downloading (before seeding ends)."},
};
// clang-format on

} // namespace

std::span<const OptionSpec> optionCatalog() { return catalog; }

const OptionSpec* findOption(std::string_view name)
{
  const auto found = std::find_if(
      catalog.begin(), catalog.end(),
      [name](const OptionSpec& spec) { return spec.name == name; });
  return found == catalog.end() ? nullptr : &*found;
}

const OptionSpec* findShortOption(char name)
{
  const auto found = std::find_if(
      catalog.begin(), catalog.end(),
      [name](const OptionSpec& spec) { return spec.shortName == name; });
  return found == catalog.end() ? nullptr : &*found;
}

} // namespace aria2
