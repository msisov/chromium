// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/x11/x11_window_ozone.h"

#include "base/bind.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/base/x/x11_util.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/events/ozone/events_ozone.h"
#include "ui/events/platform/x11/x11_event_source.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/x/x11.h"
#include "ui/ozone/platform/x11/x11_cursor_ozone.h"
#include "ui/ozone/platform/x11/x11_drag_context.h"
#include "ui/ozone/platform/x11/x11_drag_source.h"
#include "ui/ozone/platform/x11/x11_drag_util.h"
#include "ui/ozone/platform/x11/x11_window_manager_ozone.h"

namespace ui {

X11WindowOzone::X11WindowOzone(X11WindowManagerOzone* window_manager,
                               PlatformWindowDelegate* delegate,
                               const gfx::Rect& bounds)
    : X11WindowBase(delegate, bounds),
      window_manager_(window_manager),
      target_current_context_(nullptr) {
  DCHECK(window_manager);
  auto* event_source = X11EventSourceLibevent::GetInstance();
  if (event_source)
    event_source->AddXEventDispatcher(this);

// TODO(msisov, tonikitoo): Add a dummy implementation for chromeos.
#if !defined(OS_CHROMEOS)
  move_loop_client_.reset(new WindowMoveLoopClient());
#endif

  unsigned long xdnd_version = kMaxXdndVersion;
  XChangeProperty(xdisplay(), xwindow(), gfx::GetAtom(kXdndAware), XA_ATOM, 32,
                  PropModeReplace,
                  reinterpret_cast<unsigned char*>(&xdnd_version), 1);
}

X11WindowOzone::~X11WindowOzone() {
  X11WindowOzone::PrepareForShutdown();
}

void X11WindowOzone::PrepareForShutdown() {
  auto* event_source = X11EventSourceLibevent::GetInstance();
  if (event_source)
    event_source->RemoveXEventDispatcher(this);
}

void X11WindowOzone::SetCapture() {
  window_manager_->GrabEvents(this);
}

void X11WindowOzone::ReleaseCapture() {
  window_manager_->UngrabEvents(this);
}

void X11WindowOzone::SetCursor(PlatformCursor cursor) {
  X11CursorOzone* cursor_ozone = static_cast<X11CursorOzone*>(cursor);
  XDefineCursor(xdisplay(), xwindow(), cursor_ozone->xcursor());
}

void X11WindowOzone::StartDrag(const ui::OSExchangeData& data,
                               int operation,
                               gfx::NativeCursor cursor,
                               base::OnceCallback<void(int)> callback) {
  std::vector<::Atom> actions = GetOfferedDragOperations(operation);
  ui::SetAtomArrayProperty(xwindow(), kXdndActionList, "ATOM", actions);

  drag_source_ =
      std::make_unique<X11DragSource>(this, xwindow(), operation, data);
}

bool X11WindowOzone::RunMoveLoop(const gfx::Vector2d& drag_offset) {
// TODO(msisov, tonikitoo): Add a dummy implementation for chromeos.
#if !defined(OS_CHROMEOS)
  DCHECK(move_loop_client_);
  ReleaseCapture();
  return move_loop_client_->RunMoveLoop(this, drag_offset);
#endif
  return true;
}

void X11WindowOzone::StopMoveLoop() {
// TODO(msisov, tonikitoo): Add a dummy implementation for chromeos.
#if !defined(OS_CHROMEOS)
  ReleaseCapture();
  move_loop_client_->EndMoveLoop();
#endif
}

void X11WindowOzone::CheckCanDispatchNextPlatformEvent(XEvent* xev) {
  handle_next_event_ = xwindow() == x11::None ? false : IsEventForXWindow(*xev);
}

void X11WindowOzone::PlatformEventDispatchFinished() {
  handle_next_event_ = false;
}

PlatformEventDispatcher* X11WindowOzone::GetPlatformEventDispatcher() {
  return this;
}

bool X11WindowOzone::DispatchXEvent(XEvent* xev) {
  if (!IsEventForXWindow(*xev))
    return false;

  if (ProcessDragDropEvent(xev))
    return true;

  ProcessXWindowEvent(xev);
  return true;
}

bool X11WindowOzone::CanDispatchEvent(const PlatformEvent& platform_event) {
  bool in_move_loop =
#if !defined(OS_CHROMEOS)
      move_loop_client_->IsInMoveLoop();
#else
      false;
#endif
  return handle_next_event_ || in_move_loop;
}

uint32_t X11WindowOzone::DispatchEvent(const PlatformEvent& event) {
  if (!window_manager_->event_grabber() ||
      window_manager_->event_grabber() == this) {
    // This is unfortunately needed otherwise events that depend on global state
    // (eg. double click) are broken.
    DispatchEventFromNativeUiEvent(
        event, base::BindOnce(&PlatformWindowDelegate::DispatchEvent,
                              base::Unretained(delegate())));
    return POST_DISPATCH_STOP_PROPAGATION;
  }

  if (event->IsLocatedEvent()) {
    // Another X11WindowOzone has installed itself as capture. Translate the
    // event's location and dispatch to the other.
    ConvertEventLocationToTargetWindowLocation(
        window_manager_->event_grabber()->GetBounds().origin(),
        GetBounds().origin(), event->AsLocatedEvent());
  }
  return window_manager_->event_grabber()->DispatchEvent(event);
}

void X11WindowOzone::OnLostCapture() {
  delegate()->OnLostCapture();
}

void X11WindowOzone::OnDragDataCollected(const gfx::PointF& screen_point,
                                         std::unique_ptr<OSExchangeData> data,
                                         int operation) {
  // TODO(jkim).
  // delegate()->OnDragEnter(this, screen_point, std::move(data), operation);
  NOTIMPLEMENTED_LOG_ONCE();
}

void X11WindowOzone::OnMouseMoved(const gfx::Point& point,
                                  gfx::AcceleratedWidget* widget) {
  // TODO(jkim).
  // delegate()->OnMouseMoved(point, widget);
  NOTIMPLEMENTED_LOG_ONCE();
}

void X11WindowOzone::OnDragSessionClose(int dnd_action) {
  // TODO(jkim).
  // drag_source_.reset();
  // delegate()->OnDragSessionClosed(dnd_action);
  NOTIMPLEMENTED_LOG_ONCE();
}

void X11WindowOzone::OnDragMotion(const gfx::PointF& screen_point,
                                  int flags,
                                  ::Time event_time,
                                  int operation) {
  // TODO(jkim).
  // gfx::AcceleratedWidget widget = gfx::kNullAcceleratedWidget;
  // drag_operation_ =
  //    delegate()->OnDragMotion(screen_point, event_time, operation, &widget);
  NOTIMPLEMENTED_LOG_ONCE();
}

bool X11WindowOzone::ProcessDragDropEvent(XEvent* xev) {
  switch (xev->type) {
    case SelectionNotify: {
      if (!target_current_context_.get()) {
        NOTREACHED();
        return false;
      }
      target_current_context_->OnSelectionNotify(xev->xselection);
      return true;
    }
    case PropertyNotify: {
      if (xev->xproperty.atom != gfx::GetAtom(kXdndActionList))
        return false;
      if (!target_current_context_.get() ||
          target_current_context_->source_window() != xev->xany.window) {
        return false;
      }
      target_current_context_->ReadActions();
      return true;
    }
    case SelectionRequest: {
      if (!drag_source_)
        return false;
      drag_source_->OnSelectionRequest(*xev);
      return true;
    }
    case ClientMessage: {
      XClientMessageEvent& event = xev->xclient;
      Atom message_type = event.message_type;
      if (message_type == gfx::GetAtom("WM_PROTOCOLS"))
        return false;

      if (message_type == gfx::GetAtom(kXdndEnter)) {
        int version = (event.data.l[1] & 0xff000000) >> 24;
        if (version < kMinXdndVersion) {
          // This protocol version is not documented in the XDND standard (last
          // revised in 1999), so we don't support it. Since don't understand
          // the protocol spoken by the source, we can't tell it that we can't
          // talk to it.
          LOG(ERROR)
              << "XdndEnter message discarded because its version is too old.";
          return false;
        }
        if (version > kMaxXdndVersion) {
          // The XDND version used should be the minimum between the versions
          // advertised by the source and the target. We advertise
          // kMaxXdndVersion, so this should never happen when talking to an
          // XDND-compliant application.
          LOG(ERROR)
              << "XdndEnter message discarded because its version is too new.";
          return false;
        }
        // Make sure that we've run ~X11DragContext() before creating another
        // one.
        target_current_context_.reset();
        SelectionFormatMap* map = nullptr;
        if (drag_source_)
          map = drag_source_->format_map();
        target_current_context_.reset(
            new X11DragContext(this, xwindow(), event, map));
        return true;
      }
      if (message_type == gfx::GetAtom(kXdndLeave)) {
        // TODO(jkim).
        // delegate()->OnDragLeave();
        NOTIMPLEMENTED_LOG_ONCE();
        return true;
      }
      if (message_type == gfx::GetAtom(kXdndPosition)) {
        if (!target_current_context_.get()) {
          NOTREACHED();
          return false;
        }

        target_current_context_->OnXdndPosition(event);
        return true;
      }
      if (message_type == gfx::GetAtom(kXdndStatus)) {
        drag_source_->OnXdndStatus(event);
        return true;
      }
      if (message_type == gfx::GetAtom(kXdndFinished)) {
        // TODO(jkim).
        // int negotiated_operation = drag_source_->negotiated_operation();
        // drag_source_->OnXdndFinished(event);
        // delegate()->OnDragSessionClosed(negotiated_operation);
        NOTIMPLEMENTED_LOG_ONCE();
        return true;
      }
      if (message_type == gfx::GetAtom(kXdndDrop)) {
        // TODO(jkim).
        // delegate()->OnDragDrop(nullptr);
        return false;

        if (!target_current_context_.get()) {
          NOTREACHED();
          return false;
        }
        target_current_context_->OnXdndDrop(drag_operation_);
        target_current_context_.reset();
        return true;
      }
      break;
    }
    default:
      break;
  }
  return false;
}

}  // namespace ui
