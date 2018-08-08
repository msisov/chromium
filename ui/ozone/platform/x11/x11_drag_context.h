// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_X11_X11_DRAG_CONTEXT_H_
#define UI_OZONE_PLATFORM_X11_X11_DRAG_CONTEXT_H_

#include "ui/base/x/selection_utils.h"
#include "ui/gfx/x/x11.h"
#include "ui/gfx/x/x11_atom_cache.h"
#include "ui/gfx/geometry/point_f.h"

namespace ui {

class XScopedEventSelector;
class X11WindowOzone;

class X11DragContext {
 public:
  X11DragContext(X11WindowOzone* window,
                 XID local_window,
                 const XClientMessageEvent& event,
                 const SelectionFormatMap* map);
  ~X11DragContext();

  void OnXdndPosition(const XClientMessageEvent& event);
  // When we receive an XdndPosition message, we need to have all the data
  // copied from the other window before we process the XdndPosition
  // message. If we have that data already, dispatch immediately. Otherwise,
  // delay dispatching until we do.
  void OnXdndPositionMessage(::Atom suggested_action,
                             XID source_window,
                             ::Time time_stamp,
                             const gfx::PointF& screen_point);

  // Called when XSelection data has been copied to our process.
  void OnSelectionNotify(const XSelectionEvent& xselection);

  void OnXdndDrop(int drag_operation);

  // Clones the fetched targets.
  const ui::SelectionFormatMap& fetched_targets() { return fetched_targets_; }

  // Reads the kXdndActionList property from |source_window| and copies it
  // into |actions|.
  void ReadActions();

  // Creates a ui::DragDropTypes::DragOperation representation of the current
  // action list.
  int GetDragOperation() const;

  // views::DesktopDragDropClientOzone* source_client() { return source_client_; }

  XID source_window() { return source_window_; }

 private:
  // Called to request the next target from the source window. This is only
  // done on the first XdndPosition; after that, we cache the data offered by
  // the source window.
  void RequestNextTarget();

  // Masks the X11 atom |xdnd_operation|'s views representation onto
  // |drag_operation|.
  void MaskOperation(::Atom xdnd_operation, int* drag_operation) const;

  void CompleteXdndPosition(::Window source_window,
                            const gfx::PointF& screen_point);

  void SendXClientEvent(XID xid, XEvent* xev);

  X11WindowOzone* window_;

  // The XID of our chrome local aura window handling our events.
  XID local_window_;

  // The XID of the window that's initiated the drag.
  XID source_window_;

  // Events that we have selected on |source_window_|.
  std::unique_ptr<ui::XScopedEventSelector> source_window_events_;

  // Whether we're blocking the handling of an XdndPosition message by waiting
  // for |unfetched_targets_| to be fetched.
  bool waiting_to_handle_position_;

  // Where the cursor is on screen.
  gfx::PointF screen_point_;

  // The time stamp of the last XdndPosition event we received. The XDND
  // specification mandates that we use this time stamp when querying the source
  // about the drag and drop data.
  ::Time position_time_stamp_;

  // A SelectionFormatMap of data that we have in our process.
  ui::SelectionFormatMap fetched_targets_;

  // The names of various data types offered by the other window that we
  // haven't fetched and put in |fetched_targets_| yet.
  std::vector<::Atom> unfetched_targets_;

  // XdndPosition messages have a suggested action. Qt applications exclusively
  // use this, instead of the XdndActionList which is backed by |actions_|.
  ::Atom suggested_action_;

  // Possible actions.
  std::vector<::Atom> actions_;

  bool sent_entered_ = false;

  DISALLOW_COPY_AND_ASSIGN(X11DragContext);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_X11_X11_DRAG_CONTEXT_H_
