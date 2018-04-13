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

namespace ui {

class WaylandConnection;
class WaylandNestedCompositorClient;

class WaylandSurfaceFactory : public SurfaceFactoryOzone {
 public:
  explicit WaylandSurfaceFactory(WaylandConnection* connection);
  explicit WaylandSurfaceFactory(WaylandNestedCompositorClient* client);
  ~WaylandSurfaceFactory() override;

  // SurfaceFactoryOzone:
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
  WaylandNestedCompositorClient* client_;

  WaylandConnection* connection_;
  std::unique_ptr<GLOzone> egl_implementation_;
  std::unique_ptr<GLOzone> osmesa_implementation_;

  DISALLOW_COPY_AND_ASSIGN(WaylandSurfaceFactory);
};

class WaylandPixmap : public gfx::NativePixmap {
 public:
  WaylandPixmap(uint32_t fd, gfx::BufferFormat format, gfx::Size size) {
    fd_ = std::move(fd);
    format_ = format;
    size_ = size;
  }

  // NativePixmap:
  void* GetEGLClientBuffer() const override { return nullptr; }
  bool AreDmaBufFdsValid() const override { return true; }
  size_t GetDmaBufFdCount() const override { return 1; }
  int GetDmaBufFd(size_t plane) const override { return fd_; }
  int GetDmaBufPitch(size_t plane) const override { return 0; }
  int GetDmaBufOffset(size_t plane) const override { return 0; }
  uint64_t GetDmaBufModifier(size_t plane) const override { return 0; }
  gfx::BufferFormat GetBufferFormat() const override {
    return format_;
  }
  gfx::Size GetBufferSize() const override { return size_; }
  uint32_t GetUniqueId() const override { return 0; }
  bool ScheduleOverlayPlane(gfx::AcceleratedWidget widget,
                            int plane_z_order,
                            gfx::OverlayTransform plane_transform,
                            const gfx::Rect& display_bounds,
                            const gfx::RectF& crop_rect,
                            bool enable_blend) override { return false; }
 gfx::NativePixmapHandle ExportHandle() override {
  gfx::NativePixmapHandle handle;
  gfx::BufferFormat format = GetBufferFormat();
  for (size_t i = 0; i < 1; ++i) {
    base::ScopedFD scoped_fd(HANDLE_EINTR(dup(fd_)));
    handle.fds.emplace_back(
          base::FileDescriptor(scoped_fd.release(), true /* auto_close */));
  }
  return handle;
 }

 private:
  ~WaylandPixmap() override {}
  
  int fd_;

  gfx::BufferFormat format_;
  gfx::Size size_;

  DISALLOW_COPY_AND_ASSIGN(WaylandPixmap);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_WAYLAND_SURFACE_FACTORY_H_
