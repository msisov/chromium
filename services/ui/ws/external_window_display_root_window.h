// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_WS_EXTERNAL_WINDOW_DISPLAY_ROOT_WINDOW_H_
#define SERVICES_UI_WS_EXTERNAL_WINDOW_DISPLAY_ROOT_WINDOW_H_

#include "components/viz/common/surfaces/frame_sink_id.h"
#include "services/ui/ws/server_window.h"
#include "ui/gfx/geometry/rect.h"

namespace ui {
namespace ws {

class WindowServer;

// This class overrides specific methods of ws::ServerWindow
// for external window mode. The methods forward calls to
// ws::Display (and down to platform/ozone level) accordingly.
class ExternalWindowDisplayRootWindow : public ServerWindow {
 public:
  ExternalWindowDisplayRootWindow(WindowServer* window_server,
                                  const viz::FrameSinkId& frame_sink_id,
                                  const Properties& properties);
  ~ExternalWindowDisplayRootWindow() override = default;

  // ServerWindow
  void SetBounds(const gfx::Rect& bounds,
                 const base::Optional<viz::LocalSurfaceId>& local_surface_id =
                     base::nullopt) override;
  void SetVisible(bool value) override;

 private:
  WindowServer* window_server_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(ExternalWindowDisplayRootWindow);
};

}  // namespace ws
}  // namespace ui

#endif  // SERVICES_UI_WS_EXTERNAL_WINDOW_DISPLAY_ROOT_WINDOW_H_
