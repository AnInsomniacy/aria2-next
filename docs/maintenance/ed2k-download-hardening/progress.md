# ED2K Download Hardening Progress

This file is the compact chronological evidence trail for ED2K download
hardening. Keep entries short and checkpoint-sized. Do not paste raw logs,
packet captures, generated reports, local caches, public-network scratch data,
or conversation text.

## Current State

The completed `docs/maintenance/ed2k-refactor` tracker is the protocol-aligned
baseline. Public testing and comparison with aMule indicate the next reliability
gap is runtime behavior: source lifecycle, long-running discovery, queued-peer
reask, LowID handling, stable identity, transfer pacing, and cooperative upload
state.

## Entries

### AR00 - Tracker Activation and Baseline Audit

Changed: Activated the ED2K download hardening tracker as the next workstream
after the completed `ed2k-refactor` protocol baseline. The maintenance root
index points to `ed2k-download-hardening`, the tracker read path stays narrow,
and AR00 records baseline runtime findings without rewriting historical ED2K
completion records.

Reference evidence: Focused aMule inspection covered
`CDownloadQueue::Process`, `ProcessLocalRequests`, `SendNextUDPPacket`,
`DoKademliaFileRequest`, `CPartFile::Process`,
`CUpDownClient::IsSourceRequestAllowed`, `UDPReaskACK`, `UDPReaskFNF`,
`UDPReaskForDownload`, `CDeadSourceList`, `CClientCredits`, `CUploadQueue`,
`CSharedFileList`, and Kad search/firewall paths. The important aMule runtime
model is durable source discovery, source quality, dead-source expiry, queued
peer reask, LowID handling, transfer pacing, upload cooperation, and stable
identity.

Current-code evidence: Focused aria2-next inspection covered `Ed2kAttribute`,
`Ed2kCommand`, `Ed2kKadCommand`, `ed2k_policy`, `ed2k_peer`,
`Ed2kUploadQueue`, `Ed2kSharedResponder`, `SessionSerializer`,
`RpcMethodImpl`, and the existing ED2K tests. The current code already has
native protocol surfaces, but later checkpoints must harden identity,
source-policy state, server/Kad cadence, peer lifecycle, queued-peer reask,
LowID boundaries, transfer pacing, and status truth.

Verified: `git diff --check docs/maintenance` passed. The CSV parser check
passed for all `docs/maintenance/ed2k-download-hardening` CSV files.

Remaining: Start AR10 stable ED2K identity and runtime metadata.

### AR10 - Stable ED2K Identity and Metadata

Changed: Added a hidden `ed2k-client-hash` state value, stored it on
`Ed2kAttribute`, restored it while creating ED2K request groups, saved it
through `SessionSerializer`, and used it for ED2K server login and peer hello.
The previous duplicate hash helpers derived identity from the per-process
`DownloadEngine` session id and were removed.

Reference evidence: aMule reads `s_userhash` from `preferences.dat`, generates
and saves it if missing, marks bytes 5 and 14 as eMule-style identity bytes,
and Kad derives its client hash from the same persisted user hash. aria2-next
keeps the same stable-identity behavior in native hidden session state instead
of adding a legacy database.

Current-code evidence: `src/Ed2kAttribute.*` owns client-hash normalization,
creation, and option restore; `src/download_helper.cc` copies the identity into
request state; `src/SessionSerializer.cc` writes it back; `src/Ed2kCommand.cc`
uses it for server and peer handshakes; `src/Ed2kSharedPeerCommand.cc` uses the
process option identity for shared-peer replies.

Verified: `cmake --build --preset default --target aria2_tests` passed.
Focused CppUnit paths
`All Tests/aria2::SessionSerializerTest/aria2::SessionSerializerTest::testSaveEd2kDownload`
and
`All Tests/aria2::DownloadHelperTest/aria2::DownloadHelperTest::testCreateRequestGroupForUri_ED2KClientHash`
passed. `build/default/aria2_tests --list` now exposes exact test paths for
short checkpoint verification.

Remaining: Start AR20 source lifecycle and quality model.

### AR20 - Source Lifecycle and Quality Model

Changed: Added explicit ED2K peer lifecycle classification for useful,
connecting, queued, downloading, no-needed-parts, callback-waiting, dead,
retrying, no-file, and cancelled states. Source selection now has an optional
active-source cap while preserving existing quality ordering by fail count,
source origin, queued state, and queue rank. Dead and no-file sources expire
after their bounded retry horizon before peer scheduling, and no-needed-parts
peers are kept separate from permanent failure.

Reference evidence: aMule `CDeadSourceList` keeps dead sources only until their
timeout expires, and `CPartFile::Process` treats `DS_NONEEDEDPARTS` as a
temporary state with slower recheck and possible later source refresh rather
than a permanent failure. aMule source limits and source quality are tied to
per-file source count, queue state, and rare-file/source-origin behavior.

Current-code evidence: `src/ed2k_policy.*` now owns lifecycle classification
and capped connect selection. `src/Ed2kAttribute.*` expires dead source state
before peer scheduling and clears `outOfParts` only when a peer is queued or
accepted again. Existing `PeerState` flags remain the storage owner.

Verified: `cmake --build --preset default --target aria2_tests` passed.
Focused CppUnit paths
`DownloadHelperTest::testEd2kSourcePolicyClassifiesLifecycle`,
`DownloadHelperTest::testEd2kSourcePolicyExpiresDeadSources`,
`DownloadHelperTest::testEd2kSourcePolicyAppliesActiveCap`, and
`DownloadHelperTest::testEd2kSourcePolicyRanksSources` passed.

Remaining: Start AR30 server discovery cadence.

### AR30 - Server Discovery Cadence

Changed: Replaced the previous noisy server source polling model with bounded
server-source cadence state. TCP source requests now record an aMule-style
800 second next-request horizon after `OP_GETSOURCES`. Server UDP source
requests now use per-server timestamps, require usable UDP source capability
flags, respect large-file support, and avoid globally polling every 20 seconds.
Server source responses now record response time and source count so recently
useful servers are not immediately re-polled. The new metadata is persisted in
the ED2K server-state payload.

Reference evidence: aMule defines `SERVERREASKTIME` as 800000 ms and
`UDPSERVERREASKTIME` as 1300000 ms. `CPartFile::Process` queues local TCP
source requests only after `SERVERREASKTIME`, while `CDownloadQueue::Process`
and `SendNextUDPPacket` drive UDP source requests through a bounded server/file
queue using UDP flags and large-file support.

Current-code evidence: `src/ed2k_policy.*` owns deterministic TCP and UDP
source-request due checks. `src/Ed2kCommand.cc` marks TCP source sends and
records TCP responses. `src/Ed2kKadCommand.cc` gates UDP source requests on the
policy helper and records UDP response usefulness. `src/ed2k_server.*` persists
source-response time, source count, and UDP source request time.

Verified: `cmake --build --preset default --target aria2_tests` passed. Focused
CppUnit paths `DownloadHelperTest::testEd2kServerSourceCadencePolicy`,
`DownloadHelperTest::testEd2kServerStateUpdate`,
`Ed2kHelperTest::testServerStatePayload`, and
`SessionSerializerTest::testSaveEd2kDownload` passed. `git diff --check` and
the CSV parser check passed. A socket-heavy server flow test exposed a hanging
fixture path and was not used as the AR30 gate.

Remaining: Start AR40 Kad discovery cadence.

### AR40 - Kad Discovery Cadence

Changed: Added durable Kad source-search cadence state and removed the command
lifetime one-shot source-search gate as the effective policy owner. Low-source
files can now start repeated bounded Kad source searches when routing state is
usable, with increasing hourly backoff capped at seven searches. Active Kad
source traversal and source-count limits suppress duplicate or low-value
searches. Kad routing snapshots now persist last source-search time and search
count alongside the existing publish, firewall, observed-address, router-node,
and bucket state.

Reference evidence: aMule `CPartFile::Process` starts Kad source search when
the file has fewer than the UDP source limit, Kad is connected, the global
`KADEMLIAASKTIME` gate allows another file request, and the per-file
`m_LastSearchTimeKad` deadline has passed. aMule increases the per-file delay
with `KADEMLIAREASKTIME` and caps the multiplier after several searches.

Current-code evidence: `src/Ed2kAttribute.*` now owns Kad source-search due and
started helpers. `src/Ed2kKadCommand.cc` uses that policy before creating a new
source traversal. `src/Ed2kKadState.*` exposes useful routing size.
`src/ed2k_kad.cc` persists the new Kad source-search metadata.

Verified: `cmake --build --preset default --target aria2_tests` passed. Focused
CppUnit paths `Ed2kKadStateTest::testKadSourceSearchCadence`,
`Ed2kKadStateTest::testRoutingBootstrapAndRefresh`,
`Ed2kHelperTest::testKadRoutingStatePayload`, and
`SessionSerializerTest::testSaveEd2kDownload` passed. `git diff --check` and
the CSV parser check passed.

Remaining: Start AR50 peer lifecycle state machine.

### AR50 - Peer Lifecycle State Machine

Changed: Added an explicit ED2K peer action policy on top of the existing
peer lifecycle classification. `ed2k_policy.*` now returns wait, connect,
reask, callback, retry, skip, or expire actions instead of exposing only the
next connectable peer. Peer scheduling in `Ed2kAttribute.cc` consumes this
policy and only spawns TCP peer commands for connect or retry outcomes. Queued
reask and callback outcomes are now deterministic policy boundaries for AR60
and AR70 without changing network behavior early.

Reference evidence: aMule keeps peer download state separate from connection
creation in `CPartFile::Process` and `CUpDownClient`: queued, downloading,
no-needed-parts, LowID callback wait, error/dead, connecting, and retry-like
states each drive different runtime actions. Queued peers are maintained for
UDP reask instead of being treated as ordinary connect candidates.

Current-code evidence: `src/ed2k_policy.*` owns `PeerActionType` and
`selectPeerAction`. `src/Ed2kAttribute.cc` schedules only connect/retry
actions. Existing `PeerState` storage remains unchanged, preserving session
compatibility while centralizing the decision layer.

Verified: `cmake --build --preset default --target aria2_tests` passed.
Focused CppUnit paths
`DownloadHelperTest::testEd2kPeerActionPolicySelectsConnect`,
`DownloadHelperTest::testEd2kPeerActionPolicyReportsQueuedReask`,
`DownloadHelperTest::testEd2kPeerActionPolicyHandlesCallbackAndExpiry`,
`DownloadHelperTest::testEd2kConnectPolicyIgnoresNonConnectActions`,
`DownloadHelperTest::testEd2kSourcePolicyClassifiesLifecycle`,
`DownloadHelperTest::testEd2kSourcePolicyAppliesActiveCap`,
`DownloadHelperTest::testEd2kPeerSchedulingSkipsBackoff`, and
`DownloadHelperTest::testEd2kPeerSchedulingSkipsConnectingPeer` passed.

Remaining: Start AR60 queued-peer UDP reask.

### AR60 - Queued-Peer UDP Reask

Changed: Added runtime UDP reask state for queued ED2K peers. Peer state now
keeps the remote UDP port, UDP version, pending flag, remote queue-full flag,
last reask time, and next reask time. Peer hello and eMule info parsing now
preserve remote UDP metadata. The existing ED2K UDP command loop queues
bounded `OP_REASKFILEPING` packets for due queued peers and marks the request
pending before the next maintenance tick can select the same peer again.

Reference evidence: aMule `CPartFile::Process` periodically reasks queued
peers through `CUpDownClient::UDPReaskForDownload` using `FILEREASKTIME`.
`CUpDownClient::UDPReaskACK` clears UDP pending state, updates queue rank, and
refreshes last-asked time. `ClientUDPSocket` treats `OP_QUEUEFULL` as temporary
queue state and `OP_FILENOTFOUND` as dead-source evidence.

Current-code evidence: `src/Ed2kAttribute.*` owns UDP reask state transitions,
due-peer selection, queue-full retry state, and UDP-port reply matching.
`src/Ed2kKadCommand.cc` sends due peer reasks from the existing UDP command
owner and applies ACK, queue-full, and file-not-found replies. `src/ed2k_peer.*`
preserves UDP port and version metadata needed for outbound reask.

Verified: `cmake --build --preset default --target aria2_tests` passed.
Focused CppUnit paths
`DownloadHelperTest::testEd2kPeerUdpReaskStateTransitions`,
`DownloadHelperTest::testEd2kPeerUdpReaskReplyMatchesUdpPort`,
`DownloadHelperTest::testEd2kPeerUdpReaskDueSelection`,
`DownloadHelperTest::testEd2kKadCommandQueuesDuePeerReask`,
`Ed2kHelperTest::testEmuleInfoPayload`, and
`Ed2kHelperTest::testPeerHelloPayload` passed.

Remaining: Start AR70 LowID callback handling.

### AR70 - LowID Callback Handling

Changed: Added explicit LowID callback state to `PeerState` and centralized
server-mediated callback transitions in `Ed2kAttribute`. Callback sources now
move through requested, accepted, failed, timed-out, impossible, and completed
states. Server callback requests keep the requested LowID ids in the server
command so `OP_CALLBACKREQUESTED` can bind the accepted endpoint to the
matching pending request. Peer scheduling now blocks LowID direct connects
until an accepted callback exposes a reachable endpoint, and expired failed
callback waits become unreachable without consuming active peer slots.

Reference evidence: aMule `CUpDownClient::TryToConnect` separates
`DS_LOWTOLOWIP`, `DS_WAITCALLBACK`, and `DS_WAITCALLBACKKAD`. aMule
`ServerSocket` accepts server callback endpoints, while `ClientUDPSocket`,
`ClientTCPSocket`, `DownloadClient`, Kad `Search`, and
`KademliaUDPListener` show that buddy and direct UDP callback require a
separate buddy owner, direct callback timeout owner, UDP verification, and
callback routing state. aMule UPnP/NAT helper code is an application-level
router control surface.

Current-code evidence: `src/ed2k_peer.h` owns `LowIdCallbackState`.
`src/Ed2kAttribute.*` owns callback accepted, completed, failed, and expiry
helpers. `src/Ed2kCommand.*` tracks pending server callback request ids and
uses the helper for callback acceptance. `src/ed2k_policy.cc` keeps LowID and
failed callback states out of normal direct-connect scheduling. CAP15 records
buddy/direct callback as a bounded limitation for now, and CAP21 replaces
router helpers with existing aria2-next listen-port ownership.

Verified: `cmake --build --preset default --target aria2_tests` passed.
Focused CppUnit paths
`DownloadHelperTest::testEd2kLowIdCallbackStateTransitions`,
`DownloadHelperTest::testEd2kPeerActionPolicyIsolatesUnreachableLowId`,
`DownloadHelperTest::testEd2kPeerActionPolicyHandlesCallbackAndExpiry`, and
`DownloadHelperTest::testEd2kServerSourceMergeSkipsUnsupportedSources` passed.

Remaining: Start AR80 transfer pacing and retry.
