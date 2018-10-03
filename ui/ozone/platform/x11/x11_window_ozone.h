// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_X11_X11_WINDOW_OZONE_H_
#define UI_OZONE_PLATFORM_X11_X11_WINDOW_OZONE_H_

#include "base/macros.h"
#include "ui/events/platform/platform_event_dispatcher.h"
#include "ui/events/platform/x11/x11_event_source_libevent.h"
#include "ui/platform_window/platform_window_handler/wm_move_resize_handler.h"
#include "ui/platform_window/x11/window_move_loop_client.h"
#include "ui/platform_window/x11/x11_window_base.h"

namespace ui {

class OSExchangeData;
class X11DragContext;
class X11DragSource;
class X11WindowManagerOzone;

// PlatformWindow implementation for X11 Ozone. PlatformEvents are ui::Events.
class X11WindowOzone : public X11WindowBase,
                       public PlatformEventDispatcher,
                       public XEventDispatcher,
                       public WmMoveResizeHandler {
 public:
  X11WindowOzone(X11WindowManagerOzone* window_manager,
                 PlatformWindowDelegate* delegate,
                 const gfx::Rect& bounds);
  ~X11WindowOzone() override;

  // Called by |window_manager_| once capture is set to another X11WindowOzone.
  void OnLostCapture();

  // PlatformWindow:
  void PrepareForShutdown() override;
  void SetCapture() override;

  void ReleaseCapture() override;
  void SetCursor(PlatformCursor cursor) override;
  void StartDrag(const ui::OSExchangeData& data,
                 int operation,
                 gfx::NativeCursor cursor,
                 base::OnceCallback<void(int)> callback) override;

  bool RunMoveLoop(const gfx::Vector2d& drag_offset) override;
  void StopMoveLoop() override;

  // XEventDispatcher:
  void CheckCanDispatchNextPlatformEvent(XEvent* xev) override;
  void PlatformEventDispatchFinished() override;
  PlatformEventDispatcher* GetPlatformEventDispatcher() override;
  bool DispatchXEvent(XEvent* event) override;

  void OnDragDataCollected(const gfx::PointF& screen_point,
                           std::unique_ptr<OSExchangeData> data,
                           int operation);
  void OnDragMotion(const gfx::PointF& screen_point,
                    int flags,
                    ::Time event_time,
                    int operation);
  void OnMouseMoved(const gfx::Point& point, gfx::AcceleratedWidget* widget);
  void OnDragSessionClose(int dnd_action);

  // WmMoveResizeHandler
  void DispatchHostWindowDragMovement(
      int hittest,
      const gfx::Point& pointer_location) override;

 private:
  enum SourceState {
    // |source_current_window_| will receive a drop once we receive an
    // XdndStatus from it.
    SOURCE_STATE_PENDING_DROP,

    // The move looped will be ended once we receive XdndFinished from
    // |source_current_window_|. We should not send XdndPosition to
    // |source_current_window_| while in this state.
    SOURCE_STATE_DROPPED,

    // There is no drag in progress or there is a drag in progress and the
    // user has not yet released the mouse.
    SOURCE_STATE_OTHER,
  };
  // PlatformEventDispatcher:
  bool CanDispatchEvent(const PlatformEvent& event) override;
  uint32_t DispatchEvent(const PlatformEvent& event) override;
  bool ProcessDragDropEvent(XEvent* xev);

  WmMoveResizeHandler* AsWmMoveResizeHandler();

  X11WindowManagerOzone* window_manager_;

// TODO(msisov, tonikitoo): Add a dummy implementation for chromeos.
#if !defined(OS_CHROMEOS)
  std::unique_ptr<WindowMoveLoopClient> move_loop_client_;
#endif

  // Tells if this dispatcher can process next translated event based on a
  // previous check in ::CheckCanDispatchNextPlatformEvent based on a XID
  // target.
  bool handle_next_event_ = false;
  std::unique_ptr<X11DragContext> target_current_context_;
  // DesktopDragDropClientOzone* drag_drop_client_;
  int drag_operation_;
  // unsigned long source_window_;
  std::unique_ptr<X11DragSource> drag_source_;

  DISALLOW_COPY_AND_ASSIGN(X11WindowOzone);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_X11_X11_WINDOW_OZONE_H_
