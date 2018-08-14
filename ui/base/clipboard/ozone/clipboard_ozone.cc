// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/clipboard/ozone/clipboard_ozone.h"

#include <stdint.h>

#include <limits>
#include <list>
#include <memory>
#include <utility>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/clipboard/clipboard_monitor.h"
#include "ui/base/clipboard/custom_data_helper.h"
#include "ui/gfx/geometry/size.h"

#include "base/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/task/post_task.h"
#include "base/timer/timer.h"
#include "ui/ozone/public/clipboard_delegate.h"

namespace ui {

namespace {

const char kMimeTypeFilename[] = "chromium/filename";
// const char kString[] = "STRING";
// const char kText[] = "TEXT";
// const char kTextPlain[] = "text/plain";
// const char kTextPlainUtf8[] = "text/plain;charset=utf-8";
// const char kUtf8String[] = "UTF8_STRING";

}  // namespace

class ClipboardOzone::AsyncClipboardOzone {
 public:
  AsyncClipboardOzone() : weak_factory_(this) {}
  ~AsyncClipboardOzone() {}

  std::string RequestAndWaitForText(ClipboardType type,
                                    const std::string& mime_type) {
    std::unique_ptr<Request> request =
        MakeRequest(RequestType::READ, mime_type, std::string());
    ProcessRequestAndWaitForResult(request.get());

    std::string text;
    if (request->completed) {
      DCHECK(!request->read_data_map.empty());
      text.append(request->read_data_map[mime_type].begin(),
                  request->read_data_map[mime_type].end());
    }
    return text;
  }

  std::vector<std::string> RequestAndWaitForMimeTypes() {
    std::unique_ptr<Request> request =
        MakeRequest(RequestType::GET_MIME, std::string(), std::string());
    ProcessRequestAndWaitForResult(request.get());

    std::vector<std::string> mime_types;
    if (request->completed)
      mime_types = std::move(request->mime_types);
    return mime_types;
  }

  void OfferTextAndWait(const std::string& text, const std::string& mime_type) {
    std::unique_ptr<Request> request =
        MakeRequest(RequestType::OFFER, mime_type, text);
    ProcessRequestAndWaitForResult(request.get());
  }

 private:
  friend class ClipboardOzone;

  enum class RequestType {
    READ = 0,
    OFFER = 1,
    GET_MIME = 2,
    IS_SELECTION_OWNER = 3
  };

  // A structure, which holds request data to process inquiries from
  // the ClipboardOzone.
  struct Request {
    explicit Request(RequestType type) : current_type(type) {}
    ~Request() {}

    // Describes the type of the request.
    RequestType current_type;

    // A closure that is used to signal the request is processed.
    base::Closure request_closure;

    // A data map that stores the result data.
    ClipboardDelegate::DataMap read_data_map;

    // A mime type of the current request.
    std::string mime_type;

    // A data map that is used to offer data.
    ClipboardDelegate::DataMap offer_data_map;

    // A vector of mime types returned as a result to request available mime
    // types.
    std::vector<std::string> mime_types;

    // Tells if the request is completed or not.
    bool completed = false;

    // The time when the request should be aborted.
    base::TimeTicks timeout;
  };

  std::unique_ptr<Request> MakeRequest(RequestType type,
                                       const std::string& mime_type,
                                       const std::string& text) {
    std::unique_ptr<Request> request(new Request(type));
    if (request->current_type == RequestType::READ)
      PrepareReadRequest(mime_type, request.get());
    else if (request->current_type == RequestType::OFFER)
      PrepareOfferRequest(mime_type, text, request.get());
    return request;
  }

  void PrepareOfferRequest(const std::string& mime_type,
                           const std::string& text,
                           Request* request) {
    DCHECK(request);

    std::vector<char> object_map(text.begin(), text.end());
    char* object_data = &object_map.front();

    request->offer_data_map[kMimeTypeText] =
        std::vector<uint8_t>(object_data, object_data + object_map.size());
    request->mime_type = mime_type;
  }

  void PrepareReadRequest(const std::string& mime_type, Request* request) {
    DCHECK(request);
    request->mime_type = mime_type;
  }

  void ProcessRequestAndWaitForResult(Request* request) {
    DCHECK(!abort_timer_.IsRunning());

    base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
    base::TimeTicks timeout =
        base::TimeTicks::Now() + base::TimeDelta::FromMilliseconds(1000);
    request->request_closure = run_loop.QuitClosure();
    request->timeout = timeout;
    pending_request_ = request;

    bool is_processing = false;
    switch (request->current_type) {
      case (RequestType::READ):
        is_processing = ProcessReadRequest(request);
        break;
      case (RequestType::OFFER):
        is_processing = ProcessOfferRequest(request);
        break;
      case (RequestType::GET_MIME):
        is_processing = ProcessGetMimeRequest(request);
        break;
      default:
        NOTREACHED();
        break;
    }

    if (!is_processing || pending_request_->completed)
      return;

    abort_timer_.Start(FROM_HERE, base::TimeDelta::FromMilliseconds(100), this,
                       &AsyncClipboardOzone::AbortStaledRequest);
    LOG(ERROR) << "RUN LOOP";
    run_loop.Run();
    LOG(ERROR) << "AFTER RUN LOOP";
    abort_timer_.Stop();
  }

  void AbortStaledRequest() {
    LOG(ERROR) << "Abort";
    base::TimeTicks now = base::TimeTicks::Now();
    if (pending_request_ && pending_request_->timeout >= now) {
      CompleteRequest(false);
    }
  }

  bool ProcessReadRequest(Request* request) {
    auto callback = base::BindOnce(&AsyncClipboardOzone::OnTextRead,
                                   weak_factory_.GetWeakPtr());
    base::PostTask(FROM_HERE,
                   base::Bind(&ClipboardDelegate::RequestClipboardData,
                              base::Unretained(delegate_),
                              request->mime_type,
                              base::Passed(&callback)));
    return true;
  }

  bool ProcessOfferRequest(Request* request) {
    auto callback = base::BindOnce(&AsyncClipboardOzone::OnOfferDone,
                                   weak_factory_.GetWeakPtr());
    base::PostTask(FROM_HERE, base::Bind(&ClipboardDelegate::OfferClipboardData,
                                         base::Unretained(delegate_),
                                         request->offer_data_map,
                                         base::Passed(&callback)));
    return true;
  }

  bool ProcessGetMimeRequest(Request* request) {
    auto callback = base::BindOnce(&AsyncClipboardOzone::OnGetMime,
                                   weak_factory_.GetWeakPtr());
    base::PostTask(
        FROM_HERE,
        base::Bind(&ClipboardDelegate::GetAvailableMimeTypes,
                   base::Unretained(delegate_), base::Passed(&callback)));
    return true;
  }

  void set_delegate(ClipboardDelegate* delegate) { delegate_ = delegate; }

  void OnTextRead(ClipboardDelegate::DataMap data_map) {
    pending_request_->read_data_map = std::move(data_map);
    CompleteRequest(true);
  }

  void OnOfferDone() { CompleteRequest(true); }

  void OnGetMime(const std::vector<std::string>& mime_types) {
    pending_request_->mime_types = std::move(mime_types);
    CompleteRequest(true);
  }

  void CompleteRequest(bool completed) {
    LOG(ERROR) << "Complete";
    pending_request_->completed = completed;
    std::move(pending_request_->request_closure).Run();
  }

  Request* pending_request_;
  base::RepeatingTimer abort_timer_;

  ClipboardDelegate* delegate_;

  base::WeakPtrFactory<AsyncClipboardOzone> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(AsyncClipboardOzone);
};

// Clipboard::FormatType implementation.
Clipboard::FormatType::FormatType() {
  NOTIMPLEMENTED();
  return;
}

Clipboard::FormatType::FormatType(const std::string& native_format)
    : data_(native_format) {
  NOTIMPLEMENTED();
  return;
}

Clipboard::FormatType::~FormatType() {
  NOTIMPLEMENTED();
  return;
}

std::string Clipboard::FormatType::Serialize() const {
  NOTIMPLEMENTED();
  return data_;
}

// static
Clipboard::FormatType Clipboard::FormatType::Deserialize(
    const std::string& serialization) {
  return FormatType(serialization);
}

bool Clipboard::FormatType::operator<(const FormatType& other) const {
  NOTIMPLEMENTED();
  return data_ < other.data_;
}

bool Clipboard::FormatType::Equals(const FormatType& other) const {
  NOTIMPLEMENTED();
  return data_ == other.data_;
}

///////////////////////////////////////////////////////////////////////////////
// Various predefined FormatTypes.
// static
Clipboard::FormatType Clipboard::GetFormatType(
    const std::string& format_string) {
  return FormatType::Deserialize(format_string);
}

// static
const Clipboard::FormatType& Clipboard::GetUrlFormatType() {
  CR_DEFINE_STATIC_LOCAL(FormatType, type, (kMimeTypeURIList));
  return type;
}

// static
const Clipboard::FormatType& Clipboard::GetUrlWFormatType() {
  return GetUrlFormatType();
}

// static
const Clipboard::FormatType& Clipboard::GetMozUrlFormatType() {
  CR_DEFINE_STATIC_LOCAL(FormatType, type, (kMimeTypeMozillaURL));
  return type;
}

// static
const Clipboard::FormatType& Clipboard::GetPlainTextFormatType() {
  CR_DEFINE_STATIC_LOCAL(FormatType, type, (kMimeTypeText));
  return type;
}

// static
const Clipboard::FormatType& Clipboard::GetPlainTextWFormatType() {
  return GetPlainTextFormatType();
}

// static
const Clipboard::FormatType& Clipboard::GetFilenameFormatType() {
  CR_DEFINE_STATIC_LOCAL(FormatType, type, (kMimeTypeFilename));
  return type;
}

// static
const Clipboard::FormatType& Clipboard::GetFilenameWFormatType() {
  return Clipboard::GetFilenameFormatType();
}

// static
const Clipboard::FormatType& Clipboard::GetHtmlFormatType() {
  CR_DEFINE_STATIC_LOCAL(FormatType, type, (kMimeTypeHTML));
  return type;
}

// static
const Clipboard::FormatType& Clipboard::GetRtfFormatType() {
  CR_DEFINE_STATIC_LOCAL(FormatType, type, (kMimeTypeRTF));
  return type;
}

// static
const Clipboard::FormatType& Clipboard::GetBitmapFormatType() {
  CR_DEFINE_STATIC_LOCAL(FormatType, type, (kMimeTypePNG));
  return type;
}

// static
const Clipboard::FormatType& Clipboard::GetWebKitSmartPasteFormatType() {
  CR_DEFINE_STATIC_LOCAL(FormatType, type, (kMimeTypeWebkitSmartPaste));
  return type;
}

// static
const Clipboard::FormatType& Clipboard::GetWebCustomDataFormatType() {
  CR_DEFINE_STATIC_LOCAL(FormatType, type, (kMimeTypeWebCustomData));
  return type;
}

// static
const Clipboard::FormatType& Clipboard::GetPepperCustomDataFormatType() {
  CR_DEFINE_STATIC_LOCAL(FormatType, type, (kMimeTypePepperCustomData));
  return type;
}

// Clipboard factory method.
Clipboard* Clipboard::Create() {
  NOTIMPLEMENTED();
  return new ClipboardOzone;
}

// ClipboardOzone implementation.
ClipboardOzone::ClipboardOzone() = default;

ClipboardOzone::~ClipboardOzone() = default;

void ClipboardOzone::SetDelegate(ClipboardDelegate* delegate) {
  delegate_ = delegate;
  async_clipboard_ozone_ =
      std::make_unique<ClipboardOzone::AsyncClipboardOzone>();
  async_clipboard_ozone_->set_delegate(delegate_);
}

void ClipboardOzone::OnPreShutdown() {}

// TODO: fix this.
uint64_t ClipboardOzone::GetSequenceNumber(ClipboardType type) const {
  return 0;
}

bool ClipboardOzone::IsFormatAvailable(const FormatType& format,
                                       ClipboardType type) const {
  auto available_types = async_clipboard_ozone_->RequestAndWaitForMimeTypes();
  for (auto mime_type : available_types) {
    if (format.ToString() == mime_type)
      return true;
  }
  return false;
}

void ClipboardOzone::Clear(ClipboardType type) {
  NOTIMPLEMENTED();
  return;
}

void ClipboardOzone::ReadAvailableTypes(ClipboardType type,
                                        std::vector<base::string16>* types,
                                        bool* contains_filenames) const {
  auto available_types = async_clipboard_ozone_->RequestAndWaitForMimeTypes();
  for (auto mime_type : available_types)
    types->push_back(base::UTF8ToUTF16(mime_type));
}

void ClipboardOzone::ReadText(ClipboardType type,
                              base::string16* result) const {
  DCHECK(CalledOnValidThread());
  std::string text =
      async_clipboard_ozone_->RequestAndWaitForText(type, kMimeTypeText);
  *result = base::UTF8ToUTF16(text);
}

void ClipboardOzone::ReadAsciiText(ClipboardType type,
                                   std::string* result) const {
  NOTIMPLEMENTED();
  return;
}

void ClipboardOzone::ReadHTML(ClipboardType type,
                              base::string16* markup,
                              std::string* src_url,
                              uint32_t* fragment_start,
                              uint32_t* fragment_end) const {
  NOTIMPLEMENTED();
  return;
}

void ClipboardOzone::ReadRTF(ClipboardType type, std::string* result) const {
  NOTIMPLEMENTED();
  return;
}

SkBitmap ClipboardOzone::ReadImage(ClipboardType type) const {
  NOTIMPLEMENTED();
  return SkBitmap();
}

void ClipboardOzone::ReadCustomData(ClipboardType clipboard_type,
                                    const base::string16& type,
                                    base::string16* result) const {
  NOTIMPLEMENTED();
  return;
}

void ClipboardOzone::ReadBookmark(base::string16* title,
                                  std::string* url) const {
  DCHECK(CalledOnValidThread());
  NOTIMPLEMENTED();
  return;
}

void ClipboardOzone::ReadData(const FormatType& format,
                              std::string* result) const {
  NOTIMPLEMENTED();
  return;
}

void ClipboardOzone::WriteObjects(ClipboardType type,
                                  const ObjectMap& objects) {
  for (const auto& object : objects)
    DispatchObject(static_cast<ObjectType>(object.first), object.second);
}

void ClipboardOzone::WriteText(const char* text_data, size_t text_len) {
  std::string text(text_data, text_len);
  async_clipboard_ozone_->OfferTextAndWait(text, kMimeTypeText);
}

void ClipboardOzone::WriteHTML(const char* markup_data,
                               size_t markup_len,
                               const char* url_data,
                               size_t url_len) {
  NOTIMPLEMENTED();
  return;
}

void ClipboardOzone::WriteRTF(const char* rtf_data, size_t data_len) {
  NOTIMPLEMENTED();
  return;
}

void ClipboardOzone::WriteBookmark(const char* title_data,
                                   size_t title_len,
                                   const char* url_data,
                                   size_t url_len) {
  NOTIMPLEMENTED();
  return;
}

void ClipboardOzone::WriteWebSmartPaste() {
  NOTIMPLEMENTED();
  return;
}

void ClipboardOzone::WriteBitmap(const SkBitmap& bitmap) {
  NOTIMPLEMENTED();
  return;
}

void ClipboardOzone::WriteData(const FormatType& format,
                               const char* data_data,
                               size_t data_len) {
  NOTIMPLEMENTED();
  return;
}

}  // namespace ui
