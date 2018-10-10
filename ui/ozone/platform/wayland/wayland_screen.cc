// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/wayland_screen.h"

#include "ui/display/display.h"
#include "ui/display/display_finder.h"
#include "ui/display/display_observer.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/size.h"
#include "ui/ozone/platform/wayland/wayland_connection.h"
#include "ui/ozone/platform/wayland/wayland_window.h"

namespace ui {

WaylandScreen::WaylandScreen(WaylandConnection* connection)
    : connection_(connection), weak_factory_(this) {}

WaylandScreen::~WaylandScreen() = default;

void WaylandScreen::OnOutputAdded(uint32_t output_id, bool is_primary) {
  display::Display new_display(output_id);
  display_list_.AddDisplay(std::move(new_display),
                           is_primary
                               ? display::DisplayList::Type::PRIMARY
                               : display::DisplayList::Type::NOT_PRIMARY);
}

void WaylandScreen::OnOutputRemoved(uint32_t output_id) {
  display_list_.RemoveDisplay(output_id);
}

void WaylandScreen::OnOutputMetricsChanged(uint32_t output_id,
                                           const gfx::Rect& new_bounds,
                                           float device_pixel_ratio,
                                           bool is_primary) {
  display::Display changed_display(output_id);
  changed_display.set_device_scale_factor(device_pixel_ratio);
  changed_display.set_bounds(new_bounds);
  changed_display.set_work_area(new_bounds);

  display_list_.UpdateDisplay(
      changed_display, is_primary ? display::DisplayList::Type::PRIMARY
                                  : display::DisplayList::Type::NOT_PRIMARY);
}

base::WeakPtr<WaylandScreen> WaylandScreen::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

const std::vector<display::Display>& WaylandScreen::GetAllDisplays() const {
  return display_list_.displays();
}

display::Display WaylandScreen::GetPrimaryDisplay() const {
  auto iter = display_list_.GetPrimaryDisplayIterator();
  if (iter == display_list_.displays().end())
    return display::Display::GetDefaultDisplay();
  return *iter;
}

display::Display WaylandScreen::GetDisplayForAcceleratedWidget(
    gfx::AcceleratedWidget widget) const {
  auto* wayland_window = connection_->GetWindow(widget);
  if (!wayland_window)
    return GetPrimaryDisplay();

  const std::vector<uint32_t> entered_outputs_ids =
      wayland_window->entered_outputs_ids();
  if (entered_outputs_ids.empty())
    return GetPrimaryDisplay();

  // A widget can be located on two displays. Return the one, which was the very
  // first used.
  for (const auto& display : display_list_.displays()) {
    if (display.id() == entered_outputs_ids.front())
      return display;
  }

  NOTREACHED();
  return GetPrimaryDisplay();
}

gfx::Point WaylandScreen::GetCursorScreenPoint() const {
  NOTIMPLEMENTED_LOG_ONCE();
  return gfx::Point();
}

gfx::AcceleratedWidget WaylandScreen::GetAcceleratedWidgetAtScreenPoint(
    const gfx::Point& point) const {
  // This is a tricky one. To ensure right functionality, a widget under a
  // cursor must be returned. But, Wayland clients cannot know where the windows
  // are located in the global space coordinate system. Instead, it's possible
  // to know widgets located on a surface local coordinate system (remember that
  // clients cannot also know the position of the pointer in the global space
  // coordinate system, but rather on a local surface coordinate system). That
  // is, we will have to pretend that a single surface is a "display", where
  // other widgets (child widgets are located in the surface local coordinate
  // system, where the main surface has 0,0 origin) are shown. Whenever that
  // surface is focused (the cursor is located under that widget), we will use
  // it to determine if the point is on that main surface, a menu surface and
  // etc.

  // This call comes only when a cursor is under a certain window (see how
  // Wayland sends pointer events for better understanding + comment above).
  auto* window = connection_->GetCurrentFocusedWindow();
  if (!window)
    return gfx::kNullAcceleratedWidget;

  // If |point| is at origin and the focused window does not contain that point,
  // it must be the root parent, which contains that |point|.
  if (point.IsOrigin() && !window->GetBounds().Contains(point)) {
    WaylandWindow* parent_window = nullptr;
    do {
      parent_window = window->parent_window();
      if (parent_window)
        window = parent_window;
    } while (parent_window);
    DCHECK(!window->parent_window());
  }

  // When there is an implicit grab (mouse is pressed and not released), we
  // start to get events even outside the surface. Thus, if it does not contain
  // the point, return null widget here.
  if (!window->GetBounds().Contains(point))
    return gfx::kNullAcceleratedWidget;
  return window->GetWidget();
}

display::Display WaylandScreen::GetDisplayNearestPoint(
    const gfx::Point& point) const {
  if (display_list_.displays().size() <= 1)
    return GetPrimaryDisplay();
  for (const auto& display : display_list_.displays()) {
    if (display.bounds().Contains(point))
      return display;
  }

  return *FindDisplayNearestPoint(display_list_.displays(), point);
}

display::Display WaylandScreen::GetDisplayMatching(
    const gfx::Rect& match_rect) const {
  if (match_rect.IsEmpty())
    return GetDisplayNearestPoint(match_rect.origin());

  const display::Display* match =
      FindDisplayWithBiggestIntersection(display_list_.displays(), match_rect);
  return match ? *match : GetPrimaryDisplay();
}

void WaylandScreen::AddObserver(display::DisplayObserver* observer) {
  display_list_.AddObserver(observer);
}

void WaylandScreen::RemoveObserver(display::DisplayObserver* observer) {
  display_list_.RemoveObserver(observer);
}

}  // namespace ui
