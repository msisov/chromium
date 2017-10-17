// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/platform_window/x11/whole_screen_move_loop.h"

#include <stddef.h>
#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "ui/aura/client/capture_client.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/x/x11_pointer_grab.h"
#include "ui/base/x/x11_util.h"
#include "ui/base/x/x11_window_event_manager.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/events/keycodes/keyboard_code_conversion_x.h"
#include "ui/events/ozone/events_ozone.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/events/platform/scoped_event_dispatcher.h"
#include "ui/events/platform/x11/x11_event_source.h"
#include "ui/events/platform/x11/x11_event_source_libevent.h"
#include "ui/gfx/x/x11.h"
#include "ui/platform_window/platform_window_delegate.h"

namespace ui {

// XGrabKey requires the modifier mask to explicitly be specified.
const unsigned int kModifiersMasks[] = {0,         // No additional modifier.
                                        Mod2Mask,  // Num lock
                                        LockMask,  // Caps lock
                                        Mod5Mask,  // Scroll lock
                                        Mod2Mask | LockMask,
                                        Mod2Mask | Mod5Mask,
                                        LockMask | Mod5Mask,
                                        Mod2Mask | LockMask | Mod5Mask};

WholeScreenMoveLoop::WholeScreenMoveLoop(views::X11MoveLoopDelegate* delegate)
    : delegate_(delegate),
      in_move_loop_(false),
      grab_input_window_(x11::None),
      grabbed_pointer_(false),
      canceled_(false),
      weak_factory_(this) {}

WholeScreenMoveLoop::~WholeScreenMoveLoop() {}

void WholeScreenMoveLoop::DispatchMouseMovement() {
  if (!last_motion_in_screen_ && !last_motion_in_screen_->IsLocatedEvent())
    return;
  delegate_->OnMouseMovement(
      last_motion_in_screen_->AsLocatedEvent()->location(),
      last_motion_in_screen_->flags(), last_motion_in_screen_->time_stamp());
  last_motion_in_screen_.reset();
}

////////////////////////////////////////////////////////////////////////////////
// DesktopWindowTreeHostLinux, ui::PlatformEventDispatcher implementation:

bool WholeScreenMoveLoop::CanDispatchEvent(const ui::PlatformEvent& event) {
  return in_move_loop_;
}

uint32_t WholeScreenMoveLoop::DispatchEvent(
    const ui::PlatformEvent& platform_event) {
  DCHECK(base::MessageLoopForUI::IsCurrent());

  // This method processes all events while the move loop is active.
  if (!in_move_loop_)
    return ui::POST_DISPATCH_PERFORM_DEFAULT;

  auto* event = static_cast<ui::Event*>(platform_event);
  switch (event->type()) {
    case ui::ET_MOUSE_MOVED:
    case ui::ET_MOUSE_DRAGGED: {
      bool can_send = !last_motion_in_screen_.get();

      ui::MouseEvent* mouse_event = event->AsMouseEvent();
      gfx::Point root_location = mouse_event->root_location();
      mouse_event->set_location(root_location);

      last_motion_in_screen_ = ui::Event::Clone(*event);
      DCHECK(last_motion_in_screen_->IsMouseEvent());

      if (can_send)
        DispatchMouseMovement();
      return ui::POST_DISPATCH_PERFORM_DEFAULT;
    }
    case ui::ET_MOUSE_RELEASED: {
      ui::MouseEvent* mouse_event = event->AsMouseEvent();
      gfx::Point root_location = mouse_event->root_location();
      mouse_event->set_location(root_location);

      last_motion_in_screen_ = ui::Event::Clone(*event);
      DCHECK(last_motion_in_screen_->IsMouseEvent());

      EndMoveLoop();
      return ui::POST_DISPATCH_PERFORM_DEFAULT;
    }
    case ui::ET_KEY_PRESSED:
      canceled_ = true;
      EndMoveLoop();
      return ui::POST_DISPATCH_NONE;
    default:
      break;
  }
  return ui::POST_DISPATCH_PERFORM_DEFAULT;
}

bool WholeScreenMoveLoop::RunMoveLoop() {
  DCHECK(!in_move_loop_);  // Can only handle one nested loop at a time.

  CreateDragInputWindow(gfx::GetXDisplay());

  if (!GrabPointer()) {
    XDestroyWindow(gfx::GetXDisplay(), grab_input_window_);
    CHECK(false) << "failed to grab pointer";
    return false;
  }

  GrabEscKey();

  std::unique_ptr<ui::ScopedEventDispatcher> old_dispatcher =
      std::move(nested_dispatcher_);
  nested_dispatcher_ =
      ui::PlatformEventSource::GetInstance()->OverrideDispatcher(this);

  base::WeakPtr<WholeScreenMoveLoop> alive(weak_factory_.GetWeakPtr());

  in_move_loop_ = true;
  canceled_ = false;
  base::MessageLoopForUI* loop = base::MessageLoopForUI::current();
  base::MessageLoop::ScopedNestableTaskAllower allow_nested(loop);
  base::RunLoop run_loop;
  quit_closure_ = run_loop.QuitClosure();
  run_loop.Run();

  if (!alive)
    return false;

  nested_dispatcher_ = std::move(old_dispatcher);
  return !canceled_;
}

void WholeScreenMoveLoop::UpdateCursor() {}

void WholeScreenMoveLoop::EndMoveLoop() {
  if (!in_move_loop_)
    return;

  // Prevent DispatchMouseMovement from dispatching any posted motion event.
  last_motion_in_screen_.reset();

  // TODO(erg): Is this ungrab the cause of having to click to give input focus
  // on drawn out windows? Not ungrabbing here screws the X server until I kill
  // the chrome process.

  // Ungrab before we let go of the window.
  if (grabbed_pointer_)
    ui::UngrabPointer();
  else
    UpdateCursor();

  XDisplay* display = gfx::GetXDisplay();
  unsigned int esc_keycode = XKeysymToKeycode(display, XK_Escape);
  for (size_t i = 0; i < arraysize(kModifiersMasks); ++i) {
    XUngrabKey(display, esc_keycode, kModifiersMasks[i], grab_input_window_);
  }

  // Restore the previous dispatcher.
  nested_dispatcher_.reset();
  grab_input_window_events_.reset();
  XDestroyWindow(display, grab_input_window_);
  grab_input_window_ = x11::None;
  in_move_loop_ = false;
  quit_closure_.Run();
}

bool WholeScreenMoveLoop::GrabPointer() {
  XDisplay* display = gfx::GetXDisplay();

  // Pass "owner_events" as false so that X sends all mouse events to
  // |grab_input_window_|.
  int ret = ui::GrabPointer(grab_input_window_, false, x11::None);
  if (ret != GrabSuccess) {
    DLOG(ERROR) << "Grabbing pointer for dragging failed: "
                << ui::GetX11ErrorString(display, ret);
  }
  XFlush(display);
  return ret == GrabSuccess;
}

void WholeScreenMoveLoop::GrabEscKey() {
  XDisplay* display = gfx::GetXDisplay();
  unsigned int esc_keycode = XKeysymToKeycode(display, XK_Escape);
  for (size_t i = 0; i < arraysize(kModifiersMasks); ++i) {
    XGrabKey(display, esc_keycode, kModifiersMasks[i], grab_input_window_,
             x11::False, GrabModeAsync, GrabModeAsync);
  }
}

void WholeScreenMoveLoop::CreateDragInputWindow(XDisplay* display) {
  unsigned long attribute_mask = CWEventMask | CWOverrideRedirect;
  XSetWindowAttributes swa;
  memset(&swa, 0, sizeof(swa));
  swa.override_redirect = x11::True;
  grab_input_window_ = XCreateWindow(display, DefaultRootWindow(display), -100,
                                     -100, 10, 10, 0, CopyFromParent, InputOnly,
                                     CopyFromParent, attribute_mask, &swa);
  uint32_t event_mask = ButtonPressMask | ButtonReleaseMask |
                        PointerMotionMask | KeyPressMask | KeyReleaseMask |
                        StructureNotifyMask;
  grab_input_window_events_.reset(
      new ui::XScopedEventSelector(grab_input_window_, event_mask));

  XMapRaised(display, grab_input_window_);
}

}  // namespace ui
