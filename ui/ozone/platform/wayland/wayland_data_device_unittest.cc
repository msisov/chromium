// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <wayland-server.h>

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/ozone/platform/wayland/fake_server.h"
#include "ui/ozone/platform/wayland/wayland_test.h"
#include "ui/ozone/public/clipboard_data_bridge.h"

namespace ui {

class MockClipboardClient {
 public:
  MockClipboardClient()
      : clipboard_data_bridge_(new ClipboardDataBridge(data_types_)) {}
  ~MockClipboardClient() = default;

  // Fill the clipboard backing store with sample data.
  void SetData(const std::string& utf8_text,
               const std::string& mime_type,
               SetDataCallback callback) {
    std::vector<char> object_map(utf8_text.begin(), utf8_text.end());
    char* object_data = &object_map.front();
    data_types_[mime_type] =
        std::vector<uint8_t>(object_data, object_data + object_map.size());

    std::vector<std::string> mime_types;
    mime_types.push_back(mime_type);

    delegate_->WriteToWMClipboard(mime_types, std::move(callback));
  }

  void ReadData(const std::string& mime_type, GetDataCallback callback) {
    delegate_->ReadFromWMClipboard(mime_type, std::move(callback));
  }

 private:
  friend class WaylandDataDeviceManagerTest;
  std::unique_ptr<ClipboardDataBridge> clipboard_data_bridge_;
  ClipboardDelegate* delegate_ = nullptr;
  ClipboardDataBridge::DataMap data_types_;

  DISALLOW_COPY_AND_ASSIGN(MockClipboardClient);
};

class WaylandDataDeviceManagerTest : public WaylandTest {
 public:
  WaylandDataDeviceManagerTest() {}

  void SetUp() override {
    WaylandTest::SetUp();

    Sync();

    data_device_manager_ = server_.data_device_manager();
    DCHECK(data_device_manager_);

    DCHECK(connection_);
    connection_->SetupClipboardDataBridge(
        clipboard_client_.clipboard_data_bridge_.get(),
        &clipboard_client_.delegate_);
    DCHECK(clipboard_client_.delegate_);
  }

 protected:
  wl::MockDataDeviceManager* data_device_manager_;
  MockClipboardClient clipboard_client_;

  DISALLOW_COPY_AND_ASSIGN(WaylandDataDeviceManagerTest);
};

TEST_P(WaylandDataDeviceManagerTest, WriteToClipboard) {
  // The client writes data to the clipboard ...
  auto callback = base::BindOnce([]() {});
  clipboard_client_.SetData(wl::kSampleClipboardText, wl::kTextMimeTypeUtf8,
                            std::move(callback));
  Sync();

  // ... and the server reads it.
  data_device_manager_->data_source()->ReadData(
      base::BindOnce([](const std::vector<uint8_t>& data) {
        std::string string_data(data.begin(), data.end());
        EXPECT_EQ(wl::kSampleClipboardText, string_data);
      }));
  Sync();
}

TEST_P(WaylandDataDeviceManagerTest, ReadFromClibpard) {
  // TODO: implement this in terms of an actual wl_surface that gets
  // focused and compositor sends data_device data to it.
  auto* data_offer = data_device_manager_->data_device()->OnDataOffer();
  data_offer->OnOffer(wl::kTextMimeTypeUtf8);
  data_device_manager_->data_device()->OnSelection(*data_offer);
  Sync();

  // The client requests to read clipboard data from the server.
  // The Server writes in some sample data, and we check it matches
  // expectation.
  auto callback =
      base::BindOnce([](const base::Optional<std::vector<uint8_t>>& data) {
        std::string string_data = std::string(data->begin(), data->end());
        EXPECT_EQ(wl::kSampleClipboardText, string_data);
      });
  clipboard_client_.ReadData(wl::kTextMimeTypeUtf8, std::move(callback));
  Sync();
}

INSTANTIATE_TEST_CASE_P(XdgVersionV6Test,
                        WaylandDataDeviceManagerTest,
                        ::testing::Values(kXdgShellV6));
}  // namespace ui
