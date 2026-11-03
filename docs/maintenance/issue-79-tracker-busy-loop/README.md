# Issue #79: tracker announce timer busy loop

This report records the investigation and verified repair on 2026-11-03 for
[issue #79](https://github.com/AnInsomniacy/aria2-next/issues/79). A production
BitTorrent session and an isolated reproduction establish a tracker scheduling
busy loop that can consume one CPU core while a download continues to progress.
The issue author selected HTTP/HTTPS as the affected protocol. Confirmation that
the author's instance has this same cause requires a thread stack or equivalent
evidence; the findings here establish the BitTorrent failure mechanism.

### Production observations

The affected executable was aria2-next 3.0.9, release revision
`55b33e5c012638ac1855d607ed734ac1980d8594`, with ELF BuildID
`5adf4aaad2c1db47dd04ba61a24627c8103be504`. The investigation checkout at
`c625f72fb` contained one subsequent packaging commit and equivalent relevant
engine sources. One hot thread had accumulated over 176.5 CPU hours. A short
untraced sample measured 99.67% user CPU and 0.33% kernel CPU; the main control
thread spent most of its time waiting in epoll.

A three-second syscall trace counted 812 `epoll_wait` calls and 1,624
`timerfd_settime` calls. The timer repeatedly received the absolute expiration
`{0,1}`, causing immediate epoll wakeups. Disassembly of the stripped release
ELF mapped sampled program counters to the following functions. Offsets are
relative to the ELF load base and identify this exact BuildID.

| ELF offset | Function |
| --- | --- |
| `0x8fe803` | `torrent::update_tracker_timer` |
| `0x8ff7b4` | `torrent::announce_with_tracker` |
| `0x90005c` | `torrent::on_tracker_announce` |
| `0x908e6b` | `torrent::wrap` |
| `0x909cf8` | Asio timer completion |
| `0x197cd5` | `boost::asio::detail::scheduler::do_run_one` |
| `0x197fab` | `boost::asio::detail::scheduler::run` |
| `0x85f407` | libtorrent network thread |

The active task held 1,192 trackers, with approximately 40 endpoints per
tracker. RPC reported 175 working, 957 error, 57 not contacted and 3 updating
entries. There were 108 adjacent tier decreases, including 58 same-host UDP
entries moved ahead of lower-tier HTTP entries. The large endpoint set amplified
the cost of each repeated traversal. A read-only internal-state inspection
confirmed a V1 timer expiration equal to the inspection time.

Replaying both selection algorithms over that snapshot produced the decisive
entries below for one socket/endpoint/protocol combination. All three entries
were enabled and were not updating. Indices describe traversal positions.

| Index | Tier | Failures | Next announce minus inspection time (seconds) |
| --- | --- | --- | --- |
| 97 | 90 | 0 | +526 |
| 137 | 92 | 99 | +1280 |
| 139 | 27 | 1 | -475349 |

After index 97, announce selection retained tier 90 and `sent_announce=true`.
Index 137 remained in backoff and left that state intact. Index 139 was then
skipped because its tier was at most 90. Timer selection advanced to tier 92 at
index 137 and reset `found_working=false`; it subsequently selected index 139's
finite, expired deadline. A complete announce traversal sent zero requests,
leaving the state unchanged, while timer selection clamped the expiration to
the current time. This directly establishes the repeated callback condition.
The release already included the `next_announce != time_point32::min()` guard;
the selected deadline in this failure was finite and expired.

### Root cause and repair

In `third_party/libtorrent/src/tracker_list.cpp`,
`tracker_list::prioritize_udp_trackers()` moves a UDP entry ahead of an HTTP
entry with the same hostname. Moving intrusive-list nodes across tiers while
retaining their original tier numbers breaks the ordered-tier invariant used
by announce selection and timer selection. Their different state transitions
then allow a skipped backup to drive an immediately expiring timer indefinitely.
The application enables `announce_to_all_tiers` and disables
`announce_to_all_trackers`, which exposes this selection path.

The vendored compatibility fix swaps the two entries' tier values before moving
their nodes, preserving the tiers of the list positions. Tracker objects retain
their identity and associated state. The iterator is restored to the old UDP
position after replacement, including when that node was last, so traversal can
continue safely through consecutive candidates. This follows the position-tier
behavior present in the older
[upstream v2.0.11 implementation](https://github.com/arvidn/libtorrent/blob/v2.0.11/src/torrent.cpp#L5478).
The RC_2_1 source inspected during the investigation shared the omission; that
observation describes the investigation snapshot rather than a claim about
future upstream revisions.

A related application boundary occurs when additional trackers exceed
libtorrent's eight-bit tier range. `src/bt/Metainfo.cc` previously assigned
unbounded increasing integers, which wrapped during conversion to `uint8_t`.
The patch caps appended tiers at 255 and places subsequent entries in that final
fallback tier, preserving input order and duplicate filtering. The large production list exhausted this range and placed late additions in
existing tiers, changing their intended fallback priority. The four-tracker
reproduction establishes that the UDP ordering defect also occurs within range.
An artificial sleep would leave the selection inconsistency unresolved; the
repair restores the invariant that both scheduling algorithms require.

### Controlled reproduction and control

An isolated network namespace with loopback only ran the same production 3.0.9
ELF and dynamic libraries. It used separate configuration and state, a generated
missing-file torrent, empty peer responses, and disabled discovery and port
mapping. Four local trackers started in order A0, H0, B1, U2; H and U shared a
host. Faulty UDP preference produced A0, U2, B1, H0. A initially failed, then
succeeded with a long interval; the remaining entries followed finite failure
backoff. At approximately 72 seconds H expired while announce selection kept
skipping it. CPU rose from idle to 98.7%, then 99.8% and 99.8%. H remained at
two failures and `nextAnnounce=0` while A retained a future deadline.

The control changed only U's loopback host, preventing the same-host exchange.
With the same executable and response logic, order remained A0, H0, B1, U2.
Over 90.2 seconds, the highest-thread CPU samples stayed within 0-0.5%, including
after H's deadline. This isolates the ordering change as a causal trigger in
this experiment. The first isolated GDB stack had relocation limitations from
explicit dynamic-loader startup; function attribution above comes from the
production ELF mapping. These experiments validate the diagnosis; the patched
binary acceptance results follow below.

### Regression coverage and acceptance

`tests/TrackerListTest.cc` covers cross-tier UDP preference, object identity and
tracker state, last-working identity, repeated prioritization, consecutive and
last-node swaps, equal tiers, empty lists, and unrelated hosts. `tests/BtTest.cc`
covers retained-tier ordering, exclusions and duplicates, the 254/255 boundary,
270 appended trackers, repeated option application, and replacing all trackers.

The opt-in `tools/transfer_validation/bittorrent/tracker.py` runs four local HTTP
and UDP fixtures for about 90 seconds. It checks ordered runtime tiers through
RPC, keeps the recovered primary on a future deadline, and requires an actual
backup network retry after the finite backoff. The task must remain active and
RPC responsive. These behavioral assertions avoid a machine-dependent CPU
threshold. The runner requires Python 3.11+, a BitTorrent-enabled executable,
and support for the fixture's loopback aliases. It is excluded from `all`.

```sh
cmake --preset default
cmake --build --preset default
ctest --preset default
python3 tools/transfer_validation/run.py bittorrent_tracker --engine PATH
```

Validation completed on Linux x86_64 with GCC 11.5.0, CMake 3.31.8, Ninja
1.13.2, and Python 3.11.5. The default RelWithDebInfo superbuild compiled every
engine dependency from the vendored source. The host needed a task-local Perl
installation and `CMAKE_INSTALL_LIBDIR=lib` in the dependency build caches
because its native default is `lib64`; these environment adaptations are not
repository changes. SHA-256 checks confirmed that the implementation and test
files in the build copy exactly matched the submitted working tree.

| Validation | Status |
| --- | --- |
| Production sampling and scheduler-state replay | Completed; supports the diagnosis above |
| Unmodified 3.0.9 isolated reproduction and control | Completed; 99.8% versus 0-0.5% highest-thread CPU |
| New regression against unmodified 3.0.9 | Expected failure in 1.229 s: runtime tiers `0,2,1,0` fail the ordering assertion |
| Patched default configure and build | Passed, including executable and test linking |
| Patched CTest suite | Passed: 81 doctest cases, 498 assertions, no skipped cases |
| Tracker-focused unit cases | Passed: 8 cases, 36 assertions |
| Patched binary version | Passed: 3.0.9, with BitTorrent enabled |
| Patched tracker regression | Passed: 90.258 s observation, 90 RPC samples; requests A=2, H=3, B=3, U=1 |
| Ordinary BitTorrent validation | Passed in 38.524 s: single transfer, file selection, magnet selection, pause/restart recovery, and HTTP-fetched torrent follow-up |

The failed backup H made its third real request after the second finite
backoff, whereas the unmodified reproduction remained stuck at two failures.
The fixed-run acceptance uses ordering and retry progress rather than a sampled
CPU threshold. The production instance was not upgraded as part of this work.
These checks cover Linux x86_64; the other release platforms were not built in
this investigation.

Raw production snapshots, debugger payloads, logs and temporary experiment
artifacts remain outside the repository. This report retains aggregate evidence,
binary identity, scheduling fields and a durable local regression procedure.
Confirmation of the issue author's HTTP/HTTPS case remains open pending matching
runtime evidence.
