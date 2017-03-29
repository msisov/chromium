// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/ws/external_window_display_root_window.h"

#include "services/ui/ws/display.h"
#include "services/ui/ws/display_manager.h"
#include "services/ui/ws/window_server.h"

namespace ui {
namespace ws {

ExternalWindowDisplayRootWindow::ExternalWindowDisplayRootWindow(
    WindowServer* window_server,
    const viz::FrameSinkId& frame_sink_id,
    const Properties& properties)
    : ServerWindow(window_server, frame_sink_id, properties),
      window_server_(window_server) {}

void ExternalWindowDisplayRootWindow::SetBounds(
    const gfx::Rect& bounds,
    const base::Optional<viz::LocalSurfaceId>& local_surface_id) {
  Display* display =
      window_server_->display_manager()->GetDisplayContaining(this);
  if (display)
    display->SetBounds(bounds);

  // WindowManagerDisplayRoot::root_ needs to be at 0,0 position relative
  // to its parent not to break mouse/touch events.
  ServerWindow::SetBounds(gfx::Rect(bounds.size()), local_surface_id);
}

}  // namespace ws
}  // namespace ui
