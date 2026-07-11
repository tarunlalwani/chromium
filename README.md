# DevTools Fetch.getResponseBody null Resume / Collect UAF

## Problem
CDP `Fetch.getResponseBody` then `Fetch.continueResponse` (product order) can:
1. ASAN `heap-use-after-free` in `InterceptionJob::NotifyClient` → `RequestBodyCollector` (POST+body, re-entrant continue during Collect).
2. Windows `EXCEPTION_ACCESS_VIOLATION_READ` on null `MultiplexRouter::Resume…` from `GetResponseBody` → `client_receiver_.Resume()` when unbound.

## Apply
```bash
git am 0001-*.patch
```

## ASAN
```bash
autoninja -C out/ASan content_browsertests
ASAN_OPTIONS=detect_leaks=0:abort_on_error=1:halt_on_error=1:detect_odr_violation=0 \
  xvfb-run -a out/ASan/content_browsertests \
    --gtest_filter='DevToolsFetchGetResponseBodyTest.*' \
    --no-sandbox
```
- RED (without patch): POST test ASAN abort
- GREEN (with patch): both tests PASS

## Branches
- `base/devtools-fetch-getresponsebody-null-resume`
- `fix/devtools-fetch-getresponsebody-null-resume`
- PR: https://github.com/tarunlalwani/chromium/pull/3
