// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_DRM_GBM_SURFACELESS_WAYLAND_H_
#define UI_OZONE_PLATFORM_WAYLAND_DRM_GBM_SURFACELESS_WAYLAND_H_

#include "base/macros.h"
#include "ui/ozone/common/linux/gbm_surfaceless_linux.h"
#include "ui/ozone/public/swap_completion_callback.h"

namespace ui {

class WaylandSurfaceFactory;

// A GLSurface for GBM Ozone platform that uses surfaceless drawing. Drawing and
// displaying happens directly through NativePixmap buffers. CC would call into
// SurfaceFactoryOzone to allocate the buffers and then call
// ScheduleOverlayPlane(..) to schedule the buffer for presentation.
class GbmSurfacelessWayland : public GbmSurfacelessLinux {
 public:
  GbmSurfacelessWayland(WaylandSurfaceFactory* surface_factory,
                 gfx::AcceleratedWidget widget);
  
  // gl::GLSurface:
  bool SupportsPresentationCallback() override;                                 
  bool SupportsPostSubBuffer() override;

 private:
  ~GbmSurfacelessWayland() override;
   
  void SchedulePageFlip(const std::vector<OverlayPlane>& planes, SwapCompletionOnceCallback callback) override;

  WaylandSurfaceFactory* surface_factory_;

  DISALLOW_COPY_AND_ASSIGN(GbmSurfacelessWayland);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_DRM_GBM_SURFACELESS_WAYLAND_H_
