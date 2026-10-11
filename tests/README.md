# Tests

Unit tests use doctest (`third_party/doctest`) and link the engine library
`aria2_core`. Every `*.cc` here is part of the `aria2_tests` executable, which
CTest runs:

```bash
ctest --preset default
build/default/aria2_tests --test-case='*Kad*'   # a subset
```

Test behaviour at module boundaries: protocol codecs, schedulers, state
machines, persistence and parsing. Name test cases after the behaviour they
pin down. Network transfers end to end belong to `tools/transfer_validation`,
not here; tests must not reach real external hosts.
