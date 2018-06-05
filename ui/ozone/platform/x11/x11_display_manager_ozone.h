// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_X11_X11_DISPLAY_MANAGER_OZONE_H_
#define UI_OZONE_PLATFORM_X11_X11_DISPLAY_MANAGER_OZONE_H_

#include <stdint.h>

#include "ui/display/types/native_display_delegate.h"
#include "ui/events/platform/platform_event_dispatcher.h"

typedef unsigned long XID;
typedef XID Window;
typedef struct _XDisplay Display;

namespace display {
class DisplayMode;
class DisplaySnapshot;
}  // namespace display

namespace ui {

// X11DisplayManagerOzone talks to xrandr.
class X11DisplayManagerOzone : public ui::PlatformEventDispatcher {
 public:
  class Observer {
   public:
    // Will be called when X11DisplayManagerOzone is available.
    virtual void OnOutputReadyForUse() = 0;
  };

  X11DisplayManagerOzone();
  ~X11DisplayManagerOzone() override;

  void SetObserver(Observer* observer);
  Observer* observer() { return observer_; }

  void GetDisplaysSnapshot(display::GetDisplaysCallback callback);

  // ui::PlatformEventDispatcher:
  bool CanDispatchEvent(const ui::PlatformEvent& event) override;
  uint32_t DispatchEvent(const ui::PlatformEvent& event) override;

 private:
  // Builds a list of displays from the current screen information offered by
  // the X server.
  void BuildDisplaysFromXRandRInfo();

  ::Display* xdisplay_;
  ::Window x_root_window_;

  // XRandR version. MAJOR * 100 + MINOR. Zero if no xrandr is present.
  int xrandr_version_;

  // The base of the event numbers used to represent XRandr events used in
  // decoding events regarding output add/remove.
  int xrandr_event_base_;

  // The display objects we present to chrome.
  std::vector<std::unique_ptr<display::DisplaySnapshot>> snapshots_;

  // The index into displays_ that represents the primary display.
  size_t primary_display_index_;

  Observer* observer_;

  DISALLOW_COPY_AND_ASSIGN(X11DisplayManagerOzone);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_X11_X11_DISPLAY_MANAGER_OZONE_H_
