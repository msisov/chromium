// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_COMMON_LINUX_GBM_BUFFER_LINUX_H_
#define UI_OZONE_COMMON_LINUX_GBM_BUFFER_LINUX_H_

#include <vector>

#include "base/files/scoped_file.h"
#include "base/macros.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/native_pixmap.h"
#include "ui/ozone/common/linux/scanout_buffer.h"

#include "base/memory/ref_counted.h"

struct gbm_bo;

namespace ui {

class GbmDeviceLinux;

// GbmBufferLinux for a drm or wayland based backends.
class GbmBufferLinux : public ScanoutBuffer {
 public:
  uint32_t GetFormat() const { return format_; }
  uint32_t GetFlags() const { return flags_; }
  bool AreFdsValid() const;
  size_t GetFdCount() const;
  int GetFd(size_t plane) const;
  int GetStride(size_t plane) const;
  int GetOffset(size_t plane) const;
  size_t GetSize(size_t plane) const;

  // ScanoutBuffer:
  uint32_t GetFramebufferId() const override;
  uint32_t GetOpaqueFramebufferId() const override;
  uint32_t GetHandle() const override;
  gfx::Size GetSize() const override;
  uint32_t GetFramebufferPixelFormat() const override;
  uint32_t GetOpaqueFramebufferPixelFormat() const override;
  uint64_t GetFormatModifier() const override;
  const DrmDevice* GetDrmDevice() const override;
  bool RequiresGlFinish() const override;

 protected:
  GbmBufferLinux(gbm_bo* bo,
                 uint32_t format,
                 uint32_t flags,
                 uint64_t modifier,
                 std::vector<base::ScopedFD>&& fds,
                 const gfx::Size& size,
                 const std::vector<gfx::NativePixmapPlane>&& planes);
  ~GbmBufferLinux() override;

  gbm_bo* bo() const { return bo_; }

 private:
  gbm_bo* bo_;
  uint64_t format_modifier_ = 0;
  uint32_t format_;
  uint32_t flags_;
  std::vector<base::ScopedFD> fds_;
  gfx::Size size_;

  std::vector<gfx::NativePixmapPlane> planes_;

  DISALLOW_COPY_AND_ASSIGN(GbmBufferLinux);
};

class GbmPixmapLinux : public gfx::NativePixmap {
 public:
  GbmPixmapLinux(const scoped_refptr<GbmBufferLinux>& buffer);
  ~GbmPixmapLinux() override;

  // NativePixmap:
  bool AreDmaBufFdsValid() const override;
  size_t GetDmaBufFdCount() const override;
  int GetDmaBufFd(size_t plane) const override;
  int GetDmaBufPitch(size_t plane) const override;
  int GetDmaBufOffset(size_t plane) const override;
  uint64_t GetDmaBufModifier(size_t plane) const override;
  gfx::BufferFormat GetBufferFormat() const override;
  gfx::Size GetBufferSize() const override;
  uint32_t GetUniqueId() const override;
  gfx::NativePixmapHandle ExportHandle() override;

 protected:
  scoped_refptr<GbmBufferLinux> buffer() { return buffer_; }

 private:
  scoped_refptr<GbmBufferLinux> buffer_;

  DISALLOW_COPY_AND_ASSIGN(GbmPixmapLinux);
};

}  // namespace ui

#endif  // UI_OZONE_COMMON_LINUX_GBM_BUFFER_LINUX_H_
