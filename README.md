# RED RCA: Fetch.getResponseBody / TimeBack order (Crashpad `8840e25c`)

## Summary

Windows TimeBack **1.26.67** / Electron **42.4.0** browser Crashpad dump
`8840e25c` dies in:

`InterceptionJob::GetResponseBody` → `client_receiver_.Resume()` →
`MultiplexRouter::ResumeIncomingMethodCallProcessing` with **null `this`**
(`EXCEPTION_ACCESS_VIOLATION_READ` @ `null+0x208`).

Product order (TimeBack): on `Fetch.requestPaused`, call
`Fetch.getResponseBody` then `Fetch.continueResponse`.

## New Chromium branches (do **not** reuse the failed-POST WeakPtr UAF refs)

| Ref | Role |
|-----|------|
| `base/devtools-fetch-getresponsebody-null-resume` | Clean tip `b2aa0735…` (no WeakPtr NotifyClient patch) |
| `red/devtools-fetch-getresponsebody-null-resume` | RED browsertest only (no interceptor fix) |
| `share/devtools-fetch-getresponsebody-null-resume` | This share tree |

## ASAN RED (confirmed 2026-07-11)

Filter:

```bash
ASAN_OPTIONS=detect_leaks=0:abort_on_error=1:halt_on_error=1:detect_odr_violation=0 \
  xvfb-run -a out/ASan/content_browsertests \
    --gtest_filter='DevToolsFetchGetResponseBodyTest.TimeBackOrderPostWithBodyDoesNotUseAfterFree' \
    --single-process-tests --no-sandbox
```

**RED stack (Linux ASAN):** `heap-use-after-free` in
`RequestBodyCollector` / `InterceptionJob::NotifyClient` while handling
POST+body under TimeBack `getResponseBody`+`continueResponse` order.

Log: `asan-red-post-getresponsebody.log`

**GET-only** (`TimeBackOrderGetWithoutBodySurvives`): **PASSED** (no
`request_body` → Collect skipped).

## vs WeakPtr NotifyClient patch (prior PR)

That patch fixes Collect/`Unretained` during `NotifyClient`. It does **not**
change `GetResponseBody` / `client_receiver_.Resume()`. The Windows dump’s
top frames are Resume-null, not Collect — sibling surface, different
manifestation. TimeBack’s command order still drives both.

## Windows dump classification

| Item | Value |
|------|-------|
| OS | Win64 10.0.26200 |
| ptype | browser |
| Exception | AV read @ null+0x208 |
| Electron | 42.4.0 |
| App | TimeBack 1.26.67 |

## Mitigations

1. Product: gate `getResponseBody`; prefer Network-only when body not required.
2. Upstream: WeakPtr Collect fix (prior) **and** bound-check before
   `client_receiver_.Resume()` in `GetResponseBody`.
3. This share is **RED-only** — no interceptor fix commit.

## Builder worktree

Primary `/mnt/chromium/src` stays on `main`. Work this issue from:

```bash
export PATH=/mnt/chromium/bin:/mnt/chromium/depot_tools:$PATH
cd "$(chromium-wt path fetch-uaf-red-getresponsebody)"
```

See `/mnt/chromium/WORKTREES.md`.
