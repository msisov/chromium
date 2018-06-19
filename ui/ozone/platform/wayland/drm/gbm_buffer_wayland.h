// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_DRM_GBM_BUFFER_WAYLAND_H_
#define UI_OZONE_PLATFORM_WAYLAND_DRM_GBM_BUFFER_WAYLAND_H_

#include <vector>

#include "base/files/scoped_file.h"
#include "base/macros.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/native_pixmap.h"
#include "ui/ozone/common/linux/gbm_buffer_linux.h"

struct gbm_bo;

namespace ui {

class WaylandSurfaceFactory;
class WaylandConnectionProxy;

// GbmBuffer for a Wayland based backend. It's used on the GPU process side.
class GbmBufferWayland : public GbmBufferLinux {
 public:
  static scoped_refptr<GbmBufferWayland> CreateBuffer(
      WaylandConnectionProxy* connection,
      gfx::Size size,
      gfx::BufferFormat format,
      gfx::BufferUsage usage);

  // Instead of GetFd as ozone/drm does, GetBoFd should be used here as long
  // as linux platforms do not support fd's per plane.
  int GetBoFd() const;
  // Compared to GbmBuffer in ozone/drm, this GbmBuffer is able to return
  // real width and height of gbm_bo passed to WaylandConnection.
  uint32_t GetWidth() const;
  uint32_t GetHeight() const;

  // ScantoutBuffer overrides:
  uint32_t GetFramebufferId() const override;
  uint32_t GetOpaqueFramebufferId() const override;
  uint32_t GetFramebufferPixelFormat() const override;
  uint32_t GetOpaqueFramebufferPixelFormat() const override;
  const GbmDeviceLinux* GetGbmDeviceLinux() const override;

 private:
  GbmBufferWayland(WaylandConnectionProxy* connection,
                   gbm_bo* bo,
                   uint32_t format,
                   uint32_t flags,
                   uint64_t modifier,
                   std::vector<base::ScopedFD>&& fds,
                   const gfx::Size& size,
                   const std::vector<gfx::NativePixmapPlane>&& planes);
  ~GbmBufferWayland() override;

  static scoped_refptr<GbmBufferWayland> CreateBufferForBO(
      WaylandConnectionProxy* connection,
      gbm_bo* bo,
      uint32_t format,
      const gfx::Size& size,
      uint32_t flags);

  WaylandConnectionProxy* connection_;

  DISALLOW_COPY_AND_ASSIGN(GbmBufferWayland);
};

class GbmPixmapWayland : public GbmPixmapLinux {
 public:
  GbmPixmapWayland(WaylandSurfaceFactory* surface_manager,
                   const scoped_refptr<GbmBufferWayland>& buffer);

  // NativePixmap overrides:
  uint64_t GetDmaBufModifier(size_t plane) const override;
  bool ScheduleOverlayPlane(gfx::AcceleratedWidget widget,
                            int plane_z_order,
                            gfx::OverlayTransform plane_transform,
                            const gfx::Rect& display_bounds,
                            const gfx::RectF& crop_rect,
                            bool enable_blend,
                            gfx::GpuFence* gpu_fence) override;

 private:
  ~GbmPixmapWayland() override;

  WaylandSurfaceFactory* surface_manager_;

  DISALLOW_COPY_AND_ASSIGN(GbmPixmapWayland);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_DRM_GBM_BUFFER_WAYLAND_H_
