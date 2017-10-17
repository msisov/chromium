// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/platform_window/x11/window_move_loop_client.h"

#include <X11/Xlib.h>

#include "base/debug/stack_trace.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/x/x11_util.h"
#include "ui/events/event.h"

#include "ui/platform_window/platform_window.h"

namespace ui {

WindowMoveLoopClient::WindowMoveLoopClient()
    : move_loop_(this), window_(nullptr) {}

WindowMoveLoopClient::~WindowMoveLoopClient() {}

void WindowMoveLoopClient::OnMouseMovement(const gfx::Point& screen_point,
                                           int flags,
                                           base::TimeTicks event_time) {
  gfx::Point system_loc = screen_point - window_offset_;
  window_->SetBounds(gfx::Rect(system_loc, gfx::Size()));
}

void WindowMoveLoopClient::OnMouseReleased() {
  EndMoveLoop();
}

void WindowMoveLoopClient::OnMoveLoopEnded() {
  window_ = nullptr;
}

////////////////////////////////////////////////////////////////////////////////
// DesktopWindowTreeHostLinux, wm::WindowMoveClient implementation:

bool WindowMoveLoopClient::RunMoveLoop(PlatformWindow* window,
                                       const gfx::Vector2d& drag_offset) {
  window_offset_ = drag_offset;
  window_ = window;
  window_->SetCapture();
  return move_loop_.RunMoveLoop();
}

void WindowMoveLoopClient::EndMoveLoop() {
  window_->ReleaseCapture();
  move_loop_.EndMoveLoop();
}

bool WindowMoveLoopClient::IsInMoveLoop() {
  return move_loop_.in_move_loop();
}

}  // namespace ui
