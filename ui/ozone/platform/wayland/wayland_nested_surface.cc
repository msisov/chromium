// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/wayland_nested_surface.h"

#include "ui/ozone/platform/wayland/wayland_connection.h"
#include "ui/ozone/platform/wayland/wayland_nested_compositor.h"

namespace ui {

namespace {

template <class T>
T* GetUserDataAs(struct wl_resource* resource) {
  return static_cast<T*>(wl_resource_get_user_data(resource));
}

}  // namespace

WaylandNestedSurface::Buffer::Buffer(struct wl_resource* resource)
    : resource_(resource) {
  wl_list_init(&destroy_listener_.link);
  destroy_listener_.notify = DestroyListenerCallback;
  wl_resource_add_destroy_listener(resource_, &destroy_listener_);
}

WaylandNestedSurface::Buffer::~Buffer() {
  wl_list_remove(&destroy_listener_.link);
}

// static
WaylandNestedSurface::Buffer* WaylandNestedSurface::Buffer::GetBuffer(
    struct wl_resource* resource) {
  if (struct wl_listener* listener =
          wl_resource_get_destroy_listener(resource, DestroyListenerCallback)) {
    Buffer* buffer;
    return wl_container_of(listener, buffer, destroy_listener_);
  }
  return new Buffer(resource);
}

void WaylandNestedSurface::Buffer::OnBufferAttach() {
  ++attach_count_;
}

void WaylandNestedSurface::Buffer::OnBufferDetach() {
  --attach_count_;
  if (!attach_count_)
    wl_resource_queue_event(resource_, WL_BUFFER_RELEASE);
}

// static
void WaylandNestedSurface::Buffer::DestroyListenerCallback(
    struct wl_listener* listener,
    void* data) {
  Buffer* buffer = wl_container_of(listener, buffer, destroy_listener_);
  if (buffer)
    delete buffer;
}

// WaylandNestedSurface --------------------------------------------------------

WaylandNestedSurface::WaylandNestedSurface(WaylandNestedCompositor* compositor,
                                           struct wl_surface* surface)
    : compositor_(compositor),
      image_(EGL_NO_IMAGE_KHR),
      nested_surface_(surface) {
  glGenTextures(1, &texture_);
  glBindTexture(GL_TEXTURE_2D, texture_);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
}

WaylandNestedSurface::~WaylandNestedSurface() {}

void WaylandNestedSurface::AttachBuffer(struct wl_resource* buffer) {
  if (buffer)
    pending_buffer_ = Buffer::GetBuffer(buffer);
}

void WaylandNestedSurface::RequestFrameCallback(const FrameCallback& callback) {
  pending_frame_callbacks_.push_back(callback);
}

void WaylandNestedSurface::Commit() {
  static struct wl_callback_listener frame_listener = {
      WaylandNestedSurface::OnFrameCallback};

  static struct wl_buffer_listener buffer_listener = {
      WaylandNestedSurface::OnBufferRelease};

  if (!nested_surface_) {
    LOG(ERROR) << "Nested surface is not available.";
    return;
  }

  if (image_ != EGL_NO_IMAGE_KHR)
    compositor_->DestroyImage(image_);

  image_ = compositor_->CreateEGLImageKHRFromResource(
      pending_buffer_->wl_resource());

  MakePendingBufferCurrent();
  MakePendingFrameCallbacksCurrent();

  if (image_ == EGL_NO_IMAGE_KHR) {
    FlushFrameCallbacks(base::TimeTicks().Now());
    return;
  }

  // TODO(msisov, tonikitoo): instead of allocation thousands of buffers, reuse
  // a released one. Check OnBufferRelease.
  wl_buffer_ = compositor_->CreateWaylandBufferFromImage(image_);
  DCHECK(wl_buffer_);
  wl_surface_attach(nested_surface_, wl_buffer_, 0, 0);

  wl_buffer_add_listener(wl_buffer_, &buffer_listener, this);

  if (frame_callback_)
    wl_callback_destroy(frame_callback_);
  frame_callback_ = wl_surface_frame(nested_surface_);
  wl_callback_add_listener(frame_callback_, &frame_listener, this);

  wl_surface_commit(nested_surface_);

  compositor_->connection()->ScheduleFlush();
}

void WaylandNestedSurface::MakePendingBufferCurrent() {
  if (pending_buffer_ == buffer_)
    return;

  if (buffer_)
    buffer_->OnBufferDetach();

  if (pending_buffer_)
    pending_buffer_->OnBufferAttach();

  buffer_ = pending_buffer_;
}

void WaylandNestedSurface::FlushFrameCallbacks(base::TimeTicks time) {
  std::list<FrameCallback> frame_callbacks;
  frame_callbacks.splice(frame_callbacks.end(), frame_callbacks_);
  frame_callbacks_.clear();
  for (const auto& cb : frame_callbacks) {
    cb.Run(time);
  }
}

void WaylandNestedSurface::MakePendingFrameCallbacksCurrent() {
  frame_callbacks_.splice(frame_callbacks_.end(), pending_frame_callbacks_);
  pending_frame_callbacks_.clear();
}

// static
void WaylandNestedSurface::OnFrameCallback(void* data,
                                           struct wl_callback* callback,
                                           uint32_t time) {
  WaylandNestedSurface* surface = static_cast<WaylandNestedSurface*>(data);
  surface->FlushFrameCallbacks(base::TimeTicks().Now());
  if (callback)
    wl_callback_destroy(callback);
  surface->frame_callback_ = nullptr;
}

// static
void WaylandNestedSurface::OnBufferRelease(void* data,
                                           struct wl_buffer* wl_buffer) {
  // TODO(msisov, tonikitoo): reuse buffer instead. See wl_buffer_listener
  // documentation.
  wl_buffer_destroy(wl_buffer);
}

}  // namespace ui
