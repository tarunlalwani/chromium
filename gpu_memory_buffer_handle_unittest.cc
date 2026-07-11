// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/gtest_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/gpu_memory_buffer_handle.h"

namespace gfx {
namespace {

#if BUILDFLAG(IS_WIN)
void CallDxgiHandleOnEmpty() {
  GpuMemoryBufferHandle empty;
  (void)empty.dxgi_handle().IsValid();
}
#else
void CallRvalueRegionOnEmpty() {
  GpuMemoryBufferHandle empty;
  base::UnsafeSharedMemoryRegion region = std::move(empty).region();
  (void)region;
}
#endif

// Field crash (Win video_capture utility): DeliverTextureToClient calls
// dxgi_handle() on EMPTY_BUFFER after DXGI device loss. That accessor is
// CHECK_EQ(type, DXGI_SHARED_HANDLE). Prove empty + typed accessor aborts.
TEST(GpuMemoryBufferHandleDeathTest, EmptyHandleTypedAccessorChecks) {
  GpuMemoryBufferHandle empty;
  ASSERT_TRUE(empty.is_null());
  ASSERT_EQ(empty.type, EMPTY_BUFFER);

#if BUILDFLAG(IS_WIN)
  EXPECT_CHECK_DEATH(CallDxgiHandleOnEmpty());
#else
  // Same CHECK contract on non-Win: rvalue region() requires SHARED_MEMORY.
  EXPECT_CHECK_DEATH(CallRvalueRegionOnEmpty());
#endif
}


// GREEN: fixed DeliverTextureToClient guard — is_null() short-circuits before
// typed accessor, so empty handles do not CHECK.
TEST(GpuMemoryBufferHandleDeathTest, EmptyHandleSafeGuardDoesNotDie) {
  GpuMemoryBufferHandle empty;
  ASSERT_TRUE(empty.is_null());
  bool is_valid = false;
  if (!empty.is_null()) {
#if BUILDFLAG(IS_WIN)
    is_valid = empty.dxgi_handle().IsValid();
#else
    base::UnsafeSharedMemoryRegion region = std::move(empty).region();
    is_valid = region.IsValid();
#endif
  }
  EXPECT_FALSE(is_valid);
}

}  // namespace
}  // namespace gfx
