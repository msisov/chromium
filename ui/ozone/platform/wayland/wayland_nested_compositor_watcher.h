// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_WAYLAND_NESTED_COMPOSITOR_WATCHER_H_
#define UI_OZONE_PLATFORM_WAYLAND_WAYLAND_NESTED_COMPOSITOR_WATCHER_H_

#include "base/macros.h"
#include "base/message_loop/message_loop.h"

namespace ui {

class WaylandNestedCompositor;

class WaylandNestedCompositorWatcher
    : public base::MessagePumpLibevent::Watcher {
 public:
  explicit WaylandNestedCompositorWatcher(
      WaylandNestedCompositor* nested_compositor);
  ~WaylandNestedCompositorWatcher() override;

  // base::MessagePumpLibevent::Watcher:
  void OnFileCanReadWithoutBlocking(int fd) override;
  void OnFileCanWriteWithoutBlocking(int fd) override;

 private:
  base::MessagePumpLibevent::FileDescriptorWatcher controller_;
  WaylandNestedCompositor* nested_compositor_;  // non-owned.

  DISALLOW_COPY_AND_ASSIGN(WaylandNestedCompositorWatcher);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_WAYLAND_NESTED_COMPOSITOR_WATCHER_H_