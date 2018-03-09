// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/wayland_nested_compositor_watcher.h"

#include "ui/ozone/platform/wayland/wayland_nested_compositor.h"

namespace ui {

WaylandNestedCompositorWatcher::WaylandNestedCompositorWatcher(
    WaylandNestedCompositor* nested_compositor)
    : controller_(FROM_HERE), nested_compositor_(nested_compositor) {
  base::MessageLoopForUI::current()->WatchFileDescriptor(
      nested_compositor_->GetFileDescriptor(),
      true,  // persistent
      base::MessagePumpLibevent::WATCH_READ, &controller_, this);
}

WaylandNestedCompositorWatcher::~WaylandNestedCompositorWatcher() = default;

void WaylandNestedCompositorWatcher::OnFileCanReadWithoutBlocking(int fd) {
  nested_compositor_->Dispatch(base::TimeDelta());
  nested_compositor_->Flush();
}

void WaylandNestedCompositorWatcher::OnFileCanWriteWithoutBlocking(int fd) {
  NOTREACHED();
}

}  // namespace ui