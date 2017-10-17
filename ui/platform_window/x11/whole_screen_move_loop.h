// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_PLATFORM_WINDOW_X11_WHOLE_SCREEN_MOVE_LOOP_H_
#define UI_PLATFORM_WINDOW_X11_WHOLE_SCREEN_MOVE_LOOP_H_

#include <stdint.h>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "ui/base/cursor/cursor.h"
#include "ui/events/platform/platform_event_dispatcher.h"
#include "ui/gfx/geometry/vector2d_f.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/x/x11_types.h"
#include "ui/views/widget/desktop_aura/x11_move_loop.h"
#include "ui/views/widget/desktop_aura/x11_move_loop_delegate.h"

namespace ui {
class MouseEvent;
class ScopedEventDispatcher;
class XScopedEventSelector;
class PlatformWindowDelegate;

// Runs a nested run loop and grabs the mouse. This is used to implement
// dragging.
class WholeScreenMoveLoop : public ui::PlatformEventDispatcher {
 public:
  explicit WholeScreenMoveLoop(views::X11MoveLoopDelegate* delegate);
  ~WholeScreenMoveLoop() override;

  // ui:::PlatformEventDispatcher:
  bool CanDispatchEvent(const ui::PlatformEvent& event) override;
  uint32_t DispatchEvent(const ui::PlatformEvent& platform_event) override;

  // X11MoveLoop:
  bool RunMoveLoop();
  void UpdateCursor();
  void EndMoveLoop();

  bool in_move_loop() { return in_move_loop_; }

 private:
  // Grabs the pointer, setting the mouse cursor to |cursor|. Returns true if
  // successful.
  bool GrabPointer();

  void GrabEscKey();

  // Creates an input-only window to be used during the drag.
  void CreateDragInputWindow(XDisplay* display);

  // Dispatch mouse movement event to |delegate_| in a posted task.
  void DispatchMouseMovement();

  views::X11MoveLoopDelegate* delegate_;

  // Are we running a nested run loop from RunMoveLoop()?
  bool in_move_loop_;
  std::unique_ptr<ui::ScopedEventDispatcher> nested_dispatcher_;

  // An invisible InputOnly window. Keyboard grab and sometimes mouse grab
  // are set on this window.
  XID grab_input_window_;

  // Events selected on |grab_input_window_|.
  std::unique_ptr<ui::XScopedEventSelector> grab_input_window_events_;

  // Whether the pointer was grabbed on |grab_input_window_|.
  bool grabbed_pointer_;

  base::Closure quit_closure_;

  // Keeps track of whether the move-loop is cancled by the user (e.g. by
  // pressing escape).
  bool canceled_;

  std::unique_ptr<ui::Event> last_motion_in_screen_;
  base::WeakPtrFactory<WholeScreenMoveLoop> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(WholeScreenMoveLoop);
};

}  // namespace ui

#endif  // UI_PLATFORM_WINDOW_X11_WHOLE_SCREEN_MOVE_LOOP_H_
