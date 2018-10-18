// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/selection_utils_linux.h"

#include <stdint.h>

#include <set>

#include "base/i18n/icu_string_conversions.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/gfx/x/x11_atom_cache.h"

namespace ui {

namespace {
const char kString[] = "STRING";
const char kText[] = "TEXT";
const char kTextPlain[] = "text/plain";
const char kTextPlainUtf8[] = "text/plain;charset=utf-8";
const char kUtf8String[] = "UTF8_STRING";
}  // namespace

std::vector<std::string> GetTextMimesFrom() {
  std::vector<std::string> mimes;
  mimes.push_back(kUtf8String);
  mimes.push_back(kString);
  mimes.push_back(kText);
  mimes.push_back(kTextPlain);
  mimes.push_back(kTextPlainUtf8);
  return mimes;
}

std::vector<std::string> GetURLMimesFrom() {
  std::vector<std::string> mimes;
  mimes.push_back(Clipboard::kMimeTypeURIList);
  mimes.push_back(Clipboard::kMimeTypeMozillaURL);
  return mimes;
}

std::vector<std::string> GetURIListMimesFrom() {
  std::vector<std::string> mimes;
  mimes.push_back(Clipboard::kMimeTypeURIList);
  return mimes;
}

void GetMimesIntersection(const std::vector<std::string>& desired,
                          const std::vector<std::string>& offered,
                          std::vector<std::string>* output) {
  for (auto it = desired.begin(); it != desired.end(); ++it) {
    if (base::ContainsValue(offered, *it))
      output->push_back(*it);
  }
}

void AddString16ToVector(const base::string16& str,
                         std::vector<unsigned char>* bytes) {
  const unsigned char* front =
      reinterpret_cast<const unsigned char*>(str.data());
  bytes->insert(bytes->end(), front, front + (str.size() * 2));
}

std::vector<std::string> ParseURIList(const SelectionData& data) {
  // uri-lists are newline separated file lists in URL encoding.
  std::string unparsed;
  data.AssignTo(&unparsed);
  return base::SplitString(unparsed, "\n", base::KEEP_WHITESPACE,
                           base::SPLIT_WANT_NONEMPTY);
}

std::string RefCountedMemoryToString(
    const scoped_refptr<base::RefCountedMemory>& memory) {
  if (!memory.get()) {
    NOTREACHED();
    return std::string();
  }

  size_t size = memory->size();
  if (!size)
    return std::string();

  const unsigned char* front = memory->front();
  return std::string(reinterpret_cast<const char*>(front), size);
}

base::string16 RefCountedMemoryToString16(
    const scoped_refptr<base::RefCountedMemory>& memory) {
  if (!memory.get()) {
    NOTREACHED();
    return base::string16();
  }

  size_t size = memory->size();
  if (!size)
    return base::string16();

  const unsigned char* front = memory->front();
  return base::string16(reinterpret_cast<const base::char16*>(front), size / 2);
}

///////////////////////////////////////////////////////////////////////////////

SelectionFormatMap::SelectionFormatMap() {}

SelectionFormatMap::SelectionFormatMap(const SelectionFormatMap& other) =
    default;

SelectionFormatMap::~SelectionFormatMap() {}

void SelectionFormatMap::Insert(
    std::string type,
    const scoped_refptr<base::RefCountedMemory>& item) {
  data_.erase(type);
  data_.insert(std::make_pair(type, item));
}

ui::SelectionData SelectionFormatMap::GetFirstOf(
    const std::vector<std::string>& requested_types) const {
  for (auto it = requested_types.begin(); it != requested_types.end(); ++it) {
    auto data_it = data_.find(*it);
    if (data_it != data_.end()) {
      return SelectionData(data_it->first, data_it->second);
    }
  }

  return SelectionData();
}

std::vector<std::string> SelectionFormatMap::GetTypes() const {
  std::vector<std::string> mime_types;
  for (const auto& data_item : data_)
    mime_types.push_back(data_item.first);

  return mime_types;
}

///////////////////////////////////////////////////////////////////////////////

SelectionData::SelectionData() : type_(std::string()) {}

SelectionData::SelectionData(
    std::string type,
    const scoped_refptr<base::RefCountedMemory>& memory)
    : type_(type), memory_(memory) {}

SelectionData::SelectionData(const SelectionData& rhs)
    : type_(rhs.type_), memory_(rhs.memory_) {}

SelectionData::~SelectionData() {}

SelectionData& SelectionData::operator=(const SelectionData& rhs) {
  type_ = rhs.type_;
  memory_ = rhs.memory_;
  // TODO(erg): In some future where we have to support multiple X Displays,
  // the following will also need to deal with the display.
  return *this;
}

bool SelectionData::IsValid() const {
  return type_ != std::string();
}

std::string SelectionData::GetType() const {
  return type_;
}

const unsigned char* SelectionData::GetData() const {
  return memory_.get() ? memory_->front() : NULL;
}

size_t SelectionData::GetSize() const {
  return memory_.get() ? memory_->size() : 0;
}

std::string SelectionData::GetText() const {
  if (type_ == kUtf8String || type_ == kText || type_ == kTextPlainUtf8) {
    return RefCountedMemoryToString(memory_);
  } else if (type_ == kString || type_ == kTextPlain) {
    std::string result;
    base::ConvertToUtf8AndNormalize(RefCountedMemoryToString(memory_),
                                    base::kCodepageLatin1, &result);
    return result;
  } else {
    // BTW, I looked at COMPOUND_TEXT, and there's no way we're going to
    // support that. Yuck.
    NOTREACHED();
    return std::string();
  }
}

base::string16 SelectionData::GetHtml() const {
  base::string16 markup;

  if (type_ == Clipboard::kMimeTypeHTML) {
    const unsigned char* data = GetData();
    size_t size = GetSize();

    // If the data starts with 0xFEFF, i.e., Byte Order Mark, assume it is
    // UTF-16, otherwise assume UTF-8.
    if (size >= 2 && reinterpret_cast<const uint16_t*>(data)[0] == 0xFEFF) {
      markup.assign(reinterpret_cast<const uint16_t*>(data) + 1,
                    (size / 2) - 1);
    } else {
      base::UTF8ToUTF16(reinterpret_cast<const char*>(data), size, &markup);
    }

    // If there is a terminating NULL, drop it.
    if (!markup.empty() && markup.at(markup.length() - 1) == '\0')
      markup.resize(markup.length() - 1);

    return markup;
  } else {
    NOTREACHED();
    return markup;
  }
}

void SelectionData::AssignTo(std::string* result) const {
  *result = RefCountedMemoryToString(memory_);
}

void SelectionData::AssignTo(base::string16* result) const {
  *result = RefCountedMemoryToString16(memory_);
}

}  // namespace ui
