// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/drm/gbm_surfaceless_wayland.h"

#include "ui/ozone/common/linux/scanout_buffer.h"
#include "ui/ozone/platform/wayland/wayland_surface_factory.h"

namespace ui {

GbmSurfacelessWayland::GbmSurfacelessWayland(WaylandSurfaceFactory* surface_factory,
                               gfx::AcceleratedWidget widget)
    : GbmSurfacelessLinux(widget),
      surface_factory_(surface_factory) {
  surface_factory_->RegisterSurface(widget, this);
}

bool GbmSurfacelessWayland::SupportsPresentationCallback() {
  // TODO: disable presentation callback for wayland
  return true;
}

bool GbmSurfacelessWayland::SupportsPostSubBuffer() {
  return false;
}

GbmSurfacelessWayland::~GbmSurfacelessWayland() {
  surface_factory_->UnregisterSurface(widget());
}

void GbmSurfacelessWayland::SchedulePageFlip(const std::vector<OverlayPlane>& planes, SwapCompletionOnceCallback callback) {
    DCHECK(!planes.empty());
    // Gbm buffer handle is used as a buffer id to identify, which buffer
    // WaylandConnection must attach.
    uint32_t buffer_id = planes.back().buffer->GetHandle();
    surface_factory_->ScheduleBufferSwap(widget(), buffer_id);

    // TODO(msisov): Fix PresentationFeedback. As for now, it's disabled.
    std::move(callback)
        .Run(gfx::SwapResult::SWAP_ACK, gfx::PresentationFeedback());
}

}  // namespace ui
