// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/clipboard/clipboard_impl.h"

#include <string.h>
#include <utility>

#include "base/macros.h"
#include "ui/ozone/public/clipboard_data_bridge.h"
#include "ui/ozone/public/ozone_platform.h"

namespace ui {
namespace clipboard {

using DataMap = std::unordered_map<std::string, std::vector<uint8_t>>;

// ClipboardData contains data copied to the Clipboard for a variety of formats.
// It mostly just provides APIs to cleanly access and manipulate this data.
class ClipboardImpl::ClipboardData {
 public:
  ClipboardData() : sequence_number_(0) {}
  ~ClipboardData() {}

  uint64_t sequence_number() const {
    return sequence_number_;
  }

  std::vector<std::string> GetMimeTypes() const {
    std::vector<std::string> types(data_types_.size());

    // If there is no mime types cached, it means we need to query the system
    // clipboard for data.
    if (!types.size() && delegate_) {
        for (const auto& mime_type : delegate_->GetAvailableMimeTypes())
          types.push_back(mime_type);
      return types;
    }

    int i = 0;
    for (auto it = data_types_.begin(); it != data_types_.end(); ++it, ++i)
      types[i] = it->first;

    return types;
  }

  void SetData(const base::Optional<DataMap>& data) {
    sequence_number_++;
    data_types_ = data.value_or(DataMap());

    if (delegate_)
      delegate_->WriteToWMClipboard(GetMimeTypes());
  }

  void GetData(const std::string& mime_type,
               base::Optional<std::vector<uint8_t>>* data) const {
    // Read from system clipboard first.
    if (delegate_ && delegate_->ShouldGetClipboardDataFromWMClipboard())
      delegate_->ReadFromWMClipboard(mime_type);

    auto it = data_types_.find(mime_type);
    if (it != data_types_.end())
      data->emplace(it->second);
  }

  void SetupClipboardDataBridge() {
    clipboard_data_bridge_.reset(new ClipboardDataBridge(data_types_));
    OzonePlatform::GetInstance()->SetupClipboardDataBridge(
        clipboard_data_bridge_.get(), &delegate_);
  }

 private:
  uint64_t sequence_number_;
  DataMap data_types_;

  std::unique_ptr<ClipboardDataBridge> clipboard_data_bridge_;
  ClipboardDelegate* delegate_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(ClipboardData);
};

ClipboardImpl::ClipboardImpl() {
  for (int i = 0; i < kNumClipboards; ++i)
    clipboard_state_[i].reset(new ClipboardData);

#if defined(OS_LINUX) && defined(USE_OZONE) && !defined(OS_CHROMEOS)
  clipboard_state_[static_cast<int>(Clipboard::Type::COPY_PASTE)]->
      SetupClipboardDataBridge();
#endif
}

ClipboardImpl::~ClipboardImpl() {
}

void ClipboardImpl::AddBinding(mojom::ClipboardRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

void ClipboardImpl::GetSequenceNumber(Clipboard::Type clipboard_type,
                                      GetSequenceNumberCallback callback) {
  std::move(callback).Run(
      clipboard_state_[static_cast<int>(clipboard_type)]->sequence_number());
}

void ClipboardImpl::GetAvailableMimeTypes(
    Clipboard::Type clipboard_type,
    GetAvailableMimeTypesCallback callback) {
  int clipboard_num = static_cast<int>(clipboard_type);
  std::move(callback).Run(clipboard_state_[clipboard_num]->sequence_number(),
                          clipboard_state_[clipboard_num]->GetMimeTypes());
}

void ClipboardImpl::ReadClipboardData(Clipboard::Type clipboard_type,
                                      const std::string& mime_type,
                                      ReadClipboardDataCallback callback) {
  int clipboard_num = static_cast<int>(clipboard_type);
  base::Optional<std::vector<uint8_t>> mime_data;
  uint64_t sequence = clipboard_state_[clipboard_num]->sequence_number();
  clipboard_state_[clipboard_num]->GetData(mime_type, &mime_data);
  std::move(callback).Run(sequence, std::move(mime_data));
}

void ClipboardImpl::WriteClipboardData(Clipboard::Type clipboard_type,
                                       const base::Optional<DataMap>& data,
                                       WriteClipboardDataCallback callback) {
  int clipboard_num = static_cast<int>(clipboard_type);
  clipboard_state_[clipboard_num]->SetData(data);
  std::move(callback).Run(clipboard_state_[clipboard_num]->sequence_number());
}

}  // namespace clipboard
}  // namespace ui
