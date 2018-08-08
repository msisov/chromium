// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/x11/x11_drag_context.h"

#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/base/dragdrop/os_exchange_data_provider_aurax11_base.h"
#include "ui/base/x/x11_util.h"
#include "ui/base/x/x11_window_event_manager.h"
#include "ui/events/platform/x11/x11_event_source.h"
#include "ui/gfx/x/x11.h"
#include "ui/gfx/x/x11_atom_cache.h"
#include "ui/ozone/platform/x11/x11_drag_util.h"
#include "ui/ozone/platform/x11/x11_window_ozone.h"

namespace ui {

namespace {

constexpr int kWillAcceptDrop = 1;
constexpr int kWantFurtherPosEvents = 2;

// Window property that will receive the drag and drop selection data.
const char kChromiumDragReciever[] = "_CHROMIUM_DRAG_RECEIVER";

}  // namespace

class X11OSExchangeDataProvider : public OSExchangeDataProviderAuraX11Base {
 public:
  X11OSExchangeDataProvider(::Window x_window,
                            const SelectionFormatMap& selection)
      : OSExchangeDataProviderAuraX11Base(x_window, selection) {}

  X11OSExchangeDataProvider() {}

  std::unique_ptr<Provider> Clone() const override {
    std::unique_ptr<X11OSExchangeDataProvider> ret(
        new X11OSExchangeDataProvider());
    ret->format_map_ = format_map_;
    return std::move(ret);
  }
};

X11DragContext::X11DragContext(X11WindowOzone* window,
                               XID local_window,
                               const XClientMessageEvent& event,
                               const SelectionFormatMap* map)
    : window_(window),
      local_window_(local_window),
      source_window_(event.data.l[0]),
      waiting_to_handle_position_(false),
      suggested_action_(x11::None) {
  if (!map) {
    bool get_types_from_property = ((event.data.l[1] & 1) != 0);

    if (get_types_from_property) {
      if (!ui::GetAtomArrayProperty(source_window_, kXdndTypeList,
                                    &unfetched_targets_)) {
        return;
      }
    } else {
      // data.l[2,3,4] contain the first three types. Unused slots can be None.
      for (int i = 0; i < 3; ++i) {
        if (event.data.l[2 + i] != x11::None) {
          unfetched_targets_.push_back(event.data.l[2 + i]);
        }
      }
    }

#if DCHECK_IS_ON()
    DVLOG(1) << "XdndEnter has " << unfetched_targets_.size() << " data types";
    for (::Atom target : unfetched_targets_) {
      DVLOG(1) << "XdndEnter data type: " << target;
    }
#endif  // DCHECK_IS_ON()

    // The window doesn't have a DesktopDragDropClientAura, that means it's
    // created by some other process. Listen for messages on it.
    source_window_events_.reset(
        new ui::XScopedEventSelector(source_window_, PropertyChangeMask));

    // We must perform a full sync here because we could be racing
    // |source_window_|.
    XSync(gfx::GetXDisplay(), x11::False);
  } else {
    // This drag originates from an aura window within our process. This means
    // that we can shortcut the X11 server and ask the owning SelectionOwner
    // for the data it's offering.
    fetched_targets_ = *map;
  }

  ReadActions();
}

X11DragContext::~X11DragContext() = default;

void X11DragContext::OnXdndPosition(const XClientMessageEvent& event) {
  unsigned long source_window = event.data.l[0];
  int x_root_window = event.data.l[2] >> 16;
  int y_root_window = event.data.l[2] & 0xffff;
  ::Time time_stamp = event.data.l[3];
  ::Atom suggested_action = event.data.l[4];

  OnXdndPositionMessage(suggested_action, source_window, time_stamp,
                        gfx::PointF(x_root_window, y_root_window));
}

void X11DragContext::OnXdndPositionMessage(::Atom suggested_action,
                                           XID source_window,
                                           ::Time time_stamp,
                                           const gfx::PointF& screen_point) {
  DCHECK_EQ(source_window_, source_window);
  suggested_action_ = suggested_action;

  if (!unfetched_targets_.empty()) {
    // We have unfetched targets. That means we need to pause the handling of
    // the position message and ask the other window for its data.
    screen_point_ = screen_point;
    position_time_stamp_ = time_stamp;
    waiting_to_handle_position_ = true;

    fetched_targets_ = ui::SelectionFormatMap();
    RequestNextTarget();
  } else {
    CompleteXdndPosition(source_window, screen_point);
  }
}

void X11DragContext::RequestNextTarget() {
  DCHECK(!unfetched_targets_.empty());
  DCHECK(waiting_to_handle_position_);

  ::Atom target = unfetched_targets_.back();
  unfetched_targets_.pop_back();

  XConvertSelection(gfx::GetXDisplay(), gfx::GetAtom(kXdndSelection), target,
                    gfx::GetAtom(kChromiumDragReciever), local_window_,
                    position_time_stamp_);
}

void X11DragContext::OnSelectionNotify(const XSelectionEvent& event) {
  if (!waiting_to_handle_position_) {
    // A misbehaved window may send SelectionNotify without us requesting data
    // via XConvertSelection().
    return;
  }

  DVLOG(1) << "SelectionNotify, format " << event.target;

  if (event.property != x11::None) {
    DCHECK_EQ(event.property, gfx::GetAtom(kChromiumDragReciever));

    scoped_refptr<base::RefCountedMemory> data;
    ::Atom type = x11::None;
    if (ui::GetRawBytesOfProperty(local_window_, event.property, &data, NULL,
                                  &type)) {
      fetched_targets_.Insert(event.target, data);
    }
  } else {
    // The source failed to convert the drop data to the format (target in X11
    // parlance) that we asked for. This happens, even though we only ask for
    // the formats advertised by the source. http://crbug.com/628099
    DVLOG(1) << "XConvertSelection failed for source-advertised target "
             << event.target;
  }

  if (!unfetched_targets_.empty()) {
    RequestNextTarget();
  } else {
    waiting_to_handle_position_ = false;
    CompleteXdndPosition(source_window_, screen_point_);
  }
}

void X11DragContext::ReadActions() {
  std::vector<::Atom> atom_array;
  if (!ui::GetAtomArrayProperty(source_window_, kXdndActionList, &atom_array)) {
    actions_.clear();
  } else {
    actions_.swap(atom_array);
  }
}

int X11DragContext::GetDragOperation() const {
  int drag_operation = ui::DragDropTypes::DRAG_NONE;
  for (std::vector<::Atom>::const_iterator it = actions_.begin();
       it != actions_.end(); ++it) {
    MaskOperation(*it, &drag_operation);
  }

  MaskOperation(suggested_action_, &drag_operation);

  return drag_operation;
}

void X11DragContext::MaskOperation(::Atom xdnd_operation,
                                   int* drag_operation) const {
  if (xdnd_operation == gfx::GetAtom(kXdndActionCopy))
    *drag_operation |= ui::DragDropTypes::DRAG_COPY;
  else if (xdnd_operation == gfx::GetAtom(kXdndActionMove))
    *drag_operation |= ui::DragDropTypes::DRAG_MOVE;
  else if (xdnd_operation == gfx::GetAtom(kXdndActionLink))
    *drag_operation |= ui::DragDropTypes::DRAG_LINK;
}

void X11DragContext::CompleteXdndPosition(XID source_window,
                                          const gfx::PointF& screen_point) {
  // int drag_operation = ui::DragDropTypes::DRAG_COPY;
  std::unique_ptr<OSExchangeData> data = std::make_unique<OSExchangeData>(
      std::make_unique<X11OSExchangeDataProvider>(local_window_,
                                                  fetched_targets()));
  int drag_operation = GetDragOperation();
  // KDE-based file browsers such as Dolphin change the drag operation depending
  // on whether alt/ctrl/shift was pressed. However once Chromium gets control
  // over the X11 events, the source application does no longer receive X11
  // events for key modifier changes, so the dnd operation gets stuck in an
  // incorrect state. Blink can only dnd-open files of type DRAG_COPY, so the
  // DRAG_COPY mask is added if the dnd object is a file.
  if (drag_operation &
          (ui::DragDropTypes::DRAG_MOVE | ui::DragDropTypes::DRAG_LINK) &&
      data->HasFile()) {
    drag_operation |= ui::DragDropTypes::DRAG_COPY;
  }

  if (!sent_entered_) {
    window_->OnDragDataCollected(screen_point, std::move(data), drag_operation);
    sent_entered_ = true;
  }
  window_->OnDragMotion(screen_point, 0, position_time_stamp_, drag_operation);

  // Sends an XdndStatus message back to the source_window. l[2,3]
  // theoretically represent an area in the window where the current action is
  // the same as what we're returning, but I can't find any implementation that
  // actually making use of this. A client can return (0, 0) and/or set the
  // first bit of l[1] to disable the feature, and it appears that gtk neither
  // sets this nor respects it if set.
  XEvent xev;
  xev.xclient.type = ClientMessage;
  xev.xclient.message_type = gfx::GetAtom(kXdndStatus);
  xev.xclient.format = 32;
  xev.xclient.window = source_window;
  xev.xclient.data.l[0] = local_window_;
  xev.xclient.data.l[1] =
      (drag_operation != 0) ? (kWantFurtherPosEvents | kWillAcceptDrop) : 0;
  xev.xclient.data.l[2] = 0;
  xev.xclient.data.l[3] = 0;
  xev.xclient.data.l[4] = DragOperationToAtom(drag_operation);

  ui::SendXClientEvent(source_window, &xev);
}

void X11DragContext::OnXdndDrop(int drag_operation) {
  XEvent xev;
  xev.xclient.type = ClientMessage;
  xev.xclient.message_type = gfx::GetAtom(kXdndFinished);
  xev.xclient.format = 32;
  xev.xclient.window = source_window();
  xev.xclient.data.l[0] = local_window_;
  xev.xclient.data.l[1] = (drag_operation != 0) ? 1 : 0;
  xev.xclient.data.l[2] = DragOperationToAtom(drag_operation);

  ui::SendXClientEvent(source_window(), &xev);
}

}  // namespace ui
