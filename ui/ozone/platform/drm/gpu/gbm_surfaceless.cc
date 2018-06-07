// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/gpu/gbm_surfaceless.h"

#include "ui/ozone/common/linux/scanout_buffer.h"
#include "ui/ozone/platform/drm/gpu/drm_vsync_provider.h"
#include "ui/ozone/platform/drm/gpu/drm_window_proxy.h"
#include "ui/ozone/platform/drm/gpu/gbm_surface_factory.h"

namespace ui {

GbmSurfaceless::GbmSurfaceless(GbmSurfaceFactory* surface_factory,
                               std::unique_ptr<DrmWindowProxy> window,
                               gfx::AcceleratedWidget widget)
    : GbmSurfacelessLinux(widget),
      surface_factory_(surface_factory),
      window_(std::move(window)) {
  surface_factory_->RegisterSurface(window_->widget(), this);
}

GbmSurfaceless::~GbmSurfaceless() {
  Destroy();  // The EGL surface must be destroyed before SurfaceOzone.
  surface_factory_->UnregisterSurface(window_->widget());
}

bool GbmSurfaceless::Initialize(gl::GLSurfaceFormat format) {
  if (!SurfacelessEGL::Initialize(format))
    return false;
  vsync_provider_ = std::make_unique<DrmVSyncProvider>(window_.get());
  if (!vsync_provider_)
    return false;
  return true;
}

void GbmSurfaceless::SchedulePageFlip(const std::vector<OverlayPlane>& planes,
                                      SwapCompletionOnceCallback callback) {
  DCHECK(!planes.empty());
  window_->SchedulePageFlip(planes, std::move(callback));
}

}  // namespace ui
