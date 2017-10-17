// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_PLATFORM_WINDOW_X11_X11_WINDOW_BASE_H_
#define UI_PLATFORM_WINDOW_X11_X11_WINDOW_BASE_H_

#include <stdint.h>

#include <array>

#include "base/callback.h"
#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/x/x11.h"
#include "ui/gfx/x/x11_types.h"
#include "ui/platform_window/platform_window.h"
#include "ui/platform_window/platform_window_delegate.h"
#include "ui/platform_window/x11/x11_window_export.h"

namespace ui {

class XScopedEventSelector;

// Abstract base implementation for a X11 based PlatformWindow. Methods that
// are platform specific are left unimplemented.
class X11_WINDOW_EXPORT X11WindowBase : public PlatformWindow {
 public:
  X11WindowBase(PlatformWindowDelegate* delegate, const gfx::Rect& bounds);
  ~X11WindowBase() override;

  // PlatformWindow:
  void Show() override;
  void Hide() override;
  void Close() override;
  void SetBounds(const gfx::Rect& bounds) override;
  gfx::Rect GetBounds() override;
  void SetTitle(const base::string16& title) override;
  void SetCapture() override;
  void ReleaseCapture() override;
  void ToggleFullscreen() override;
  void Maximize() override;
  void Minimize() override;
  void Restore() override;
  void MoveCursorTo(const gfx::Point& location) override;
  void ConfineCursorToBounds(const gfx::Rect& bounds) override;
  PlatformImeController* GetPlatformImeController() override;
  void PerformNativeWindowDragOrResize(uint32_t hittest) override;
  bool RunMoveLoop(const gfx::Vector2d& drag_offset) override;
  void StopMoveLoop() override;

 protected:
  // Creates new underlying XWindow. Does not map XWindow.
  void Create();

  void Destroy();

  void SetPointerGrab();
  void ReleasePointerGrab();

  PlatformWindowDelegate* delegate() { return delegate_; }

  XDisplay* xdisplay() { return xdisplay_; }
  XID xwindow() const { return xwindow_; }

  void UnConfineCursor();

  // Checks if XEvent is for this XWindow.
  bool IsEventForXWindow(const XEvent& xev) const;

  // Processes events for this XWindow.
  void ProcessXWindowEvent(XEvent* xev);

  // Sets a location of ButtonPress event on xroot_window_.
  void SetXRootWindowEventLocation(const gfx::Point& location) {
    xroot_window_event_location_ = location;
  }

 private:
  // Called when WM_STATE property is changed.
  void OnWMStateUpdated();

  bool IsMinimized() const;
  bool IsMaximized() const;
  bool IsFullscreen() const;

  void OnCrossingEvent(bool enter,
                       bool focus_in_window_or_ancestor,
                       int mode,
                       int detail);

  // Called on an XFocusInEvent, XFocusOutEvent, XIFocusInEvent, or an
  // XIFocusOutEvent.
  // TODO(msisov, tonikitoo): share this with DesktopWindowTreeHostX11.
  void OnFocusEvent(bool focus_in, int mode, int detail);

  // Record the activation state.
  // TODO(msisov, tonikitoo): share this with DesktopWindowTreeHostX11.
  void BeforeActivationStateChanged();

  // Handle the state change since BeforeActivationStateChanged().
  // TODO(msisov, tonikitoo): share this with DesktopWindowTreeHostX11.
  void AfterActivationStateChanged();

  PlatformWindowDelegate* const delegate_;

  XDisplay* xdisplay_;
  XID xwindow_ = x11::None;
  XID xroot_window_;
  std::unique_ptr<ui::XScopedEventSelector> xwindow_events_;

  base::string16 window_title_;

  // The bounds of |xwindow_|.
  gfx::Rect bounds_;

  bool IsActive() const;

  bool window_mapped_in_server_ = false;
  // Does |xwindow_| have the pointer grab (XI2 or normal)?
  bool has_pointer_grab_ = false;

  // Is the pointer in |xwindow_| or one of its children?
  bool has_pointer_ = false;

  // Is |xwindow_| or one of its children focused?
  bool has_window_focus_ = false;

  // (An ancestor window or the PointerRoot is focused) && |has_pointer_|.
  // |has_pointer_focus_| == true is the odd case where we will receive keyboard
  // input when |has_window_focus_| == false.  |has_window_focus_| and
  // |has_pointer_focus_| are mutually exclusive.
  bool has_pointer_focus_ = false;

  // Used for tracking activation state in {Before|After}ActivationStateChanged.
  bool was_active_ = false;
  bool had_pointer_ = false;
  bool had_pointer_grab_ = false;
  bool had_window_focus_ = false;

  // The point on xroot_window_, where a ButtonPress event occurred.
  // Used for interactive window drag/resize.
  gfx::Point xroot_window_event_location_;

  // The window manager state bits.
  base::flat_set<::Atom> window_properties_;

  // Stores current state of this window.
  ui::PlatformWindowState state_;

  // Keep track of barriers to confine cursor.
  bool has_pointer_barriers_ = false;
  std::array<XID, 4> pointer_barriers_;

  bool window_mapped_ = false;

  DISALLOW_COPY_AND_ASSIGN(X11WindowBase);
};

}  // namespace ui

#endif  // UI_PLATFORM_WINDOW_X11_X11_WINDOW_BASE_H_
