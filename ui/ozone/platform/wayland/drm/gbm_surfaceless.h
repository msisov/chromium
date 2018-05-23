// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_DRM_GBM_SURFACELESS_H_
#define UI_OZONE_PLATFORM_WAYLAND_DRM_GBM_SURFACELESS_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gl/gl_image.h"
#include "ui/gl/gl_surface_egl.h"
#include "ui/gl/gl_surface_overlay.h"
#include "ui/gl/scoped_binders.h"
#include "ui/ozone/common/linux/overlay_plane.h"

namespace ui {

class WaylandSurfaceFactory;

// A GLSurface for Wayland ozone platform that uses surfaceless drawing.
// Drawing and displaying happens directly through NativePixmap buffers. CC
// would call into SurfaceFactoryOzone to allocate the buffers and then call
// ScheduleOverlayPlane(..) to schedule the buffer for presentation. The buffer
// presentation happens in such a way that
// WaylandConnection::ScheduleBufferSwap is called with current's widget
// and buffer id provided, and WaylandConnection attaches a needed wl_buffer
// on the browser process side to a right WaylandWindow, which holds the
// provided widget.
class GbmSurfaceless : public gl::SurfacelessEGL {
 public:
  GbmSurfaceless(WaylandSurfaceFactory* surface_factory,
                 gfx::AcceleratedWidget widget);

  void QueueOverlayPlane(const OverlayPlane& plane);

  // gl::GLSurface:
  bool ScheduleOverlayPlane(int z_order,
                            gfx::OverlayTransform transform,
                            gl::GLImage* image,
                            const gfx::Rect& bounds_rect,
                            const gfx::RectF& crop_rect,
                            bool enable_blend,
                            std::unique_ptr<gfx::GpuFence> gpu_fence) override;
  bool IsOffscreen() override;
  bool SupportsPresentationCallback() override;
  bool SupportsAsyncSwap() override;
  bool SupportsPostSubBuffer() override;
  gfx::SwapResult PostSubBuffer(int x,
                                int y,
                                int width,
                                int height,
                                const PresentationCallback& callback) override;
  void SwapBuffersAsync(
      const SwapCompletionCallback& completion_callback,
      const PresentationCallback& presentation_callback) override;
  void PostSubBufferAsync(
      int x,
      int y,
      int width,
      int height,
      const SwapCompletionCallback& completion_callback,
      const PresentationCallback& presentation_callback) override;
  EGLConfig GetConfig() override;

 protected:
  ~GbmSurfaceless() override;

  gfx::AcceleratedWidget widget() { return widget_; }

 private:
  struct PendingFrame {
    PendingFrame();
    ~PendingFrame();

    bool ScheduleOverlayPlanes(gfx::AcceleratedWidget widget);
    void Flush();

    bool ready = false;
    std::vector<gl::GLSurfaceOverlay> overlays;
    using SwapCompletionAndPresentationCallback =
        base::OnceCallback<void(gfx::SwapResult,
                                const gfx::PresentationFeedback&)>;
    SwapCompletionAndPresentationCallback callback;
  };

  void SubmitFrame();

  EGLSyncKHR InsertFence(bool implicit);
  void FenceRetired(PendingFrame* frame);

  void SwapCompleted(const SwapCompletionCallback& completion_callback,
                     const PresentationCallback& presentation_callback,
                     gfx::SwapResult result,
                     const gfx::PresentationFeedback& feedback);

  WaylandSurfaceFactory* surface_factory_;

  // The native surface. Deleting this is allowed to free the EGLNativeWindow.
  gfx::AcceleratedWidget widget_;
  std::vector<std::unique_ptr<PendingFrame>> unsubmitted_frames_;
  std::vector<OverlayPlane> planes_;
  bool has_implicit_external_sync_;
  bool last_swap_buffers_result_ = true;
  bool swap_buffers_pending_ = false;

  base::WeakPtrFactory<GbmSurfaceless> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(GbmSurfaceless);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_GPU_GBM_SURFACELESS_H_
