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

  void GetMimeTypes(GetAvailableMimeTypesCallback callback) const {
    // If we do not "own" the selection, it means we need to query the system
    // for the available clipboard data.
    if (delegate_ && !delegate_->IsSelectionOwner()) {
      auto closure = base::BindOnce(std::move(callback), sequence_number_);
      delegate_->GetAvailableMimeTypes(std::move(closure));
      return;
    }

    std::vector<std::string> types(data_types_.size());
    int i = 0;
    for (auto it = data_types_.begin(); it != data_types_.end(); ++it, ++i)
      types[i] = it->first;

    std::move(callback).Run(sequence_number(), types);
  }

  void SetData(const base::Optional<DataMap>& data,
               WriteClipboardDataCallback callback) {
    sequence_number_++;
    data_types_ = data.value_or(DataMap());

    if (delegate_) {
      std::vector<std::string> types(data_types_.size());
      int i = 0;
      for (auto it : data_types_)
        types[i++] = it.first;

      auto closure = base::BindOnce(std::move(callback), sequence_number_);
      delegate_->WriteToWMClipboard(types, std::move(closure));
      return;
    }

    std::move(callback).Run(sequence_number());
  }

  void GetData(const std::string& mime_type,
               ReadClipboardDataCallback callback) const {
    uint64_t sequence = sequence_number();

    // Read from system clipboard first.
    if (delegate_ && !delegate_->IsSelectionOwner()) {
      auto closure = base::BindOnce(std::move(callback), sequence);
      delegate_->ReadFromWMClipboard(mime_type, std::move(closure));
      return;
    }

    base::Optional<std::vector<uint8_t>> data;
    auto it = data_types_.find(mime_type);
    if (it != data_types_.end())
      data.emplace(it->second);
    std::move(callback).Run(sequence, std::move(data));
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
  clipboard_state_[clipboard_num]->GetMimeTypes(std::move(callback));
}

void ClipboardImpl::ReadClipboardData(Clipboard::Type clipboard_type,
                                      const std::string& mime_type,
                                      ReadClipboardDataCallback callback) {
  int clipboard_num = static_cast<int>(clipboard_type);
  clipboard_state_[clipboard_num]->GetData(mime_type, std::move(callback));
}

void ClipboardImpl::WriteClipboardData(Clipboard::Type clipboard_type,
                                       const base::Optional<DataMap>& data,
                                       WriteClipboardDataCallback callback) {
  int clipboard_num = static_cast<int>(clipboard_type);
  clipboard_state_[clipboard_num]->SetData(data, std::move(callback));
}

}  // namespace clipboard
}  // namespace ui
