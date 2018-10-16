// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/clipboard/clipboard.h"

namespace ui {

constexpr char kClipboard[] = "CLIPBOARD";
constexpr char kClipboardManager[] = "CLIPBOARD_MANAGER";
constexpr char kMimeTypeFilename[] = "chromium/filename";

Clipboard::FormatType::FormatType() {}

Clipboard::FormatType::FormatType(const std::string& native_format)
    : data_(native_format) {}

Clipboard::FormatType::~FormatType() {}

std::string Clipboard::FormatType::Serialize() const {
  return data_;
}

// static
Clipboard::FormatType Clipboard::FormatType::Deserialize(
    const std::string& serialization) {
  return FormatType(serialization);
}

bool Clipboard::FormatType::operator<(const FormatType& other) const {
  return data_ < other.data_;
}

bool Clipboard::FormatType::Equals(const FormatType& other) const {
  return data_ == other.data_;
}

// Various predefined FormatTypes.
// static
Clipboard::FormatType Clipboard::GetFormatType(
    const std::string& format_string) {
  return FormatType::Deserialize(format_string);
}

// static
const Clipboard::FormatType& Clipboard::GetUrlFormatType() {
  static base::NoDestructor<FormatType> type(kMimeTypeURIList);
  return *type;
}

// static
const Clipboard::FormatType& Clipboard::GetUrlWFormatType() {
  return GetUrlFormatType();
}

// static
const Clipboard::FormatType& Clipboard::GetMozUrlFormatType() {
  static base::NoDestructor<FormatType> type(kMimeTypeMozillaURL);
  return *type;
}

// static
const Clipboard::FormatType& Clipboard::GetPlainTextFormatType() {
  static base::NoDestructor<FormatType> type(kMimeTypeText);
  return *type;
}

// static
const Clipboard::FormatType& Clipboard::GetPlainTextWFormatType() {
  return GetPlainTextFormatType();
}

// static
const Clipboard::FormatType& Clipboard::GetFilenameFormatType() {
  static base::NoDestructor<FormatType> type(kMimeTypeFilename);
  return *type;
}

// static
const Clipboard::FormatType& Clipboard::GetFilenameWFormatType() {
  return Clipboard::GetFilenameFormatType();
}

// static
const Clipboard::FormatType& Clipboard::GetHtmlFormatType() {
  static base::NoDestructor<FormatType> type(kMimeTypeHTML);
  return *type;
}

// static
const Clipboard::FormatType& Clipboard::GetRtfFormatType() {
  static base::NoDestructor<FormatType> type(kMimeTypeRTF);
  return *type;
}

// static
const Clipboard::FormatType& Clipboard::GetBitmapFormatType() {
  static base::NoDestructor<FormatType> type(kMimeTypePNG);
  return *type;
}

// static
const Clipboard::FormatType& Clipboard::GetWebKitSmartPasteFormatType() {
  static base::NoDestructor<FormatType> type(kMimeTypeWebkitSmartPaste);
  return *type;
}

// static
const Clipboard::FormatType& Clipboard::GetWebCustomDataFormatType() {
  static base::NoDestructor<FormatType> type(kMimeTypeWebCustomData);
  return *type;
}

// static
const Clipboard::FormatType& Clipboard::GetPepperCustomDataFormatType() {
  static base::NoDestructor<FormatType> type(kMimeTypePepperCustomData);
  return *type;
}

}  // namespace ui
