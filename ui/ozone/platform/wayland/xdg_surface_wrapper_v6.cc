// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/xdg_surface_wrapper_v6.h"

#include <xdg-shell-unstable-v6-client-protocol.h>

#include "base/strings/utf_string_conversions.h"
#include "ui/base/hit_test.h"
#include "ui/ozone/platform/wayland/wayland_connection.h"
#include "ui/ozone/platform/wayland/wayland_window.h"

namespace ui {

namespace {

// Identifies the direction of the "hittest" for Wayland.
bool IdentifyDirection(int hittest, int* direction) {
  DCHECK(direction);
  *direction = -1;
  switch (hittest) {
    case HTBOTTOM:
      *direction =
          zxdg_toplevel_v6_resize_edge::ZXDG_TOPLEVEL_V6_RESIZE_EDGE_BOTTOM;
      break;
    case HTBOTTOMLEFT:
      *direction = zxdg_toplevel_v6_resize_edge::
          ZXDG_TOPLEVEL_V6_RESIZE_EDGE_BOTTOM_LEFT;
      break;
    case HTBOTTOMRIGHT:
      *direction = zxdg_toplevel_v6_resize_edge::
          ZXDG_TOPLEVEL_V6_RESIZE_EDGE_BOTTOM_RIGHT;
      break;
    case HTLEFT:
      *direction =
          zxdg_toplevel_v6_resize_edge::ZXDG_TOPLEVEL_V6_RESIZE_EDGE_LEFT;
      break;
    case HTRIGHT:
      *direction =
          zxdg_toplevel_v6_resize_edge::ZXDG_TOPLEVEL_V6_RESIZE_EDGE_RIGHT;
      break;
    case HTTOP:
      *direction =
          zxdg_toplevel_v6_resize_edge::ZXDG_TOPLEVEL_V6_RESIZE_EDGE_TOP;
      break;
    case HTTOPLEFT:
      *direction =
          zxdg_toplevel_v6_resize_edge::ZXDG_TOPLEVEL_V6_RESIZE_EDGE_TOP_LEFT;
      break;
    case HTTOPRIGHT:
      *direction =
          zxdg_toplevel_v6_resize_edge::ZXDG_TOPLEVEL_V6_RESIZE_EDGE_TOP_RIGHT;
      break;
    default:
      return false;
  }
  return true;
}

}  // namespace

XDGSurfaceWrapperV6::XDGSurfaceWrapperV6(WaylandWindow* wayland_window)
    : wayland_window_(wayland_window) {}

XDGSurfaceWrapperV6::~XDGSurfaceWrapperV6() {}

bool XDGSurfaceWrapperV6::Initialize(WaylandConnection* connection,
                                     wl_surface* surface,
                                     bool with_toplevel) {
  static const zxdg_surface_v6_listener zxdg_surface_v6_listener = {
      &XDGSurfaceWrapperV6::Configure,
  };
  static const zxdg_toplevel_v6_listener zxdg_toplevel_v6_listener = {
      &XDGSurfaceWrapperV6::ConfigureTopLevel,
      &XDGSurfaceWrapperV6::CloseTopLevel,
  };

  // If this surface is created for the popup role, mark it like that as long as
  // acknowledge configure must be sent back on each configure event.
  surface_for_popup_ = !with_toplevel;

  zxdg_surface_v6_.reset(
      zxdg_shell_v6_get_xdg_surface(connection->shell_v6(), surface));
  if (!zxdg_surface_v6_) {
    LOG(ERROR) << "Failed to create zxdg_surface";
    return false;
  }
  zxdg_surface_v6_add_listener(zxdg_surface_v6_.get(),
                               &zxdg_surface_v6_listener, this);
  // XDGPopupV6 requires a separate surface to be created, so this is just a
  // request to get an xdg_surface for it.
  if (surface_for_popup_)
    return true;

  zxdg_toplevel_v6_.reset(zxdg_surface_v6_get_toplevel(zxdg_surface_v6_.get()));
  if (!zxdg_toplevel_v6_) {
    LOG(ERROR) << "Failed to create zxdg_toplevel";
    return false;
  }
  zxdg_toplevel_v6_add_listener(zxdg_toplevel_v6_.get(),
                                &zxdg_toplevel_v6_listener, this);
  wl_surface_commit(surface);
  return true;
}

void XDGSurfaceWrapperV6::SetMaximized() {
  DCHECK(zxdg_toplevel_v6_);
  zxdg_toplevel_v6_set_maximized(zxdg_toplevel_v6_.get());
}

void XDGSurfaceWrapperV6::UnSetMaximized() {
  DCHECK(zxdg_toplevel_v6_);
  zxdg_toplevel_v6_unset_maximized(zxdg_toplevel_v6_.get());
}

void XDGSurfaceWrapperV6::SetFullscreen() {
  DCHECK(zxdg_toplevel_v6_);
  zxdg_toplevel_v6_set_fullscreen(zxdg_toplevel_v6_.get(), nullptr);
}

void XDGSurfaceWrapperV6::UnSetFullscreen() {
  DCHECK(zxdg_toplevel_v6_);
  zxdg_toplevel_v6_unset_fullscreen(zxdg_toplevel_v6_.get());
}

void XDGSurfaceWrapperV6::SetMinimized() {
  DCHECK(zxdg_toplevel_v6_);
  zxdg_toplevel_v6_set_minimized(zxdg_toplevel_v6_.get());
}

void XDGSurfaceWrapperV6::SurfaceMove(WaylandConnection* connection) {
  DCHECK(zxdg_toplevel_v6_);
  zxdg_toplevel_v6_move(zxdg_toplevel_v6_.get(), connection->seat(),
                        connection->serial());
}

void XDGSurfaceWrapperV6::SurfaceResize(WaylandConnection* connection,
                                        uint32_t hittest) {
  int direction;
  if (!IdentifyDirection(hittest, &direction))
    return;
  DCHECK(zxdg_toplevel_v6_);
  zxdg_toplevel_v6_resize(zxdg_toplevel_v6_.get(), connection->seat(),
                          connection->serial(), direction);
}

void XDGSurfaceWrapperV6::SetTitle(const base::string16& title) {
  DCHECK(zxdg_toplevel_v6_);
  zxdg_toplevel_v6_set_title(zxdg_toplevel_v6_.get(),
                             base::UTF16ToUTF8(title).c_str());
}

void XDGSurfaceWrapperV6::AckConfigure() {
  DCHECK(zxdg_surface_v6_);
  zxdg_surface_v6_ack_configure(zxdg_surface_v6_.get(),
                                pending_configure_serial_);
}

void XDGSurfaceWrapperV6::SetWindowGeometry(const gfx::Rect& bounds) {
  DCHECK(zxdg_surface_v6_);
  zxdg_surface_v6_set_window_geometry(zxdg_surface_v6_.get(), bounds.x(),
                                      bounds.y(), bounds.width(),
                                      bounds.height());
}

// static
void XDGSurfaceWrapperV6::Configure(void* data,
                                    struct zxdg_surface_v6* zxdg_surface_v6,
                                    uint32_t serial) {
  XDGSurfaceWrapperV6* surface = static_cast<XDGSurfaceWrapperV6*>(data);
  surface->pending_configure_serial_ = serial;

  if (surface->surface_for_popup_)
    surface->AckConfigure();
}

// static
void XDGSurfaceWrapperV6::ConfigureTopLevel(
    void* data,
    struct zxdg_toplevel_v6* zxdg_toplevel_v6,
    int32_t width,
    int32_t height,
    struct wl_array* states) {
  XDGSurfaceWrapperV6* surface = static_cast<XDGSurfaceWrapperV6*>(data);

  bool is_maximized =
      CheckIfWlArrayHasValue(states, ZXDG_TOPLEVEL_V6_STATE_MAXIMIZED);
  bool is_fullscreen =
      CheckIfWlArrayHasValue(states, ZXDG_TOPLEVEL_V6_STATE_FULLSCREEN);
  bool is_activated =
      CheckIfWlArrayHasValue(states, ZXDG_TOPLEVEL_V6_STATE_ACTIVATED);

  surface->wayland_window_->HandleSurfaceConfigure(width, height, is_maximized,
                                                   is_fullscreen, is_activated);
}

// static
void XDGSurfaceWrapperV6::CloseTopLevel(
    void* data,
    struct zxdg_toplevel_v6* zxdg_toplevel_v6) {
  NOTIMPLEMENTED();
}

zxdg_surface_v6* XDGSurfaceWrapperV6::xdg_surface() const {
  DCHECK(zxdg_surface_v6_);
  return zxdg_surface_v6_.get();
}

}  // namespace ui
