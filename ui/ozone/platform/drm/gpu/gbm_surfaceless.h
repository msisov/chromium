// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_GPU_GBM_SURFACELESS_H_
#define UI_OZONE_PLATFORM_DRM_GPU_GBM_SURFACELESS_H_

#include "ui/ozone/common/linux/gbm_surfaceless_linux.h"
#include "ui/ozone/public/swap_completion_callback.h"

namespace ui {

class DrmWindowProxy;
class GbmSurfaceFactory;

// A GLSurface for GBM Ozone platform that uses surfaceless drawing. Drawing and
// displaying happens directly through NativePixmap buffers. CC would call into
// SurfaceFactoryOzone to allocate the buffers and then call
// ScheduleOverlayPlane(..) to schedule the buffer for presentation.
class GbmSurfaceless : public GbmSurfacelessLinux {
 public:
  GbmSurfaceless(GbmSurfaceFactory* surface_factory,
                 std::unique_ptr<DrmWindowProxy> window,
                 gfx::AcceleratedWidget widget);

  // gl::GLSurface:
  bool Initialize(gl::GLSurfaceFormat format) override;

 protected:
  ~GbmSurfaceless() override;

  GbmSurfaceFactory* surface_factory() { return surface_factory_; }

 private:
  void SchedulePageFlip(const std::vector<OverlayPlane>& planes,
                        SwapCompletionOnceCallback callback) override;

  GbmSurfaceFactory* surface_factory_;
  std::unique_ptr<DrmWindowProxy> window_;

  std::unique_ptr<gfx::VSyncProvider> vsync_provider_;

  DISALLOW_COPY_AND_ASSIGN(GbmSurfaceless);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_GPU_GBM_SURFACELESS_H_
