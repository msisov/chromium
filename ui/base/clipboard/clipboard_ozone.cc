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
#include "ui/gfx/geometry/size.h"
#include "ui/ozone/public/clipboard_delegate.h"

namespace ui {

class ClipboardOzone::AsyncClipboardOzone {
 public:
  AsyncClipboardOzone() : weak_factory_(this) {}
  ~AsyncClipboardOzone() {}

  void set_delegate(ClipboardDelegate* delegate) { delegate_ = delegate; }

  std::string RequestTypesAndWait(ClipboardType type,
                                  const std::string& mime_type) {
    if (type == ClipboardType::CLIPBOARD_TYPE_SELECTION)
      return std::string();

    // We can use a fastpath if we are the owner of the selection.
    std::string text;
    if (delegate_->IsSelectionOwner()) {
      if (type == ClipboardType::CLIPBOARD_TYPE_COPY_PASTE &&
          !cached_text_.empty()) {
        DCHECK(cached_text_.find(mime_type) != cached_text_.end());
        text.append(cached_text_[mime_type].begin(),
                    cached_text_[mime_type].end());
      }
      return text;
    }

    std::unique_ptr<Request> request =
        std::make_unique<Request>(RequestType::READ);
    request->requested_mime_type = mime_type;
    ProcessRequestAndWaitForResult(request.get());

    if (!request->data_map.empty()) {
      text.append(request->data_map[mime_type].begin(),
                  request->data_map[mime_type].end());
    }
    return text;
  }

  std::vector<std::string> RequestMimeTypes() {
    std::unique_ptr<Request> request =
        std::make_unique<Request>(RequestType::GET_MIME);
    ProcessRequestAndWaitForResult(request.get());
    return std::move(request->mime_types);
  }

  void OfferData() {
    cached_text_ = std::move(selection_text_);

    std::unique_ptr<Request> request =
        std::make_unique<Request>(RequestType::OFFER);
    request->data_map = cached_text_;
    ProcessRequestAndWaitForResult(request.get());
  }

  template <typename Data>
  void InsertData(const Data& data, const std::string& mime_type) {
    std::vector<char> object_map(data.begin(), data.end());
    char* object_data = &object_map.front();

    selection_text_[mime_type] =
        std::vector<uint8_t>(object_data, object_data + object_map.size());
  }

  void ClearSelectionData() { selection_text_.clear(); }

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

    switch (request->current_type) {
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

  // Cached text, which was written to a system clipboard.
  ClipboardDelegate::DataMap cached_text_;

  // Cached selection text, which is pending to be written.
  ClipboardDelegate::DataMap selection_text_;

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
  LOG(ERROR) << "GET SEQUENCE " << type;
  // TODO(msisov): https://crbug.com/875168
  NOTIMPLEMENTED();
  return 0;
}

bool ClipboardOzone::IsFormatAvailable(const FormatType& format,
                                       ClipboardType type) const {
  DCHECK(CalledOnValidThread());
  LOG(ERROR) << "Is format available " << format.ToString();
  // TODO(msisov): implement DCHECK(IsSupportedClipboardType(type));
  auto available_types = async_clipboard_ozone_->RequestMimeTypes();
  for (auto mime_type : available_types) {
    if (format.ToString() == mime_type) {
      LOG(ERROR) << "YES " << format.ToString();
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
  for (auto mime_type : available_types) {
    LOG(ERROR) << "READ AVAILABLE " << mime_type;
    types->push_back(base::UTF8ToUTF16(mime_type));
  }
}

void ClipboardOzone::ReadText(ClipboardType type,
                              base::string16* result) const {
  DCHECK(CalledOnValidThread());

  std::string text =
      async_clipboard_ozone_->RequestTypesAndWait(type, kMimeTypeText);
  *result = base::UTF8ToUTF16(text);
}

void ClipboardOzone::ReadAsciiText(ClipboardType type,
                                   std::string* result) const {
  DCHECK(CalledOnValidThread());
  std::string text =
      async_clipboard_ozone_->RequestTypesAndWait(type, kMimeTypeText);
  *result = text;
}

void ClipboardOzone::ReadHTML(ClipboardType type,
                              base::string16* markup,
                              std::string* src_url,
                              uint32_t* fragment_start,
                              uint32_t* fragment_end) const {
  DCHECK(CalledOnValidThread());
  // std::string text = async_clipboard_ozone_->RequestTypesAndWait(type,
  // kMimeTypeHTML);
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
  DCHECK(CalledOnValidThread());
  async_clipboard_ozone_->ClearSelectionData();

  for (const auto& object : objects)
    DispatchObject(static_cast<ObjectType>(object.first), object.second);

  if (type == ClipboardType::CLIPBOARD_TYPE_COPY_PASTE)
    async_clipboard_ozone_->OfferData();
}

void ClipboardOzone::WriteText(const char* text_data, size_t text_len) {
  std::string text(text_data, text_len);
  async_clipboard_ozone_->InsertData(text, kMimeTypeText);
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
