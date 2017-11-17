// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_DISPLAY_VIEWPORT_METRICS_H_
#define SERVICES_UI_DISPLAY_VIEWPORT_METRICS_H_

#include <string>

#include "services/ui/public/interfaces/window_manager_constants.mojom.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_widget_types.h"

namespace display {

struct ViewportMetrics {
  std::string ToString() const;

  gfx::Rect bounds_in_pixels;
  float device_scale_factor = 0.0f;
  float ui_scale_factor = 0.0f;

  // TODO(tonikitoo,msisov): This is an abuse of ViewportMetrics,
  // and for upstreaming, we should likely consider renaming ViewportMetrics
  // to DisplayProperties or so.
  ui::mojom::WindowType window_type = ui::mojom::WindowType::WINDOW;

  // An id of a suggested parent window. Used by ozone platforms (currently by
  // Wayland) to get a parent window and form parent-child relationship.
  gfx::AcceleratedWidget parent_window_widget_id = gfx::kNullAcceleratedWidget;
};

inline bool operator==(const ViewportMetrics& lhs, const ViewportMetrics& rhs) {
  return lhs.bounds_in_pixels == rhs.bounds_in_pixels &&
         lhs.device_scale_factor == rhs.device_scale_factor &&
         lhs.ui_scale_factor == rhs.ui_scale_factor;
}

inline bool operator!=(const ViewportMetrics& lhs, const ViewportMetrics& rhs) {
  return !(lhs == rhs);
}

}  // namespace display

#endif  // SERVICES_UI_DISPLAY_VIEWPORT_METRICS_H_
