// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/wayland_data_offer.h"

#include <algorithm>
#include <fcntl.h>

#include "base/logging.h"

namespace ui {

namespace {

const char kString[] = "STRING";
const char kText[] = "TEXT";
const char kTextPlain[] = "text/plain";
const char kTextPlainUtf8[] = "text/plain;charset=utf-8";
const char kUtf8String[] = "UTF8_STRING";

}  // namespace

WaylandDataOffer::WaylandDataOffer(wl_data_offer* data_offer)
    : data_offer_(data_offer) {
  static const struct wl_data_offer_listener kDataOfferListener = {
      WaylandDataOffer::OnOffer};
  wl_data_offer_add_listener(data_offer, &kDataOfferListener, this);
}

WaylandDataOffer::~WaylandDataOffer() {
  data_offer_.reset();
}

void WaylandDataOffer::EnsureTextMimeTypeIfNeeded() {
  if (std::find(mime_types_.begin(), mime_types_.end(), kTextPlain) !=
      mime_types_.end())
    return;

  if (std::any_of(mime_types_.begin(), mime_types_.end(),
                  [](const std::string& mime_type) {
                    return mime_type == kString || mime_type == kText ||
                           mime_type == kTextPlainUtf8 ||
                           mime_type == kUtf8String;
                  })) {
    mime_types_.push_back(kTextPlain);
    text_plain_mime_type_inserted_ = true;
  }
}

int WaylandDataOffer::Receive(const std::string& mime_type) {
  if (std::find(mime_types_.begin(), mime_types_.end(), mime_type) ==
      mime_types_.end())
    return -1;

  int pipefd[2];
  if (pipe2(pipefd, O_CLOEXEC) == -1) {
    LOG(ERROR) << "Failed to create pipe: " << strerror(errno);
    return -1;
  }

  // If we needed to forcibly write "text/plain" as an available
  // mimetype, then it is safer to "read" the clipboard data with
  // a mimetype mime_type known to be available.
  std::string effective_mime_type = mime_type;
  if (mime_type == kTextPlain && text_plain_mime_type_inserted_) {
    effective_mime_type = "text/plain;charset=utf-8";
  }

  wl_data_offer_receive(data_offer_.get(), effective_mime_type.data(),
                        pipefd[1]);
  close(pipefd[1]);

  return pipefd[0];
}

// static
void WaylandDataOffer::OnOffer(void* data,
                               wl_data_offer* data_offer,
                               const char* mime_type) {
  auto* self = static_cast<WaylandDataOffer*>(data);
  self->mime_types_.push_back(mime_type);
}

}  // namespace ui
