// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_X11_X11_DRAG_SOURCE_H_
#define UI_OZONE_PLATFORM_X11_X11_DRAG_SOURCE_H_

#include "base/timer/timer.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/x/selection_owner.h"
#include "ui/base/x/x11_util.h"
#include "ui/events/platform/platform_event_dispatcher.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/native_widget_types.h"

namespace ui {
class MouseEvent;
class ScopedEventDispatcher;
class XScopedEventSelector;
class X11WindowOzone;
class OSExchangeData;

class X11DragSource : public ui::PlatformEventDispatcher,
                      public ui::EnumerateWindowsDelegate {
 public:
  explicit X11DragSource(X11WindowOzone* window,
                         XID xwindow,
                         int operation,
                         const OSExchangeData& data);
  ~X11DragSource() override;

  void OnXdndStatus(const XClientMessageEvent& event);
  void OnXdndFinished(const XClientMessageEvent& event);
  void OnSelectionRequest(const XEvent& event);

  DragDropTypes::DragOperation negotiated_operation();
  SelectionFormatMap* format_map() { return &format_map_; }

  // ui:::PlatformEventDispatcher:
  bool CanDispatchEvent(const ui::PlatformEvent& event) override;
  uint32_t DispatchEvent(const ui::PlatformEvent& event) override;

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

  // Creates an input-only window to be used during the drag.
  void CreateDragInputWindow(XDisplay* display);

  void HandleMouseRelease();

  void FinishDragDrop();

  void SendXdndEnter(XID dest_window);
  void SendXdndLeave(XID dest_window);
  void SendXdndPosition(XID dest_window,
                        const gfx::Point& screen_point,
                        unsigned long event_time);
  void SendXdndDrop(XID dest_window);

  void ProcessMouseMove(const gfx::Point& screen_point,
                        unsigned long event_time);

  // Dispatch mouse movement event to |delegate_| in a posted task.
  void DispatchMouseMovement();

  void StartEndMoveLoopTimer();

  // ui::EnumerateWindowsDelegate:
  bool ShouldStopIterating(XID xid) override;

  // An invisible InputOnly window. Keyboard grab and sometimes mouse grab
  // are set on this window.
  XID grab_input_window_;

  // Events selected on |grab_input_window_|.
  std::unique_ptr<ui::XScopedEventSelector> grab_input_window_events_;

  // Whether the pointer was grabbed on |grab_input_window_|.
  bool grabbed_pointer_;

  X11WindowOzone* window_;

  XID xwindow_;

  std::unique_ptr<MouseEvent> last_motion_in_screen_;
  gfx::AcceleratedWidget source_current_window_;

  // When the mouse is released, we need to wait for the last XdndStatus message
  // only if we have previously received a status message from
  // |source_current_window_|.
  bool status_received_since_enter_;

  // In the Xdnd protocol, we aren't supposed to send another XdndPosition
  // message until we have received a confirming XdndStatus message.
  bool waiting_on_status_;

  // If we would send an XdndPosition message while we're waiting for an
  // XdndStatus response, we need to cache the latest details we'd send.
  std::unique_ptr<std::pair<gfx::Point, unsigned long>> next_position_message_;

  // The operation bitfield as requested by StartDragAndDrop.
  int drag_operation_;

  // We offer the other window a list of possible operations,
  // XdndActionsList. This is the requested action from the other window. This
  // is DRAG_NONE if we haven't sent out an XdndPosition message yet, haven't
  // yet received an XdndStatus or if the other window has told us that there's
  // no action that we can agree on.
  DragDropTypes::DragOperation negotiated_operation_;

  // Reprocesses the most recent mouse move event if the mouse has not moved
  // in a while in case the window stacking order has changed and
  // |source_current_window_| needs to be updated.
  base::OneShotTimer repeat_mouse_move_timer_;

  SourceState source_state_;

  // Ends the move loop if the target is too slow to respond after the mouse is
  // released.
  base::OneShotTimer end_move_loop_timer_;

  std::unique_ptr<ui::ScopedEventDispatcher> nested_dispatcher_;

  std::unique_ptr<ui::ScopedEventDispatcher> old_dispatcher_;

  // A representation of data. This is either passed to us from the other
  // process, or built up through a sequence of Set*() calls. It can be passed
  // to |selection_owner_| when we take the selection.
  SelectionFormatMap format_map_;

  // Takes a snapshot of |format_map_| and offers it to other windows.
  mutable SelectionOwner selection_owner_;

  gfx::Point screen_point_;
  XID toplevel_;

  base::WeakPtrFactory<X11DragSource> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(X11DragSource);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_X11_X11_DRAG_SOURCE_H_
