// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_PLATFORM_WINDOW_X11_WINDOW_MOVE_LOOP_CLIENT_H_
#define UI_PLATFORM_WINDOW_X11_WINDOW_MOVE_LOOP_CLIENT_H_

#include <X11/Xlib.h>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/message_loop/message_loop.h"
#include "ui/gfx/geometry/point.h"
#include "ui/platform_window/x11/whole_screen_move_loop.h"
#include "ui/views/views_export.h"
#include "ui/wm/public/window_move_client.h"

namespace ui {

class PlatformWindow;

// When we're dragging tabs, we need to manually position our window.
class WindowMoveLoopClient : public views::X11MoveLoopDelegate {
 public:
  WindowMoveLoopClient();
  ~WindowMoveLoopClient() override;

  // Overridden from X11MoveLoopDelegate:
  void OnMouseMovement(const gfx::Point& screen_point,
                       int flags,
                       base::TimeTicks event_time) override;
  void OnMouseReleased() override;
  void OnMoveLoopEnded() override;

  bool RunMoveLoop(PlatformWindow* window, const gfx::Vector2d& drag_offset);
  void EndMoveLoop();

  bool IsInMoveLoop();

 private:
  WholeScreenMoveLoop move_loop_;

  // We need to keep track of this so we can actually move it when reacting to
  // mouse events.
  PlatformWindow* window_;

  // Our cursor offset from the top left window origin when the drag
  // started. Used to calculate the window's new bounds relative to the current
  // location of the cursor.
  gfx::Vector2d window_offset_;
};

}  // namespace ui

#endif  // UI_PLATFORM_WINDOW_X11_WINDOW_MOVE_LOOP_CLIENT_H_
