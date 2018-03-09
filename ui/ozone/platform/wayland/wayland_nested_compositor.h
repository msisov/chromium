// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/scoped_make_current.h"

struct wl_buffer;
struct wl_display;
struct wl_resource;

namespace ui {

class WaylandConnection;

class WaylandNestedCompositor {
 public:
  WaylandNestedCompositor(WaylandConnection* connection);
  ~WaylandNestedCompositor();

  bool Initialize();

  int GetFileDescriptor() const;

  void Dispatch(base::TimeDelta timeout);

  void Flush();

  wl_display* display() const { return wl_display_; }
  EGLDisplay egl_display() const { return egl_display_; }
  WaylandConnection* connection() const { return connection_; }

  wl_buffer* CreateWaylandBufferFromImage(EGLImageKHR image);
  void DestroyImage(EGLImageKHR image);
  EGLImageKHR CreateEGLImageKHRFromResource(struct wl_resource* resource);

 private:
  // EGL initialization helper methods.
  bool InitializeEGL();
  void InitializeEGLBindings();
  void InitializeEGLDisplay();

  bool AddSocket(const std::string socket_name);

  wl_display* wl_display_ = nullptr;

  WaylandConnection* connection_;  // non-owned primary client connection.

  EGLDisplay egl_display_ = EGL_NO_DISPLAY;
  scoped_refptr<gl::GLSurface> gl_surface_;
  scoped_refptr<gl::GLContext> gl_context_;
  std::unique_ptr<ui::ScopedMakeCurrent> make_current_;

  DISALLOW_COPY_AND_ASSIGN(WaylandNestedCompositor);
};

}  // namespace ui
