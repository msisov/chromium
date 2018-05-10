// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_DRM_GBM_BUFFER_H_
#define UI_OZONE_PLATFORM_WAYLAND_DRM_GBM_BUFFER_H_

#include <vector>

#include "base/files/scoped_file.h"
#include "base/macros.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/native_pixmap.h"
#include "ui/ozone/platform/drm/gpu/scanout_buffer.h"

#include "base/memory/ref_counted.h"

struct gbm_bo;

namespace ui {

class GbmDeviceBase;
class SurfaceFactoryOzone;

class GbmBuffer : public ScanoutBuffer {
 public:
  ~GbmBuffer();
  static scoped_refptr<GbmBuffer> CreateBuffer(
      const scoped_refptr<GbmDeviceBase>& gbm,
     gfx::Size size,
     gfx::BufferFormat format,
     gfx::BufferUsage usage);
  
  uint32_t GetFormat() const { return format_; }
  uint32_t GetFlags() const { return flags_; }
  bool AreFdsValid() const;
  size_t GetFdCount() const;
  int GetFd(size_t plane) const;
  int GetStride(size_t plane) const;
  int GetOffset(size_t plane) const;
  size_t GetSize(size_t plane) const;
  //uint32_t GetHandle() const;
  uint32_t GetWidth() const;
  uint32_t GetHeight() const;

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

  gbm_bo* bo() const { return bo_; }

 private:
  GbmBuffer(const scoped_refptr<GbmDeviceBase>& gbm,
            gbm_bo* bo,
            uint32_t format,
            uint32_t flags,
            uint64_t modifier,
            std::vector<base::ScopedFD>&& fds,
            const gfx::Size& size,
            const std::vector<gfx::NativePixmapPlane>&& planes);


  static scoped_refptr<GbmBuffer> CreateBufferForBO(
      const scoped_refptr<GbmDeviceBase>& gbm,
      gbm_bo* bo,
      uint32_t format,
      const gfx::Size& size,
      uint32_t flags);

  scoped_refptr<GbmDeviceBase> gbm_;
  gbm_bo* bo_;
  uint32_t buffer_fd_ = 0;
  uint32_t framebuffer_pixel_format_ = 0;
  // If |opaque_framebuffer_pixel_format_| differs from
  // |framebuffer_pixel_format_| the following member is set to a valid fb,
  // otherwise it is set to 0.
  uint64_t format_modifier_ = 0;
  uint32_t format_;
  uint32_t flags_;
  std::vector<base::ScopedFD> fds_;
  gfx::Size size_;

  std::vector<gfx::NativePixmapPlane> planes_;

  DISALLOW_COPY_AND_ASSIGN(GbmBuffer);
};

class GbmPixmap : public gfx::NativePixmap {
 public:
  GbmPixmap(SurfaceFactoryOzone* surface_manager,
            const scoped_refptr<GbmBuffer>& buffer);

  // NativePixmap:
  void* GetEGLClientBuffer() const override;
  bool AreDmaBufFdsValid() const override;
  size_t GetDmaBufFdCount() const override;
  int GetDmaBufFd(size_t plane) const override;
  int GetDmaBufPitch(size_t plane) const override;
  int GetDmaBufOffset(size_t plane) const override;
  uint64_t GetDmaBufModifier(size_t plane) const override;
  gfx::BufferFormat GetBufferFormat() const override;
  gfx::Size GetBufferSize() const override;
  uint32_t GetUniqueId() const override;
  bool ScheduleOverlayPlane(gfx::AcceleratedWidget widget,
                            int plane_z_order,
                            gfx::OverlayTransform plane_transform,
                            const gfx::Rect& display_bounds,
                            const gfx::RectF& crop_rect,
                            bool enable_blend) override;
  gfx::NativePixmapHandle ExportHandle() override;

  scoped_refptr<GbmBuffer> buffer() { return buffer_; }

 private:
  ~GbmPixmap() override;
  scoped_refptr<ScanoutBuffer> ProcessBuffer(const gfx::Size& size,
                                             uint32_t format);

  SurfaceFactoryOzone* surface_manager_;
  scoped_refptr<GbmBuffer> buffer_;

  DISALLOW_COPY_AND_ASSIGN(GbmPixmap);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_DRM_GBM_BUFFER_H_
