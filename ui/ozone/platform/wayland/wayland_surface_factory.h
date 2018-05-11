// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_WAYLAND_SURFACE_FACTORY_H_
#define UI_OZONE_PLATFORM_WAYLAND_WAYLAND_SURFACE_FACTORY_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "ui/gl/gl_surface.h"
#include "ui/ozone/public/surface_factory_ozone.h"

#include "base/posix/eintr_wrapper.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/sequenced_task_runner_handle.h"

namespace ui {

class WaylandConnectionProxy;
class GbmSurfaceless;

class WaylandSurfaceFactory : public SurfaceFactoryOzone {
 public:
  explicit WaylandSurfaceFactory(WaylandConnectionProxy* connection);
  ~WaylandSurfaceFactory() override;

  void ScheduleBufferSwap(gfx::AcceleratedWidget widget, uint32_t buffer_id);

  // SurfaceFactoryOzone:
  void RegisterSurface(gfx::AcceleratedWidget widget,
                       gl::SurfacelessEGL* surface) override;
  void UnregisterSurface(gfx::AcceleratedWidget widget) override;
  gl::SurfacelessEGL* GetSurface(gfx::AcceleratedWidget widget) const override;

  std::vector<gl::GLImplementation> GetAllowedGLImplementations() override;
  GLOzone* GetGLOzone(gl::GLImplementation implementation) override;
  std::unique_ptr<SurfaceOzoneCanvas> CreateCanvasForWidget(
      gfx::AcceleratedWidget widget) override;
  scoped_refptr<gfx::NativePixmap> CreateNativePixmap(
      gfx::AcceleratedWidget widget,
      gfx::Size size,
      gfx::BufferFormat format,
      gfx::BufferUsage usage) override;
  scoped_refptr<gfx::NativePixmap> CreateNativePixmapFromHandle(
      gfx::AcceleratedWidget widget,
      gfx::Size size,
      gfx::BufferFormat format,
      const gfx::NativePixmapHandle& handle) override;

 private:
  WaylandConnectionProxy* connection_ = nullptr;
  std::unique_ptr<GLOzone> egl_implementation_;
  std::unique_ptr<GLOzone> osmesa_implementation_;

  std::map<gfx::AcceleratedWidget, GbmSurfaceless*> widget_to_surface_map_;

  DISALLOW_COPY_AND_ASSIGN(WaylandSurfaceFactory);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_WAYLAND_SURFACE_FACTORY_H_
