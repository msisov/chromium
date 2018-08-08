// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/x11/x11_drag_util.h"

#include "base/memory/ref_counted_memory.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/x/selection_utils.h"

namespace ui {

namespace {
const char kDragNetscapeURL[] = "_NETSCAPE_URL";
const char kDragString[] = "STRING";
const char kDragText[] = "TEXT";
const char kDragTextPlain[] = "text/plain";
const char kDragTextPlainUtf8[] = "text/plain;charset=utf-8";
const char kDragUtf8String[] = "UTF8_STRING";
}  // namespace

::Atom DragOperationToAtom(int drag_operation) {
  if (drag_operation & DragDropTypes::DRAG_COPY)
    return gfx::GetAtom(kXdndActionCopy);
  if (drag_operation & DragDropTypes::DRAG_MOVE)
    return gfx::GetAtom(kXdndActionMove);
  if (drag_operation & DragDropTypes::DRAG_LINK)
    return gfx::GetAtom(kXdndActionLink);

  return x11::None;
}

DragDropTypes::DragOperation AtomToDragOperation(::Atom atom) {
  if (atom == gfx::GetAtom(kXdndActionCopy))
    return DragDropTypes::DRAG_COPY;
  if (atom == gfx::GetAtom(kXdndActionMove))
    return DragDropTypes::DRAG_MOVE;
  if (atom == gfx::GetAtom(kXdndActionLink))
    return DragDropTypes::DRAG_LINK;

  return DragDropTypes::DRAG_NONE;
}

std::vector<::Atom> GetOfferedDragOperations(int drag_operations) {
  std::vector<::Atom> operations;
  if (drag_operations & DragDropTypes::DRAG_COPY)
    operations.push_back(gfx::GetAtom(kXdndActionCopy));
  if (drag_operations & DragDropTypes::DRAG_MOVE)
    operations.push_back(gfx::GetAtom(kXdndActionMove));
  if (drag_operations & DragDropTypes::DRAG_LINK)
    operations.push_back(gfx::GetAtom(kXdndActionLink));
  return operations;
}

void InsertStringToSelectionFormatMap(const base::string16& text_data,
                                      SelectionFormatMap* map) {
  std::string utf8 = base::UTF16ToUTF8(text_data);
  scoped_refptr<base::RefCountedMemory> mem(
      base::RefCountedString::TakeString(&utf8));

  map->Insert(gfx::GetAtom(Clipboard::kMimeTypeText), mem);
  map->Insert(gfx::GetAtom(kDragText), mem);
  map->Insert(gfx::GetAtom(kDragString), mem);
  map->Insert(gfx::GetAtom(kDragUtf8String), mem);
  map->Insert(gfx::GetAtom(kDragTextPlain), mem);
  map->Insert(gfx::GetAtom(kDragTextPlainUtf8), mem);
}

void InsertURLToSelectionFormatMap(const GURL& url,
                                   const base::string16& title,
                                   SelectionFormatMap* map) {
  if (url.is_valid()) {
    // Mozilla's URL format: (UTF16: URL, newline, title)
    base::string16 spec = base::UTF8ToUTF16(url.spec());

    std::vector<unsigned char> data;
    ui::AddString16ToVector(spec, &data);
    ui::AddString16ToVector(base::ASCIIToUTF16("\n"), &data);
    ui::AddString16ToVector(title, &data);
    scoped_refptr<base::RefCountedMemory> mem(
        base::RefCountedBytes::TakeVector(&data));

    map->Insert(gfx::GetAtom(Clipboard::kMimeTypeMozillaURL), mem);

    // Set a string fallback as well.
    InsertStringToSelectionFormatMap(spec, map);

    // Set _NETSCAPE_URL for file managers like Nautilus that use it as a hint
    // to create a link to the URL. Setting text/uri-list doesn't work because
    // Nautilus will fetch and copy the contents of the URL to the drop target
    // instead of linking...
    // Format is UTF8: URL + "\n" + title.
    std::string netscape_url = url.spec();
    netscape_url += "\n";
    netscape_url += base::UTF16ToUTF8(title);
    map->Insert(gfx::GetAtom(kDragNetscapeURL),
                scoped_refptr<base::RefCountedMemory>(
                    base::RefCountedString::TakeString(&netscape_url)));
  }
}

void SendXClientEvent(XID window, XEvent* xev) {
  DCHECK_EQ(ClientMessage, xev->type);
  XSendEvent(gfx::GetXDisplay(), window, x11::False, 0, xev);
}

}  // namespace ui
