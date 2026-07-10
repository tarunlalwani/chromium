# DevTools Fetch Response UAF — failed POST with body

Chromium tip base when developed: b2aa0735b9db0348e5bdb25cfc6fb97fc26ce92b (Chrome 152.0.7943.0 tree).
Also reproduced on Electron 43.1.0 (macOS Crashpad).

## Bug

CDP Fetch.enable (Response) + failed POST with body + re-entrant Fetch.continueResponse
causes a use-after-free in InterceptionJob::NotifyClient
(content/browser/devtools/devtools_url_loader_interceptor.cc).

## Apply on a Chromium checkout

```bash
cd src
git am path/to/0001-Fix-UAF-in-DevTools-Fetch-Response-interception-for-failed-POSTs.patch
```

## Reproduce under ASAN

Before the fix: heap-use-after-free. After the fix: PASS.

```bash
autoninja -C out/ASan content_browsertests
ASAN_OPTIONS=detect_leaks=0:abort_on_error=1:halt_on_error=1:detect_odr_violation=0 \
  xvfb-run -a out/ASan/content_browsertests \
    --gtest_filter=DevToolsFetchFailedRequestsTest.ResponseStageFailedPostWithBodyDoesNotUseAfterFree \
    --single-process-tests --no-sandbox
```

## Contents

- 0001-*.patch — interceptor WeakPtr fix + browsertest + BUILD.gn
- devtools_fetch_failed_requests_browsertest.cc
