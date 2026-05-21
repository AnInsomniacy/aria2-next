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
