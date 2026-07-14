# HttpCache done_headers doom-race CHECK fix

Patch for Chromium net/http when DoDoomEntryComplete(ERR_CACHE_RACE) restarts a transaction that still has done_headers_create_new_entry_ set, causing DoGetBackendComplete to CHECK(!done_headers_create_new_entry_).

## Apply

```bash
git am 0001-Fix-CHECK-do-not-restart-done_headers_-HttpCache-tx-.patch
```

## Proof

```bash
# RED (before patch): death test
# HttpCacheSimpleGetTest.QueuedDoomRaceAfterDoneHeadersCreateNewEntryRestartsCheckDeath
# See red-httpcache-done-headers.log (PASSED = CHECK fired)

# GREEN (after patch)
ASAN_OPTIONS=detect_odr_violation=0 out/ASan/net_unittests \
  --gtest_filter=HttpCacheSimpleGetTest.QueuedDoomRaceAfterDoneHeadersCreateNewEntryDoesNotRestart
```

Related: crbug 428819090, 433619513; speculative CL https://chromium-review.googlesource.com/c/chromium/src/+/6817840
