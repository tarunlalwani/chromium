# Win MF video_capture: empty GMB → dxgi_handle() CHECK

## Problem

After DXGI device loss/hung, `GpuMemoryBufferTrackerWin::GetGpuMemoryBufferHandle()`
returns an empty handle (`EMPTY_BUFFER`). `VideoCaptureDeviceMFWin::DeliverTextureToClient`
called `gmb_handle.dxgi_handle().IsValid()`; `dxgi_handle()` does
`CHECK_EQ(type, DXGI_SHARED_HANDLE)` and aborts the video_capture utility process
(`EXCEPTION_BREAKPOINT` / `int 0x3`).

Related (accessor hardening, not this call site): Chromium bug 40584691.
Related (different site): bug 399888623 / CL 6324781.

## Fix

Guard `gmb_handle.is_null()` before `dxgi_handle()` and return `MF_E_UNEXPECTED`.

## Proof

```
# RED (empty typed accessor still CHECKs):
gfx_unittests --gtest_filter=GpuMemoryBufferHandleDeathTest.EmptyHandleTypedAccessorChecks

# GREEN (safe guard does not die):
gfx_unittests --gtest_filter=GpuMemoryBufferHandleDeathTest.EmptyHandleSafeGuardDoesNotDie

# Windows-only (authored; needs Win host):
media_unittests --gtest_filter=GpuMemoryBufferTrackerWinTest.EmptyHandleAfterDeviceLoss*
```

## Apply

```
git am 0001-*.patch
```

Or see the GitHub PR against `base/win-videocapture-empty-gmb-dxgi-check`.
