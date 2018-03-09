// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/ozone/platform/wayland/wayland_object.h"

namespace ui {

class WaylandNestedCompositorClient {
 public:
  WaylandNestedCompositorClient();
  ~WaylandNestedCompositorClient();

  wl_display* display() { return wl_display_.get(); }

  bool Initialize();

  // Returns new surface. This cannot be called twice.
  wl_surface* CreateOrGetSurface();

 private:
  void Flush();

  static void Global(void* data,
                     wl_registry* registry,
                     uint32_t name,
                     const char* interface,
                     uint32_t version);
  static void GlobalRemove(void* data, wl_registry* registry, uint32_t name);

  wl::Object<wl_display> wl_display_;
  wl::Object<wl_registry> registry_;
  wl::Object<wl_compositor> compositor_;

  wl::Object<wl_surface> wl_surface_;

  DISALLOW_COPY_AND_ASSIGN(WaylandNestedCompositorClient);
};

}  // namespace ui
