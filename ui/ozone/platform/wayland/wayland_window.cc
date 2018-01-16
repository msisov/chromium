// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/wayland_window.h"

#include <wayland-client.h>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "ui/base/cursor/ozone/bitmap_cursor_factory_ozone.h"
#include "ui/base/hit_test.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/events/ozone/events_ozone.h"
#include "ui/ozone/platform/wayland/wayland_connection.h"
#include "ui/ozone/platform/wayland/xdg_popup_wrapper_v5.h"
#include "ui/ozone/platform/wayland/xdg_popup_wrapper_v6.h"
#include "ui/ozone/platform/wayland/xdg_surface_wrapper_v5.h"
#include "ui/ozone/platform/wayland/xdg_surface_wrapper_v6.h"

namespace ui {

namespace {

// Factory, which decides which version type of xdg object to build.
class XDGShellObjectFactory {
 public:
  XDGShellObjectFactory() = default;
  ~XDGShellObjectFactory() = default;

  std::unique_ptr<XDGSurfaceWrapper> CreateXDGSurface(
      WaylandConnection* connection,
      WaylandWindow* wayland_window) {
    if (connection->shell_v6())
      return std::make_unique<XDGSurfaceWrapperV6>(wayland_window);

    DCHECK(connection->shell());
    return std::make_unique<XDGSurfaceWrapperV5>(wayland_window);
  }

  std::unique_ptr<XDGPopupWrapper> CreateXDGPopup(
      WaylandConnection* connection,
      WaylandWindow* wayland_window) {
    if (connection->shell_v6()) {
      std::unique_ptr<XDGSurfaceWrapper> surface =
          CreateXDGSurface(connection, wayland_window);
      surface->Initialize(connection, wayland_window->surface(), false);
      return std::make_unique<XDGPopupWrapperV6>(std::move(surface),
                                                 wayland_window);
    }
    DCHECK(connection->shell());
    return std::make_unique<XDGPopupWrapperV5>(wayland_window);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(XDGShellObjectFactory);
};

static WaylandWindow* g_current_capture_ = nullptr;

// TODO(msisov, tonikitoo): fix customization according to screen resolution
// once we are able to get global coordinates of wayland windows.
gfx::Rect TranslateBoundsToScreenCoordinates(const gfx::Rect& child_bounds,
                                             const gfx::Rect& parent_bounds) {
  int x = child_bounds.x() - parent_bounds.x();
  int y = child_bounds.y() - parent_bounds.y();
  return gfx::Rect(gfx::Point(x, y), child_bounds.size());
}

}  // namespace

WaylandWindow::WaylandWindow(PlatformWindowDelegate* delegate,
                             WaylandConnection* connection,
                             const gfx::Rect& bounds)
    : delegate_(delegate),
      connection_(connection),
      xdg_shell_objects_factory_(new XDGShellObjectFactory()),
      bounds_(bounds),
      state_(PlatformWindowState::PLATFORM_WINDOW_STATE_UNKNOWN) {}

WaylandWindow::~WaylandWindow() {
  PlatformEventSource::GetInstance()->RemovePlatformEventDispatcher(this);
  connection_->RemoveWindow(surface_.id());
  if (parent_window_)
    parent_window_->set_child_window(nullptr);
}

// static
WaylandWindow* WaylandWindow::FromSurface(wl_surface* surface) {
  return static_cast<WaylandWindow*>(
      wl_proxy_get_user_data(reinterpret_cast<wl_proxy*>(surface)));
}

bool WaylandWindow::Initialize() {
  DCHECK(xdg_shell_objects_factory_);

  surface_.reset(wl_compositor_create_surface(connection_->compositor()));
  if (!surface_) {
    LOG(ERROR) << "Failed to create wl_surface";
    return false;
  }
  wl_surface_set_user_data(surface_.get(), this);

  // There is now default initialization for this type. Initialize it
  // to ::WINDOW here. It will be changed by delelgate if it know the
  // type of the window.
  ui::PlatformWindowType ui_window_type =
      ui::PlatformWindowType::PLATFORM_WINDOW_TYPE_WINDOW;
  delegate_->GetWindowType(&ui_window_type);
  switch (ui_window_type) {
    case ui::PlatformWindowType::PLATFORM_WINDOW_TYPE_MENU:
    case ui::PlatformWindowType::PLATFORM_WINDOW_TYPE_POPUP:
      // TODO(msisov, tonikitoo): Handle notification windows, which are marked
      // as popup windows as well. Those are the windows that do not have
      // parents and popup when a browser receive a notification.
      CreateXdgPopup();
      break;
    case ui::PlatformWindowType::PLATFORM_WINDOW_TYPE_TOOLTIP:
      // Tooltips subsurfaces are created on demand, uppon ::Show calls.
      is_tooltip_ = true;
      break;
    case ui::PlatformWindowType::PLATFORM_WINDOW_TYPE_WINDOW:
      CreateXdgSurface();
      break;
    default:
      NOTREACHED() << "Not supported window type: type=" << ui_window_type;
      break;
  }

  connection_->ScheduleFlush();

  connection_->AddWindow(surface_.id(), this);
  PlatformEventSource::GetInstance()->AddPlatformEventDispatcher(this);
  delegate_->OnAcceleratedWidgetAvailable(surface_.id(), 1.f);

  return true;
}

void WaylandWindow::CreateXdgPopup() {
  if (!parent_window_)
    parent_window_ = GetParentWindow();

  DCHECK(parent_window_);

  gfx::Rect bounds =
      TranslateBoundsToScreenCoordinates(bounds_, parent_window_->GetBounds());

  xdg_popup_ = xdg_shell_objects_factory_->CreateXDGPopup(connection_, this);
  if (!xdg_popup_ ||
      !xdg_popup_->Initialize(connection_, surface(), parent_window_, bounds)) {
    CHECK(false) << "Failed to create xdg_popup";
  }

  parent_window_->set_child_window(this);
}

void WaylandWindow::CreateXdgSurface() {
  xdg_surface_ =
      xdg_shell_objects_factory_->CreateXDGSurface(connection_, this);
  if (!xdg_surface_ ||
      !xdg_surface_->Initialize(connection_, surface_.get(), true)) {
    CHECK(false) << "Failed to create xdg_surface";
  }
}

void WaylandWindow::CreateTooltipSubSurface() {
  parent_window_ = GetParentWindow();

  // Tooltip creation is an async operation. By the time Mus actually start to
  // create the tooltip, it is possible that user has already moved the
  // mouse/pointer out of the window who triggered the tooptip. In this case,
  // parent_window_ is NULL.
  if (!parent_window_) {
    Hide();
    return;
  }

  wl_subcompositor* subcompositor = connection_->subcompositor();
  DCHECK(subcompositor);
  tooltip_subsurface_.reset(wl_subcompositor_get_subsurface(
      subcompositor, surface_.get(), parent_window_->surface()));

  wl_subsurface_set_position(tooltip_subsurface_.get(), bounds_.x(),
                             bounds_.y());
  wl_subsurface_set_desync(tooltip_subsurface_.get());
  wl_surface_commit(parent_window_->surface());
  connection_->ScheduleFlush();
}

void WaylandWindow::ApplyPendingBounds() {
  if (pending_bounds_.IsEmpty())
    return;

  SetBounds(pending_bounds_);
  DCHECK(xdg_surface_);
  xdg_surface_->SetWindowGeometry(bounds_);
  xdg_surface_->AckConfigure();
  pending_bounds_ = gfx::Rect();
  connection_->ScheduleFlush();
}

bool WaylandWindow::HasCapture() {
  return g_current_capture_ == this;
}

void WaylandWindow::Show() {
  if (!is_tooltip_)
    set_keyboard_focus(true);

  if (xdg_surface_)
    return;
  if (is_tooltip_) {
    if (!tooltip_subsurface_)
      CreateTooltipSubSurface();
    return;
  }
  if (!xdg_popup_) {
    CreateXdgPopup();
    connection_->ScheduleFlush();
  }
}

void WaylandWindow::Hide() {
  if (is_tooltip_) {
    wl_surface_attach(surface_.get(), NULL, 0, 0);
    wl_surface_commit(surface_.get());
    // Tooltip subsurface must be reset only after the buffer is detached.
    // Otherwise, gnome shell, for example, can end up with broken event
    // pipe.
    tooltip_subsurface_.reset();
    return;
  }
  if (child_window_)
    child_window_->Hide();
  if (xdg_popup_) {
    parent_window_->set_child_window(nullptr);
    xdg_popup_.reset();
    // Detach buffer from surface in order to completely shutdown popups and
    // release resources.
    wl_surface_attach(surface_.get(), NULL, 0, 0);
    wl_surface_commit(surface_.get());
  }
}

void WaylandWindow::Close() {
  NOTIMPLEMENTED();
}

void WaylandWindow::PrepareForShutdown() {}

void WaylandWindow::SetBounds(const gfx::Rect& bounds) {
  if (bounds == bounds_)
    return;
  bounds_ = bounds;
  delegate_->OnBoundsChanged(bounds);
}

gfx::Rect WaylandWindow::GetBounds() {
  return bounds_;
}

void WaylandWindow::SetTitle(const base::string16& title) {
  DCHECK(xdg_surface_);
  xdg_surface_->SetTitle(title);
  connection_->ScheduleFlush();
}

void WaylandWindow::SetCapture() {
  if (HasCapture())
    return;

  WaylandWindow* old_capture = g_current_capture_;
  if (old_capture)
    old_capture->delegate()->OnLostCapture();

  g_current_capture_ = this;
}

void WaylandWindow::ReleaseCapture() {
  if (HasCapture())
    g_current_capture_ = nullptr;
}

void WaylandWindow::ToggleFullscreen() {
  DCHECK(xdg_surface_);

  // TODO(msisov, tonikitoo): add multiscreen support. As the documentation says
  // if xdg_surface_set_fullscreen() is not provided with wl_output, it's up to
  // the compositor to choose which display will be used to map this surface.
  if (!IsFullscreen()) {
    // Client might have requested a fullscreen state while the window was in
    // a maximized state. Thus, |restored_bounds_| can contain the bounds of a
    // "normal" state before the window was maximized. We don't override them
    // unless they are empty, because |bounds_| can contain bounds of a
    // maximized window instead.
    if (restored_bounds_.IsEmpty())
      restored_bounds_ = bounds_;
    xdg_surface_->SetFullscreen();
  } else {
    xdg_surface_->UnSetFullscreen();
  }

  connection_->ScheduleFlush();
}

void WaylandWindow::Maximize() {
  DCHECK(xdg_surface_);

  if (IsFullscreen())
    ToggleFullscreen();

  // Keeps track of the previous bounds, which are used to restore a window
  // after unmaximize call. We don't override |restored_bounds_| if they have
  // already had value, which means the previous state has been a fullscreen
  // state. That is, the bounds can be stored during a change from a normal
  // state to a maximize state, and then preserved to be the same, when changing
  // from maximized to fullscreen and back to a maximized state.
  if (restored_bounds_.IsEmpty())
    restored_bounds_ = bounds_;

  xdg_surface_->SetMaximized();
  connection_->ScheduleFlush();
}

void WaylandWindow::Minimize() {
  DCHECK(xdg_surface_);

  DCHECK(xdg_surface_);
  xdg_surface_->SetMinimized();
  connection_->ScheduleFlush();

  // Wayland doesn't say if a window is minimized. Handle this case manually
  // here. We can track if the window was unminimized once wayland sends the
  // window is activated, and the previous state was minimized.
  state_ = PlatformWindowState::PLATFORM_WINDOW_STATE_MINIMIZED;
}

void WaylandWindow::Restore() {
  DCHECK(xdg_surface_);

  // Unfullscreen the window if it is fullscreen.
  if (IsFullscreen())
    ToggleFullscreen();

  xdg_surface_->UnSetMaximized();
  connection_->ScheduleFlush();
}

void WaylandWindow::SetCursor(PlatformCursor cursor) {
  scoped_refptr<BitmapCursorOzone> bitmap =
      BitmapCursorFactoryOzone::GetBitmapCursor(cursor);
  if (bitmap_ == bitmap)
    return;

  bitmap_ = bitmap;

  if (bitmap_) {
    connection_->SetCursorBitmap(bitmap_->bitmaps(), bitmap_->hotspot());
  } else {
    connection_->SetCursorBitmap(std::vector<SkBitmap>(), gfx::Point());
  }
}

void WaylandWindow::MoveCursorTo(const gfx::Point& location) {
  NOTIMPLEMENTED();
}

void WaylandWindow::ConfineCursorToBounds(const gfx::Rect& bounds) {
  NOTIMPLEMENTED();
}

PlatformImeController* WaylandWindow::GetPlatformImeController() {
  NOTIMPLEMENTED();
  return nullptr;
}

void WaylandWindow::PerformNativeWindowDragOrResize(uint32_t hittest) {
  connection_->ResetPointerFlags();

  if (hittest == HTCAPTION)
    xdg_surface_->SurfaceMove(connection_);
  else
    xdg_surface_->SurfaceResize(connection_, hittest);
}

bool WaylandWindow::RunMoveLoop(const gfx::Vector2d& drag_offset) {
  return true;
}

void WaylandWindow::StopMoveLoop() {}

bool WaylandWindow::CanDispatchEvent(const PlatformEvent& native_event) {
  if (HasCapture())
    return true;

  // If another window has capture, return early before checking focus.
  if (g_current_capture_)
    return false;

  Event* event = static_cast<Event*>(native_event);
  if (event->IsMouseEvent())
    return has_pointer_focus_;
  if (event->IsKeyEvent())
    return has_keyboard_focus_;
  if (event->IsTouchEvent())
    return has_touch_focus_;
  return false;
}

uint32_t WaylandWindow::DispatchEvent(const PlatformEvent& native_event) {
  Event* event = static_cast<Event*>(native_event);
  if (event->IsLocatedEvent() && !has_pointer_focus_) {
    DCHECK(connection_);
    WaylandWindow* window = connection_->GetCurrentFocusedWindow();
    if (window) {
      ConvertEventLocationToTargetWindowLocation(GetBounds().origin(),
                                                 window->GetBounds().origin(),
                                                 event->AsLocatedEvent());
    }
  }

  DispatchEventFromNativeUiEvent(
      native_event, base::BindOnce(&PlatformWindowDelegate::DispatchEvent,
                                   base::Unretained(delegate_)));
  return POST_DISPATCH_STOP_PROPAGATION;
}

void WaylandWindow::HandleSurfaceConfigure(int32_t width,
                                           int32_t height,
                                           bool is_maximized,
                                           bool is_fullscreen,
                                           bool is_activated) {
  // Propagate the window state information to the client.
  PlatformWindowState old_state = state_;
  if (IsMinimized() && !is_activated)
    state_ = PlatformWindowState::PLATFORM_WINDOW_STATE_MINIMIZED;
  else if (is_fullscreen)
    state_ = PlatformWindowState::PLATFORM_WINDOW_STATE_FULLSCREEN;
  else if (is_maximized)
    state_ = PlatformWindowState::PLATFORM_WINDOW_STATE_MAXIMIZED;
  else
    state_ = PlatformWindowState::PLATFORM_WINDOW_STATE_NORMAL;

  if (old_state != state_)
    delegate_->OnWindowStateChanged(state_);

  // Rather than call SetBounds here for every configure event, just save the
  // most recent bounds, and have WaylandConnection call ApplyPendingBounds
  // when it has finished processing events. We may get many configure events
  // in a row during an interactive resize, and only the last one matters.
  SetPendingBounds(width, height);

  was_active_ = is_active_;
  is_active_ = is_activated;
  if (was_active_ != is_active_)
    delegate_->OnActivationChanged(is_active_);
}

void WaylandWindow::OnCloseRequest() {
  // Before calling OnCloseRequest, the |xdg_popup_| must become hidden and
  // only then call OnCloseRequest().
  DCHECK(!xdg_popup_);
  delegate_->OnCloseRequest();
}

bool WaylandWindow::IsMinimized() const {
  return state_ == PlatformWindowState::PLATFORM_WINDOW_STATE_MINIMIZED;
}

bool WaylandWindow::IsMaximized() const {
  return state_ == PlatformWindowState::PLATFORM_WINDOW_STATE_MAXIMIZED;
}

bool WaylandWindow::IsFullscreen() const {
  return state_ == PlatformWindowState::PLATFORM_WINDOW_STATE_FULLSCREEN;
}

void WaylandWindow::SetPendingBounds(int32_t width, int32_t height) {
  // Width or height set to 0 means that we should decide on width and height by
  // ourselves, but we don't want to set them to anything else. Use restored
  // bounds size or the current bounds.
  //
  // Note: if the browser was started with --start-fullscreen and a user exits
  // the fullscreen mode, wayland may set the width and height to be 1. Instead,
  // explicitly set the bounds to the current desired ones or the previous
  // bounds.
  if (width <= 1 || height <= 1) {
    pending_bounds_.set_size(restored_bounds_.IsEmpty()
                                 ? GetBounds().size()
                                 : restored_bounds_.size());
  } else {
    pending_bounds_ = gfx::Rect(0, 0, width, height);
  }

  if (!IsFullscreen() && !IsMaximized())
    restored_bounds_ = gfx::Rect();
}

WaylandWindow* WaylandWindow::GetParentWindow() {
  gfx::AcceleratedWidget widget = gfx::kNullAcceleratedWidget;
  delegate_->GetParentWindowAcceleratedWidget(&widget);
  WaylandWindow* parent_window = connection_->GetWindow(widget);

  // If propagated parent has already had a child, it means that |this| is a
  // submenu of a 3-dot menu. In aura, the parent of a 3-dot menu and its
  // submenu is the main native widget, which is the main window. In contrast,
  // Wayland requires a menu window to be a parent of a submenu window. Thus,
  // check if the suggested parent has a child. If yes, take its child as a
  // parent of |this|.
  // Another case is a notifcation window or a drop down window, which do not
  // have a parent in aura. In this case, take the current focused window as a
  // parent.
  if (parent_window && parent_window->child_window_)
    return parent_window->child_window_;
  if (!parent_window)
    return connection_->GetCurrentFocusedWindow();
  return parent_window;
}

}  // namespace ui
