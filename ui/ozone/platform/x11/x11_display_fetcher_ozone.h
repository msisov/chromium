// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_X11_X11_DISPLAY_FETCHER_OZONE_H_
#define UI_OZONE_PLATFORM_X11_X11_DISPLAY_FETCHER_OZONE_H_

#include <stdint.h>

#include "ui/display/types/native_display_delegate.h"
#include "ui/events/platform/platform_event_dispatcher.h"

typedef unsigned long XID;
typedef XID Window;
typedef struct _XDisplay Display;

namespace display {
class Display;
class DisplayMode;
}  // namespace display

namespace ui {

// X11DisplayFetcherOzone talks to xrandr.
class X11DisplayFetcherOzone : public ui::PlatformEventDispatcher {
 public:
  class Delegate {
   public:
    virtual void AddDisplay(const display::Display& display,
                            bool is_primary) = 0;
    virtual void RemoveDisplay(const display::Display& display) = 0;
  };

  explicit X11DisplayFetcherOzone(X11DisplayFetcherOzone::Delegate* observer);
  ~X11DisplayFetcherOzone() override;

  // ui::PlatformEventDispatcher:
  uint32_t DispatchEvent(const ui::PlatformEvent& event) override;

 private:
  // ui::PlatformEventDispatcher:
  bool CanDispatchEvent(const ui::PlatformEvent& event) override;

  // Builds a list of displays from the current screen information offered by
  // the X server.
  std::vector<display::Display> BuildDisplaysFromXRandRInfo();

  int64_t primary_display_index_ = 0;

  ::Display* xdisplay_;
  ::Window x_root_window_;

  // XRandR version. MAJOR * 100 + MINOR. Zero if no xrandr is present.
  int xrandr_version_;

  // The base of the event numbers used to represent XRandr events used in
  // decoding events regarding output add/remove.
  int xrandr_event_base_;

  Delegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(X11DisplayFetcherOzone);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_X11_X11_DISPLAY_FETCHER_OZONE_H_
