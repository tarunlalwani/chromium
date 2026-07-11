# VideoCaptureHost shutdown null deref

Chromium tip base when developed: b2aa0735b9db0348e5bdb25cfc6fb97fc26ce92b (Chrome 152.0.7943.0 tree).
Also seen on Electron 42.4.0 / Chromium 148.0.7778.254 (Windows Crashpad ACCESS_VIOLATION_READ).

## Bug

During browser IO-thread teardown, MediaStreamManager::WillDestroyCurrentMessageLoop
nulls video_capture_manager_ before Clear()ing video_capture_hosts_.
VideoCaptureHost destructors then call VideoCaptureManager::DisconnectClient
through a null manager (release: ACCESS_VIOLATION; debug/ASAN: DCHECK).

## Apply on a Chromium checkout

```bash
cd src
git am path/to/0001-Fix-VideoCaptureHost-crash-on-IO-thread-shutdown.patch
```

## Reproduce under ASAN / debug

Before the fix: DCHECK failed: video_capture_manager_.get() (or null deref in release).
After the fix: PASS.

```bash
autoninja -C out/ASan content_unittests
ASAN_OPTIONS=detect_leaks=0:abort_on_error=1:halt_on_error=1:detect_odr_violation=0 \
  out/ASan/content_unittests \
    --gtest_filter=VideoCaptureTest.HostDestructorAfterVideoCaptureManagerShutdown \
    --single-process-tests
```

## Contents

- 0001-*.patch — MediaStreamManager / VideoCaptureHost fix + unittest
- video_capture_unittest.cc — file after patch (for reading only; prefer git am)
