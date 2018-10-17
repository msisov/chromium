// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/clipboard/clipboard_ozone.h"

#include <stdint.h>

#include <limits>
#include <list>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/timer/timer.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/clipboard/clipboard_monitor.h"
#include "ui/base/clipboard/custom_data_helper.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/geometry/size.h"
#include "ui/ozone/public/clipboard_delegate.h"

namespace ui {

// A helper class, which uses a request pattern to asynchronously communicate
// with the ozone::ClipboardDelegate and fetch clipboard data with mimes
// specified.
class ClipboardOzone::AsyncClipboardOzone {
 public:
  AsyncClipboardOzone() : weak_factory_(this) {}

  ~AsyncClipboardOzone() = default;

  void set_delegate(ClipboardDelegate* delegate) {
    DCHECK(!delegate_);
    delegate_ = delegate;
  }

  std::vector<uint8_t> ReadClipboardDataAndWait(
      ClipboardType type,
      const Clipboard::FormatType& mime_type) {
    if (type == ClipboardType::CLIPBOARD_TYPE_SELECTION)
      return std::vector<uint8_t>();

    // We can use a fastpath if we are the owner of the selection.
    const std::string mime_type_str = mime_type.ToString();
    if (delegate_->IsSelectionOwner()) {
      DCHECK_EQ(type, ClipboardType::CLIPBOARD_TYPE_COPY_PASTE);
      return offered_data_.find(mime_type_str) != offered_data_.end()
                 ? offered_data_[mime_type_str]
                 : std::vector<uint8_t>();
    }

    std::unique_ptr<Request> request =
        std::make_unique<Request>(RequestType::READ);
    request->requested_mime_type = mime_type_str;
    ProcessRequestAndWaitForResult(request.get());
    return !request->data_map.empty() ? request->data_map[mime_type_str]
                                      : std::vector<uint8_t>();
  }

  std::vector<std::string> RequestMimeTypes() {
    std::unique_ptr<Request> request =
        std::make_unique<Request>(RequestType::GET_MIME);
    ProcessRequestAndWaitForResult(request.get());
    return std::move(request->mime_types);
  }

  void OfferData() {
    std::unique_ptr<Request> request =
        std::make_unique<Request>(RequestType::OFFER);
    request->data_map = offered_data_;
    ProcessRequestAndWaitForResult(request.get());
  }

  void InsertData(std::vector<uint8_t> data,
                  const Clipboard::FormatType& mime_type) {
    offered_data_[mime_type.ToString()] = std::move(data);
  }

  void ClearOfferedData() { offered_data_.clear(); }

 private:
  enum class RequestType {
    READ = 0,
    OFFER = 1,
    GET_MIME = 2,
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

    // A data map that stores the result data or offers clipboard data.
    ClipboardDelegate::DataMap data_map;

    // A requested mime type of the current request.
    std::string requested_mime_type;

    // A vector of mime types returned as a result to request available mime
    // types.
    std::vector<std::string> mime_types;

    // The time when the request should be aborted.
    base::TimeTicks timeout;
  };

  void ProcessRequestAndWaitForResult(Request* request) {
    DCHECK(!abort_timer_.IsRunning());
    DCHECK(!pending_request_);

    base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
    base::TimeTicks timeout =
        base::TimeTicks::Now() + base::TimeDelta::FromMilliseconds(1000);
    request->request_closure = run_loop.QuitClosure();
    request->timeout = timeout;

    pending_request_ = request;
    switch (pending_request_->current_type) {
      case (RequestType::READ):
        ProcessReadRequest(request);
        break;
      case (RequestType::OFFER):
        ProcessOfferRequest(request);
        break;
      case (RequestType::GET_MIME):
        ProcessGetMimeRequest(request);
        break;
      default:
        NOTREACHED();
        break;
    }

    if (!pending_request_)
      return;
    abort_timer_.Start(FROM_HERE, base::TimeDelta::FromMilliseconds(100), this,
                       &AsyncClipboardOzone::AbortStaledRequest);
    run_loop.Run();
  }

  void AbortStaledRequest() {
    base::TimeTicks now = base::TimeTicks::Now();
    if (pending_request_ && pending_request_->timeout >= now)
      std::move(pending_request_->request_closure).Run();
  }

  void ProcessReadRequest(Request* request) {
    auto callback = base::BindOnce(&AsyncClipboardOzone::OnTextRead,
                                   weak_factory_.GetWeakPtr());
    DCHECK(delegate_);
    delegate_->RequestClipboardData(request->requested_mime_type,
                                    &request->data_map, std::move(callback));
  }

  void ProcessOfferRequest(Request* request) {
    auto callback = base::BindOnce(&AsyncClipboardOzone::OnOfferDone,
                                   weak_factory_.GetWeakPtr());
    DCHECK(delegate_);
    delegate_->OfferClipboardData(request->data_map, std::move(callback));
  }

  void ProcessGetMimeRequest(Request* request) {
    auto callback = base::BindOnce(&AsyncClipboardOzone::OnGotMimeTypes,
                                   weak_factory_.GetWeakPtr());
    DCHECK(delegate_);
    delegate_->GetAvailableMimeTypes(std::move(callback));
  }

  void OnTextRead(const base::Optional<std::vector<uint8_t>>& data) {
    CompleteRequest();
  }

  void OnOfferDone() { CompleteRequest(); }

  void OnGotMimeTypes(const std::vector<std::string>& mime_types) {
    pending_request_->mime_types = std::move(mime_types);
    CompleteRequest();
  }

  void CompleteRequest() {
    abort_timer_.Stop();
    auto closure = std::move(pending_request_->request_closure);
    pending_request_ = nullptr;
    std::move(closure).Run();
  }

  // Cached clipboard data, which is pending to be written. Must be cleared on
  // every new write to a clipboard.
  ClipboardDelegate::DataMap offered_data_;

  Request* pending_request_ = nullptr;

  base::RepeatingTimer abort_timer_;

  ClipboardDelegate* delegate_ = nullptr;

  base::WeakPtrFactory<AsyncClipboardOzone> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(AsyncClipboardOzone);
};

// Clipboard factory method.
Clipboard* Clipboard::Create() {
  return new ClipboardOzone;
}

void ClipboardOzone::SetDelegate(ClipboardDelegate* delegate) {
  DCHECK(async_clipboard_ozone_);
  async_clipboard_ozone_->set_delegate(delegate);
}

// ClipboardOzone implementation.
ClipboardOzone::ClipboardOzone() {
  async_clipboard_ozone_ =
      std::make_unique<ClipboardOzone::AsyncClipboardOzone>();
}

ClipboardOzone::~ClipboardOzone() = default;

void ClipboardOzone::OnPreShutdown() {}

uint64_t ClipboardOzone::GetSequenceNumber(ClipboardType type) const {
  // TODO(msisov): https://crbug.com/875168
  NOTIMPLEMENTED();
  return 0;
}

bool ClipboardOzone::IsFormatAvailable(const FormatType& format,
                                       ClipboardType type) const {
  DCHECK(CalledOnValidThread());
  // TODO(msisov): implement DCHECK(IsSupportedClipboardType(type));
  auto available_types = async_clipboard_ozone_->RequestMimeTypes();
  for (auto mime_type : available_types) {
    if (format.ToString() == mime_type) {
      return true;
    }
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
  DCHECK(CalledOnValidThread());
  types->clear();

  auto available_types = async_clipboard_ozone_->RequestMimeTypes();
  for (auto mime_type : available_types)
    types->push_back(base::UTF8ToUTF16(mime_type));
}

void ClipboardOzone::ReadText(ClipboardType type,
                              base::string16* result) const {
  DCHECK(CalledOnValidThread());
  auto clipboard_data = async_clipboard_ozone_->ReadClipboardDataAndWait(
      type, GetPlainTextFormatType());
  *result = base::UTF8ToUTF16(
      std::string(clipboard_data.begin(), clipboard_data.end()));
}

void ClipboardOzone::ReadAsciiText(ClipboardType type,
                                   std::string* result) const {
  DCHECK(CalledOnValidThread());
  auto clipboard_data = async_clipboard_ozone_->ReadClipboardDataAndWait(
      type, GetPlainTextFormatType());
  result->assign(clipboard_data.begin(), clipboard_data.end());
}

void ClipboardOzone::ReadHTML(ClipboardType type,
                              base::string16* markup,
                              std::string* src_url,
                              uint32_t* fragment_start,
                              uint32_t* fragment_end) const {
  DCHECK(CalledOnValidThread());
  markup->clear();
  if (src_url)
    src_url->clear();
  *fragment_start = 0;
  *fragment_end = 0;

  auto clipboard_data = async_clipboard_ozone_->ReadClipboardDataAndWait(
      type, GetHtmlFormatType());
  *markup = base::UTF8ToUTF16(
      std::string(clipboard_data.begin(), clipboard_data.end()));
  DCHECK(markup->length() <= std::numeric_limits<uint32_t>::max());
  *fragment_end = static_cast<uint32_t>(markup->length());
}

void ClipboardOzone::ReadRTF(ClipboardType type, std::string* result) const {
  DCHECK(CalledOnValidThread());
  auto clipboard_data = async_clipboard_ozone_->ReadClipboardDataAndWait(
      type, GetRtfFormatType());
  result->assign(clipboard_data.begin(), clipboard_data.end());
}

SkBitmap ClipboardOzone::ReadImage(ClipboardType type) const {
  DCHECK(CalledOnValidThread());
  auto clipboard_data = async_clipboard_ozone_->ReadClipboardDataAndWait(
      type, GetBitmapFormatType());
  SkBitmap bitmap;
  if (gfx::PNGCodec::Decode(&clipboard_data.front(), clipboard_data.size(),
                            &bitmap))
    return SkBitmap(bitmap);
  return SkBitmap();
}

void ClipboardOzone::ReadCustomData(ClipboardType clipboard_type,
                                    const base::string16& type,
                                    base::string16* result) const {
  DCHECK(CalledOnValidThread());
  auto custom_data = async_clipboard_ozone_->ReadClipboardDataAndWait(
      clipboard_type, GetWebCustomDataFormatType());
  ui::ReadCustomDataForType(&custom_data.front(), custom_data.size(), type,
                            result);
}

void ClipboardOzone::ReadBookmark(base::string16* title,
                                  std::string* url) const {
  DCHECK(CalledOnValidThread());
  // TODO(msisov): This was left NOTIMPLEMENTED() in all the Linux platforms.
  NOTIMPLEMENTED();
}

void ClipboardOzone::ReadData(const FormatType& format,
                              std::string* result) const {
  DCHECK(CalledOnValidThread());
  auto clipboard_data = async_clipboard_ozone_->ReadClipboardDataAndWait(
      ClipboardType::CLIPBOARD_TYPE_COPY_PASTE, format);
  result->assign(clipboard_data.begin(), clipboard_data.end());
}

void ClipboardOzone::WriteObjects(ClipboardType type,
                                  const ObjectMap& objects) {
  DCHECK(CalledOnValidThread());
  if (type == ClipboardType::CLIPBOARD_TYPE_COPY_PASTE) {
    async_clipboard_ozone_->ClearOfferedData();

    for (const auto& object : objects)
      DispatchObject(static_cast<ObjectType>(object.first), object.second);

    async_clipboard_ozone_->OfferData();
  }
}

void ClipboardOzone::WriteText(const char* text_data, size_t text_len) {
  std::vector<uint8_t> data(text_data, text_data + text_len);
  async_clipboard_ozone_->InsertData(std::move(data), GetPlainTextFormatType());
}

void ClipboardOzone::WriteHTML(const char* markup_data,
                               size_t markup_len,
                               const char* url_data,
                               size_t url_len) {
  std::vector<uint8_t> data(markup_data, markup_data + markup_len);
  async_clipboard_ozone_->InsertData(std::move(data), GetHtmlFormatType());
}

void ClipboardOzone::WriteRTF(const char* rtf_data, size_t data_len) {
  std::vector<uint8_t> data(rtf_data, rtf_data + data_len);
  async_clipboard_ozone_->InsertData(std::move(data), GetRtfFormatType());
}

void ClipboardOzone::WriteBookmark(const char* title_data,
                                   size_t title_len,
                                   const char* url_data,
                                   size_t url_len) {
  // Writes a Mozilla url (UTF16: URL, newline, title)
  base::string16 bookmark =
      base::UTF8ToUTF16(base::StringPiece(url_data, url_len)) +
      base::ASCIIToUTF16("\n") +
      base::UTF8ToUTF16(base::StringPiece(title_data, title_len));

  std::vector<uint8_t> data(
      reinterpret_cast<const uint8_t*>(bookmark.data()),
      reinterpret_cast<const uint8_t*>(bookmark.data() + bookmark.size()));
  async_clipboard_ozone_->InsertData(std::move(data), GetMozUrlFormatType());
}

void ClipboardOzone::WriteWebSmartPaste() {
  async_clipboard_ozone_->InsertData(std::vector<uint8_t>(),
                                     GetWebKitSmartPasteFormatType());
}

void ClipboardOzone::WriteBitmap(const SkBitmap& bitmap) {
  std::vector<unsigned char> output;
  if (gfx::PNGCodec::FastEncodeBGRASkBitmap(bitmap, false, &output))
    async_clipboard_ozone_->InsertData(std::move(output),
                                       GetBitmapFormatType());
}

void ClipboardOzone::WriteData(const FormatType& format,
                               const char* data_data,
                               size_t data_len) {
  std::vector<uint8_t> data(data_data, data_data + data_len);
  async_clipboard_ozone_->InsertData(std::vector<uint8_t>(), format);
}

}  // namespace ui
