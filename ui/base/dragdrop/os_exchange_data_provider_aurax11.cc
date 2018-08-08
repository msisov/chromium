// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/dragdrop/os_exchange_data_provider_aurax11.h"

#include <utility>

#include "base/logging.h"
#include "ui/base/x/selection_utils.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/gfx/x/x11_atom_cache.h"

// Note: the GetBlah() methods are used immediately by the
// web_contents_view_aura.cc:PrepareDropData(), while the omnibox is a
// little more discriminating and calls HasBlah() before trying to get the
// information.

namespace ui {

OSExchangeDataProviderAuraX11::OSExchangeDataProviderAuraX11(
    ::Window x_window,
    const SelectionFormatMap& selection)
    : OSExchangeDataProviderAuraX11Base(x_window, selection) {}

OSExchangeDataProviderAuraX11::OSExchangeDataProviderAuraX11()
    : OSExchangeDataProviderAuraX11Base() {
  PlatformEventSource::GetInstance()->AddPlatformEventDispatcher(this);
}

OSExchangeDataProviderAuraX11::~OSExchangeDataProviderAuraX11() {
  if (own_window_)
    PlatformEventSource::GetInstance()->RemovePlatformEventDispatcher(this);
}

std::unique_ptr<OSExchangeData::Provider>
OSExchangeDataProviderAuraX11::Clone() const {
  std::unique_ptr<OSExchangeDataProviderAuraX11> ret(
      new OSExchangeDataProviderAuraX11());
  ret->format_map_ = format_map_;
  return std::move(ret);
}

void OSExchangeDataProviderAuraX11::SetFileContents(
    const base::FilePath& filename,
    const std::string& file_contents) {
  DCHECK(!filename.empty());
  DCHECK(format_map_.end() ==
         format_map_.find(gfx::GetAtom(Clipboard::kMimeTypeMozillaURL)));

  file_contents_name_ = filename;

  // Direct save handling is a complicated juggling affair between this class,
  // SelectionFormat, and DesktopDragDropClientAuraX11. The general idea behind
  // the protocol is this:
  // - The source window sets its XdndDirectSave0 window property to the
  //   proposed filename.
  // - When a target window receives the drop, it updates the XdndDirectSave0
  //   property on the source window to the filename it would like the contents
  //   to be saved to and then requests the XdndDirectSave0 type from the
  //   source.
  // - The source is supposed to copy the file here and return success (S),
  //   failure (F), or error (E).
  // - In this case, failure means the destination should try to populate the
  //   file itself by copying the data from application/octet-stream. To make
  //   things simpler for Chrome, we always 'fail' and let the destination do
  //   the work.
  std::string failure("F");
  format_map_.Insert(gfx::GetAtom("XdndDirectSave0"),
                     scoped_refptr<base::RefCountedMemory>(
                         base::RefCountedString::TakeString(&failure)));
  std::string file_contents_copy = file_contents;
  format_map_.Insert(
      gfx::GetAtom("application/octet-stream"),
      scoped_refptr<base::RefCountedMemory>(
          base::RefCountedString::TakeString(&file_contents_copy)));
}


bool OSExchangeDataProviderAuraX11::CanDispatchEvent(
    const PlatformEvent& event) {
  return event->xany.window == x_window_;
}

uint32_t OSExchangeDataProviderAuraX11::DispatchEvent(
    const PlatformEvent& event) {
  XEvent* xev = event;
  switch (xev->type) {
    case SelectionRequest:
      selection_owner_.OnSelectionRequest(*xev);
      return ui::POST_DISPATCH_STOP_PROPAGATION;
    default:
      NOTIMPLEMENTED();
  }
  return ui::POST_DISPATCH_NONE;
}

}  // namespace ui
