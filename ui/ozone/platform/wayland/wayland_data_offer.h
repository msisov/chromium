// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_WAYLAND_DATA_OFFER_H_
#define UI_OZONE_PLATFORM_WAYLAND_WAYLAND_DATA_OFFER_H_

#include <wayland-client.h>

#include <string>
#include <vector>

#include "base/macros.h"
#include "ui/ozone/platform/wayland/wayland_object.h"

namespace ui {

// The WaylandDataOffer represents copy-and-paste or (in the future)
// drag-and-drop data sent to us by some Wayland client (possibly ourself).
class WaylandDataOffer {
 public:
  // Takes ownership of data_offer.
  explicit WaylandDataOffer(wl_data_offer* data_offer);
  ~WaylandDataOffer();

  const std::vector<std::string>& GetAvailableMimeTypes() const {
    return mime_types_;
  }

  // Some applications X11 applications on Gnome/Wayland (running through
  // XWayland) to not send "text/plain" mime that Chrome relios on. When
  // it happens, this method forcibly inserts "text/plain" to the list of
  // provided mime types so that Chrome clipboard's machinery works fine.
  void EnsureTextMimeTypeIfNeeded();

  // Receive data of type mime_type from another Wayland client. Returns
  // an open file descriptor to read the data from, or -1 on failure.
  int Receive(const std::string& mime_type);

 private:
  // wl_data_offer_listener callbacks.
  static void OnOffer(void* data,
                      wl_data_offer* data_offer,
                      const char* mime_type);

  wl::Object<wl_data_offer> data_offer_;
  std::vector<std::string> mime_types_;

  bool text_plain_mime_type_inserted_ = false;

  DISALLOW_COPY_AND_ASSIGN(WaylandDataOffer);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_WAYLAND_DATA_OFFER_H_
