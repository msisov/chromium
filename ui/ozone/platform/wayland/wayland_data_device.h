// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_WAYLAND_DATA_DEVICE_H_
#define UI_OZONE_PLATFORM_WAYLAND_WAYLAND_DATA_DEVICE_H_

#include <wayland-client.h>
#include <string>

#include "base/macros.h"
#include "base/optional.h"
#include "ui/ozone/platform/wayland/wayland_data_offer.h"
#include "ui/ozone/platform/wayland/wayland_object.h"
#include "ui/ozone/public/clipboard_data_bridge.h"

namespace ui {

class WaylandDataOffer;
class WaylandConnection;

// This class handles copy-and-paste and (in the future) drag-and-drop.
class WaylandDataDevice {
 public:
  WaylandDataDevice(WaylandConnection* connection, wl_data_device* data_device);
  ~WaylandDataDevice();

  void RequestSelectionData(const std::string& mime_type);

  wl_data_device* data_device() { return data_device_.get(); }

  std::vector<std::string> GetAvailableMimeTypes();

 private:
  // wl_data_device_listener callbacks
  static void OnDataOffer(void* data,
                          wl_data_device* data_device,
                          wl_data_offer* id);
  static void OnSelection(void* data,
                          wl_data_device* data_device,
                          wl_data_offer* id);

  // The wl_data_device wrapped by this WaylandDataDevice.
  wl::Object<wl_data_device> data_device_;

  // Used to dispatch events to the browser process.
  WaylandConnection* connection_ = nullptr;

  // There are two separate data offers at a time, the drag offer and the
  // selection offer, each with independent lifetimes. When we receive a new
  // offer, it is not immediately possible to know whether the new offer is the
  // drag offer or the selection offer. This variable is used to store ownership
  // of new data offers temporarily until its identity becomes known.
  std::unique_ptr<WaylandDataOffer> new_offer_;

  // Offer that holds the most-recent clipboard selection, or null if no
  // clipboard data is available.
  std::unique_ptr<WaylandDataOffer> selection_offer_;

  // Local clipboard backing store, that can override client's after reading
  // from system clipboard.
  base::Optional<ClipboardDataBridge::DataMap> current_clipboard_;

  DISALLOW_COPY_AND_ASSIGN(WaylandDataDevice);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_WAYLAND_DATA_DEVICE_H_
