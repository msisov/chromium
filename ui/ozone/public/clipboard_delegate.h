// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PUBLIC_CLIPBOARD_DELEGATE_H_
#define UI_OZONE_PUBLIC_CLIPBOARD_DELEGATE_H_

#include <string>
#include <unordered_map>
#include <vector>

#include "base/macros.h"
#include "base/optional.h"
#include "ui/ozone/ozone_base_export.h"

namespace ui {

// ClipboardDelegate allows clients, eg Mus, to exchange content with
// host system clipboard.
class OZONE_BASE_EXPORT ClipboardDelegate {
 public:
  using DataMap = std::unordered_map<std::string, std::vector<uint8_t>>;

  // Writes 'data_map' contents to the host system clipboard.
  using SetDataCallback = base::OnceCallback<void()>;
  virtual void WriteToWMClipboard(const DataMap& data_map,
                                  SetDataCallback callback) = 0;

  // Reads data from host system clipboard into 'data_map'.
  using GetDataCallback =
      base::OnceCallback<void(const base::Optional<std::vector<uint8_t>>&)>;
  virtual void ReadFromWMClipboard(const std::string& mime_type,
                                   DataMap* data_map,
                                   GetDataCallback callback) = 0;

  // Gets the mime types available in the host system clipboard.
  // They are usually set by the compositor when the window gets focused
  // or clipboard content changes behind the scenes.
  using GetMimeTypesCallback =
      base::OnceCallback<void(const std::vector<std::string>&)>;
  virtual void GetAvailableMimeTypes(GetMimeTypesCallback callback) = 0;

  // Returns 'true' if the active clipboard client on the system is
  // this one.
  virtual bool IsSelectionOwner() = 0;
};

}  // namespace ui

#endif  // UI_OZONE_PUBLIC_CLIPBOARD_DELEGATE_H_
