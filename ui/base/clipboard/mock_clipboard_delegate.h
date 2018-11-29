// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_CLIPBOARD_MOCK_CLIPBOARD_DELEGATE_
#define UI_BASE_CLIPBOARD_MOCK_CLIPBOARD_DELEGATE_

#include "base/callback.h"
#include "ui/ozone/public/clipboard_delegate.h"

namespace ui {

// Delegate class, which mocks out system clipboard.
// TODO(msisov): move the declaration details to .cc file.
class MockClipboardDelegate : public ClipboardDelegate {
  public:
    MockClipboardDelegate() = default;
    ~MockClipboardDelegate() = default;
    
    void OfferClipboardData(const DataMap& data_map,
                                  OfferDataClosure callback) override {
      offered_data_map_ = data_map;
      std::move(callback).Run();
    }

    void RequestClipboardData(const std::string& mime_type,
                                    DataMap* data_map,
                                    RequestDataClosure callback) override {
      auto it = offered_data_map_.find(mime_type);
      DCHECK(it != offered_data_map_.end());
      *data_map = offered_data_map_;
      std::move(callback).Run(std::move(it->second));
    }

    void GetAvailableMimeTypes(GetMimeTypesClosure callback) override {
      std::vector<std::string> mime_types;
      for (const auto& item : offered_data_map_) {
        mime_types.push_back(item.first);
      }
      std::move(callback).Run(std::move(mime_types));
    }

    bool IsSelectionOwner() override {
      return !offered_data_map_.empty();
    }
  private:
    ClipboardDelegate::DataMap offered_data_map_;
};

} // namespace ui

#endif // UI_OZONE_PUBLIC_CLIPBOARD_DELEGATE_H_
