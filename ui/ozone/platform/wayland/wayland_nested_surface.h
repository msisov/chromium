// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_WAYLAND_NESTED_SURFACE_H_
#define UI_OZONE_PLATFORM_WAYLAND_WAYLAND_NESTED_SURFACE_H_

#include <list>

#include <wayland-server-protocol-core.h>

#include "base/callback.h"
#include "base/macros.h"
#include "ui/gl/gl_bindings.h"

namespace ui {

class WaylandNestedCompositor;

// This is a representation of wayland surface, which nested compositor creates.
// The WaylandNestedSurface is mapped to certain wl_surface created by
// WaylandWindow, and a buffer is attached to it. Once new contents are
// committed from gpu, this surface signals host compositor about new contents
// to be drawn.
class WaylandNestedSurface {
 public:
  WaylandNestedSurface(WaylandNestedCompositor* compositor,
                       struct wl_surface* surface);
  ~WaylandNestedSurface();

  // Attaches a new WaylandNestedSurface::Buffer linked to |buffer| to this
  // surface.
  void AttachBuffer(struct wl_resource* buffer);

  // Sets |callback| to |pending_frame_callbacks_|.
  using FrameCallback = base::Callback<void(base::TimeTicks frame_time)>;
  void RequestFrameCallback(const FrameCallback& callback);

  // Creates an EGLImageKHR out of the wl_resource buffer contents and commits
  // the data to host compositor, which draws pixels.
  void Commit();

  void MakePendingBufferCurrent();

  void FlushFrameCallbacks(base::TimeTicks time);

  void MakePendingFrameCallbacksCurrent();

 private:
  // A buffer abstraction, which represents clients' buffer contents via
  // wl_resource.
  class Buffer {
   public:
    Buffer(struct wl_resource* resource);
    ~Buffer();

    // Gets already created buffer or creates a new one based on |resource|.
    static Buffer* GetBuffer(struct wl_resource* resource);

    void OnBufferAttach();

    void OnBufferDetach();

    struct wl_resource* wl_resource() { return resource_; }

   private:
    static void DestroyListenerCallback(struct wl_listener* listener,
                                        void* data);

    // A pointer to a buffer resource, which clients holds and renders to. It's
    // used to create an EGL image and commit the contents to host compositor.
    struct wl_resource* resource_ = nullptr;
    struct wl_listener destroy_listener_;

    uint32_t attach_count_ = 0;

    DISALLOW_COPY_AND_ASSIGN(Buffer);
  };

  static void OnFrameCallback(void* data,
                              struct wl_callback* callback,
                              uint32_t time);

  static void OnBufferRelease(void* data, struct wl_buffer* wl_buffer);

  WaylandNestedCompositor* compositor_ = nullptr;

  unsigned texture_;
  EGLImageKHR image_;

  // Hold client's frame callbacks.
  std::list<FrameCallback> pending_frame_callbacks_;
  std::list<FrameCallback> frame_callbacks_;

  Buffer* buffer_ = nullptr;
  Buffer* pending_buffer_ = nullptr;

  struct wl_buffer* wl_buffer_ = nullptr;
  struct wl_callback* frame_callback_ = nullptr;

  // This wl_surface represents a real wayland surface this nested surface is
  // mapped to. wl_buffer is attached to this surface.
  struct wl_surface* nested_surface_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(WaylandNestedSurface);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_WAYLAND_NESTED_SURFACE_H_
