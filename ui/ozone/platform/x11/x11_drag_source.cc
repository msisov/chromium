// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/x11/x11_drag_source.h"

#include "base/threading/thread_task_runner_handle.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/base/x/x11_window_event_manager.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/events/platform/scoped_event_dispatcher.h"
#include "ui/ozone/platform/x11/x11_drag_util.h"
#include "ui/ozone/platform/x11/x11_window_ozone.h"

namespace ui {

namespace {

// The time to wait since sending the last XdndPosition message before
// reprocessing the most recent mouse move event in case that the window
// stacking order has changed and |source_current_window_| needs to be updated.
const int kRepeatMouseMoveTimeoutMs = 350;

// The time to wait for the target to respond after the user has released the
// mouse button before ending the move loop.
const int kEndMoveLoopTimeoutMs = 1000;

XID ValidateXdndWindow(XID window) {
  if (window == x11::None)
    return x11::None;

  // TODO(crbug/651775): The proxy window should be reported separately from the
  //     target window. XDND messages should be sent to the proxy, and their
  //     window field should point to the target.

  // Figure out which window we should test as XdndAware. If |target| has
  // XdndProxy, it will set that proxy on target, and if not, |target|'s
  // original value will remain.
  ui::GetXIDProperty(window, kXdndProxy, &window);

  int version;
  if (ui::GetIntProperty(window, kXdndAware, &version) &&
      version >= kMaxXdndVersion) {
    return window;
  }
  return x11::None;
}

}  // namespace

X11DragSource::X11DragSource(X11WindowOzone* window,
                             XID xwindow,
                             int operation,
                             const ui::OSExchangeData& data)
    : window_(window),
      xwindow_(xwindow),
      drag_operation_(operation),
      negotiated_operation_(ui::DragDropTypes::DRAG_NONE),
      source_state_(SOURCE_STATE_OTHER),
      format_map_(),
      selection_owner_(gfx::GetXDisplay(),
                       xwindow,
                       gfx::GetAtom(kXdndSelection)),
      screen_point_(gfx::Point()),
      weak_factory_(this) {
  XStoreName(gfx::GetXDisplay(), xwindow, "Chromium Drag & Drop Window");
  CreateDragInputWindow(gfx::GetXDisplay());

  base::string16 str;
  const OSExchangeData::FilenameToURLPolicy policy =
      OSExchangeData::FilenameToURLPolicy::DO_NOT_CONVERT_FILENAMES;
  if (data.HasURL(policy)) {
    GURL url;
    base::string16 title;
    data.GetURLAndTitle(policy, &url, &title);
    InsertURLToSelectionFormatMap(url, title, &format_map_);
  }
  if (data.GetString(&str)) {
    InsertStringToSelectionFormatMap(str, &format_map_);
  }
  selection_owner_.TakeOwnershipOfSelection(format_map_);

  old_dispatcher_ = std::move(nested_dispatcher_);
  nested_dispatcher_ =
      ui::PlatformEventSource::GetInstance()->OverrideDispatcher(this);
}

X11DragSource::~X11DragSource() {
  window_ = nullptr;
  last_motion_in_screen_.reset();

  if (source_current_window_ != gfx::kNullAcceleratedWidget) {
    SendXdndLeave(source_current_window_);
    source_current_window_ = gfx::kNullAcceleratedWidget;
  }
  repeat_mouse_move_timer_.Stop();
  end_move_loop_timer_.Stop();

  grab_input_window_events_.reset();
  XDestroyWindow(gfx::GetXDisplay(), grab_input_window_);
  grab_input_window_ = gfx::kNullAcceleratedWidget;

  nested_dispatcher_ = std::move(old_dispatcher_);
}

void X11DragSource::OnXdndStatus(const XClientMessageEvent& event) {
  DVLOG(1) << "OnXdndStatus";

  gfx::AcceleratedWidget source_window = event.data.l[0];

  if (source_window != source_current_window_)
    return;

  if (source_state_ != SOURCE_STATE_PENDING_DROP &&
      source_state_ != SOURCE_STATE_OTHER) {
    return;
  }

  waiting_on_status_ = false;
  status_received_since_enter_ = true;

  if (event.data.l[1] & 1) {
    ::Atom atom_operation = event.data.l[4];
    negotiated_operation_ = AtomToDragOperation(atom_operation);
  } else {
    negotiated_operation_ = ui::DragDropTypes::DRAG_NONE;
  }

  if (source_state_ == SOURCE_STATE_PENDING_DROP) {
    // We were waiting on the status message so we could send the XdndDrop.
    if (negotiated_operation_ == ui::DragDropTypes::DRAG_NONE) {
      FinishDragDrop();
      return;
    }
    source_state_ = SOURCE_STATE_DROPPED;
    SendXdndDrop(source_window);
    return;
  }

  ui::CursorType cursor_type = ui::CursorType::kNull;
  switch (negotiated_operation_) {
    case ui::DragDropTypes::DRAG_NONE:
      cursor_type = ui::CursorType::kDndNone;
      break;
    case ui::DragDropTypes::DRAG_MOVE:
      cursor_type = ui::CursorType::kDndMove;
      break;
    case ui::DragDropTypes::DRAG_COPY:
      cursor_type = ui::CursorType::kDndCopy;
      break;
    case ui::DragDropTypes::DRAG_LINK:
      cursor_type = ui::CursorType::kDndLink;
      break;
  }
  // Note: event.data.[2,3] specify a rectangle. It is a request by the other
  // window to not send further XdndPosition messages while the cursor is
  // within it. However, it is considered advisory and (at least according to
  // the spec) the other side must handle further position messages within
  // it. GTK+ doesn't bother with this, so neither should we.

  if (next_position_message_.get()) {
    // We were waiting on the status message so we could send off the next
    // position message we queued up.
    gfx::Point p = next_position_message_->first;
    unsigned long event_time = next_position_message_->second;
    next_position_message_.reset();

    SendXdndPosition(source_window, p, event_time);
  }
}

void X11DragSource::OnXdndFinished(const XClientMessageEvent& event) {
  gfx::AcceleratedWidget source_window = event.data.l[0];
  if (source_current_window_ != source_window)
    return;

  // Clear |negotiated_operation_| if the drag was rejected.
  if ((event.data.l[1] & 1) == 0)
    negotiated_operation_ = ui::DragDropTypes::DRAG_NONE;

  // Clear |source_current_window_| to avoid sending XdndLeave upon ending the
  // move loop.
  source_current_window_ = gfx::kNullAcceleratedWidget;
  FinishDragDrop();
}

void X11DragSource::OnSelectionRequest(const XEvent& event) {
  selection_owner_.OnSelectionRequest(event);
}

DragDropTypes::DragOperation X11DragSource::negotiated_operation() {
  return negotiated_operation_;
}

bool X11DragSource::CanDispatchEvent(const ui::PlatformEvent& event) {
  return true;
}

uint32_t X11DragSource::DispatchEvent(const ui::PlatformEvent& event) {
  DCHECK(base::MessageLoopForUI::IsCurrent());

  // This method processes all events while the move loop is active.
  ui::EventType type = event->type();
  switch (type) {
    case ui::ET_MOUSE_MOVED:
    case ui::ET_MOUSE_DRAGGED: {
      bool dispatch_mouse_event = !last_motion_in_screen_.get();
      last_motion_in_screen_.reset(
          ui::EventFromNative(event).release()->AsMouseEvent());

      last_motion_in_screen_->set_location(
          ui::EventSystemLocationFromNative(event));
      if (dispatch_mouse_event) {
        // Post a task to dispatch mouse movement event when control returns to
        // the message loop. This allows smoother dragging since the events are
        // dispatched without waiting for the drag widget updates.
        base::ThreadTaskRunnerHandle::Get()->PostTask(
            FROM_HERE, base::Bind(&X11DragSource::DispatchMouseMovement,
                                  weak_factory_.GetWeakPtr()));
      }
      return ui::POST_DISPATCH_NONE;
    } break;
    case ui::ET_MOUSE_RELEASED: {
      // TODO: left button checking.
      // Assume that drags are being done with the left mouse button. Only
      // break the drag if the left mouse button was released.
      DispatchMouseMovement();
      HandleMouseRelease();

      if (!grabbed_pointer_) {
        // If the source widget had capture prior to the move loop starting,
        // it may be relying on views::Widget getting the mouse release and
        // releasing capture in Widget::OnMouseEvent().
        return ui::POST_DISPATCH_PERFORM_DEFAULT;
      }
    } break;
    default:
      break;
  }
  return ui::POST_DISPATCH_PERFORM_DEFAULT;
}

void X11DragSource::CreateDragInputWindow(XDisplay* display) {
  unsigned long attribute_mask = CWEventMask | CWOverrideRedirect;
  XSetWindowAttributes swa;
  memset(&swa, 0, sizeof(swa));
  swa.override_redirect = x11::True;
  grab_input_window_ = XCreateWindow(display, DefaultRootWindow(display), -100,
                                     -100, 10, 10, 0, CopyFromParent, InputOnly,
                                     CopyFromParent, attribute_mask, &swa);
  uint32_t event_mask = ButtonPressMask | ButtonReleaseMask |
                        PointerMotionMask | KeyPressMask | KeyReleaseMask |
                        StructureNotifyMask;
  grab_input_window_events_.reset(
      new ui::XScopedEventSelector(grab_input_window_, event_mask));

  XMapRaised(display, grab_input_window_);
}

void X11DragSource::SendXdndEnter(XID dest_window) {
  XEvent xev;
  xev.xclient.type = ClientMessage;
  xev.xclient.message_type = gfx::GetAtom(kXdndEnter);
  xev.xclient.format = 32;
  xev.xclient.window = dest_window;
  xev.xclient.data.l[0] = xwindow_;
  xev.xclient.data.l[1] = (kMaxXdndVersion << 24);  // The version number.
  xev.xclient.data.l[2] = 0;
  xev.xclient.data.l[3] = 0;
  xev.xclient.data.l[4] = 0;

  std::vector<Atom> targets;
  selection_owner_.RetrieveTargets(&targets);

  if (targets.size() > 3) {
    xev.xclient.data.l[1] |= 1;
    ui::SetAtomArrayProperty(xwindow_, kXdndTypeList, "ATOM", targets);
  } else {
    // Pack the targets into the enter message.
    for (size_t i = 0; i < targets.size(); ++i)
      xev.xclient.data.l[2 + i] = targets[i];
  }

  ui::SendXClientEvent(dest_window, &xev);
}

void X11DragSource::SendXdndLeave(XID dest_window) {
  XEvent xev;
  xev.xclient.type = ClientMessage;
  xev.xclient.message_type = gfx::GetAtom(kXdndLeave);
  xev.xclient.format = 32;
  xev.xclient.window = dest_window;
  xev.xclient.data.l[0] = xwindow_;
  xev.xclient.data.l[1] = 0;
  xev.xclient.data.l[2] = 0;
  xev.xclient.data.l[3] = 0;
  xev.xclient.data.l[4] = 0;
  ui::SendXClientEvent(dest_window, &xev);
}

void X11DragSource::SendXdndPosition(XID dest_window,
                                     const gfx::Point& screen_point,
                                     unsigned long event_time) {
  waiting_on_status_ = true;

  XEvent xev;
  xev.xclient.type = ClientMessage;
  xev.xclient.message_type = gfx::GetAtom(kXdndPosition);
  xev.xclient.format = 32;
  xev.xclient.window = dest_window;
  xev.xclient.data.l[0] = xwindow_;
  xev.xclient.data.l[1] = 0;
  xev.xclient.data.l[2] = (screen_point.x() << 16) | screen_point.y();
  xev.xclient.data.l[3] = event_time;
  xev.xclient.data.l[4] = DragOperationToAtom(drag_operation_);
  ui::SendXClientEvent(dest_window, &xev);

  // http://www.whatwg.org/specs/web-apps/current-work/multipage/dnd.html and
  // the Xdnd protocol both recommend that drag events should be sent
  // periodically.
  repeat_mouse_move_timer_.Start(
      FROM_HERE, base::TimeDelta::FromMilliseconds(kRepeatMouseMoveTimeoutMs),
      base::Bind(&X11DragSource::ProcessMouseMove, base::Unretained(this),
                 screen_point, event_time));
}

void X11DragSource::SendXdndDrop(XID dest_window) {
  XEvent xev;
  xev.xclient.type = ClientMessage;
  xev.xclient.message_type = gfx::GetAtom(kXdndDrop);
  xev.xclient.format = 32;
  xev.xclient.window = dest_window;
  xev.xclient.data.l[0] = xwindow_;
  xev.xclient.data.l[1] = 0;
  xev.xclient.data.l[2] = x11::CurrentTime;
  xev.xclient.data.l[3] = x11::None;
  xev.xclient.data.l[4] = x11::None;
  ui::SendXClientEvent(dest_window, &xev);
}

void X11DragSource::DispatchMouseMovement() {
  if (!last_motion_in_screen_)
    return;

  repeat_mouse_move_timer_.Stop();
  base::TimeTicks time_stamp = last_motion_in_screen_->time_stamp();
  ProcessMouseMove(last_motion_in_screen_->location(),
                   (time_stamp - base::TimeTicks()).InMilliseconds());

  last_motion_in_screen_.reset();
}

void X11DragSource::ProcessMouseMove(const gfx::Point& screen_point,
                                     unsigned long event_time) {
  if (source_state_ != SOURCE_STATE_OTHER)
    return;

  // Find the current window the cursor is over.
  gfx::AcceleratedWidget dest_window = gfx::kNullAcceleratedWidget;
  window_->OnMouseMoved(screen_point, &dest_window);
  if (dest_window == gfx::kNullAcceleratedWidget) {
    screen_point_ = screen_point;
    ui::EnumerateTopLevelWindows(this);
    dest_window = ValidateXdndWindow(toplevel_);
  }

  if (source_current_window_ != dest_window) {
    if (source_current_window_ != gfx::kNullAcceleratedWidget)
      SendXdndLeave(source_current_window_);

    source_current_window_ = dest_window;
    waiting_on_status_ = false;
    next_position_message_.reset();
    status_received_since_enter_ = false;
    negotiated_operation_ = ui::DragDropTypes::DRAG_NONE;

    if (source_current_window_ != gfx::kNullAcceleratedWidget)
      SendXdndEnter(source_current_window_);
  }

  if (source_current_window_ != gfx::kNullAcceleratedWidget) {
    if (waiting_on_status_) {
      next_position_message_.reset(
          new std::pair<gfx::Point, unsigned long>(screen_point, event_time));
    } else {
      SendXdndPosition(dest_window, screen_point, event_time);
    }
  }
}

void X11DragSource::StartEndMoveLoopTimer() {
  end_move_loop_timer_.Start(
      FROM_HERE, base::TimeDelta::FromMilliseconds(kEndMoveLoopTimeoutMs), this,
      &X11DragSource::FinishDragDrop);
}

void X11DragSource::FinishDragDrop() {
  window_->OnDragSessionClose(negotiated_operation_);
}

void X11DragSource::HandleMouseRelease() {
  repeat_mouse_move_timer_.Stop();

  if (source_state_ != SOURCE_STATE_OTHER) {
    // The user has previously released the mouse and is clicking in
    // frustration.
    FinishDragDrop();
    return;
  }

  if (source_current_window_ != gfx::kNullAcceleratedWidget) {
    if (waiting_on_status_) {
      if (status_received_since_enter_) {
        // If we are waiting for an XdndStatus message, we need to wait for it
        // to complete.
        source_state_ = SOURCE_STATE_PENDING_DROP;

        // Start timer to end the move loop if the target takes too long to send
        // the XdndStatus and XdndFinished messages.
        StartEndMoveLoopTimer();
        return;
      }

      FinishDragDrop();
      return;
    }

    if (negotiated_operation_ != ui::DragDropTypes::DRAG_NONE) {
      // Start timer to end the move loop if the target takes too long to send
      // an XdndFinished message. It is important that StartEndMoveLoopTimer()
      // is called before SendXdndDrop() because SendXdndDrop()
      // sends XdndFinished synchronously if the drop target is a Chrome
      // window.
      StartEndMoveLoopTimer();

      // We have negotiated an action with the other end.
      source_state_ = SOURCE_STATE_DROPPED;
      SendXdndDrop(source_current_window_);
      return;
    }
  }

  FinishDragDrop();
}

bool X11DragSource::ShouldStopIterating(XID xid) {
  if (!ui::IsWindowVisible(xid))
    return false;

  if (ui::WindowContainsPoint(xid, screen_point_)) {
    toplevel_ = xid;
    return true;
  }
  return false;
}

}  // namespace ui
