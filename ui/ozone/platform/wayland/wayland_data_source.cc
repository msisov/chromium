// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/wayland_data_source.h"

#include "ui/ozone/platform/wayland/wayland_connection.h"

namespace ui {

WaylandDataSource::WaylandDataSource(wl_data_source* data_source)
    : data_source_(data_source) {
  static const struct wl_data_source_listener kDataSourceListener = {
      WaylandDataSource::OnTarget, WaylandDataSource::OnSend,
      WaylandDataSource::OnCancel};
  wl_data_source_add_listener(data_source, &kDataSourceListener, this);
}

WaylandDataSource::~WaylandDataSource() = default;

void WaylandDataSource::WriteToClipboard(
    const std::vector<std::string>& mime_types) {
  for (const auto& mime_type : mime_types) {
    wl_data_source_offer(data_source_.get(), mime_type.c_str());
    if (strcmp(mime_type.c_str(), "text/plain") == 0)
      wl_data_source_offer(data_source_.get(), "text/plain;charset=utf-8");
  }
  wl_data_device_set_selection(connection_->data_device(), data_source_.get(),
                               connection_->serial());
  wl_display_flush(connection_->display());
}

// static
void WaylandDataSource::OnTarget(void* data,
                                 wl_data_source* source,
                                 const char* mime_type) {
  NOTIMPLEMENTED();
}

// static
void WaylandDataSource::OnSend(void* data,
                               wl_data_source* source,
                               const char* mime_type,
                               int32_t fd) {
  WaylandDataSource* self = static_cast<WaylandDataSource*>(data);
  base::Optional<std::vector<uint8_t>> mime_data;
  self->connection_->GetClipboardData(mime_type, &mime_data);
  if (!mime_data.has_value() &&
      strcmp(mime_type, "text/plain;charset=utf-8") == 0)
    self->connection_->GetClipboardData("text/plain", &mime_data);

  std::string str(mime_data->begin(), mime_data->end());
  if (write(fd, str.data(), str.length()) < 0)
    LOG(ERROR) << "write failed: " << fd;

  close(fd);
}

// static
void WaylandDataSource::OnCancel(void* data, wl_data_source* source) {
  WaylandDataSource* self = static_cast<WaylandDataSource*>(data);
  self->connection_->DataSourceCancelled();
}

}  // namespace ui
