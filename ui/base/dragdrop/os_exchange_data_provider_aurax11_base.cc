// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/dragdrop/os_exchange_data_provider_aurax11_base.h"

#include <utility>

#include "base/logging.h"
#include "base/memory/ref_counted_memory.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "net/base/filename_util.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/dragdrop/file_info.h"
#include "ui/base/x/selection_utils.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/gfx/x/x11_atom_cache.h"

// Note: the GetBlah() methods are used immediately by the
// web_contents_view_aura.cc:PrepareDropData(), while the omnibox is a
// little more discriminating and calls HasBlah() before trying to get the
// information.

namespace ui {

namespace {

const char kDndSelection[] = "XdndSelection";
const char kRendererTaint[] = "chromium/x-renderer-taint";

const char kNetscapeURL[] = "_NETSCAPE_URL";

}  // namespace

OSExchangeDataProviderAuraX11Base::OSExchangeDataProviderAuraX11Base(
    ::Window x_window,
    const SelectionFormatMap& selection)
    : x_display_(gfx::GetXDisplay()),
      x_root_window_(DefaultRootWindow(x_display_)),
      own_window_(false),
      x_window_(x_window),
      format_map_(selection),
      selection_owner_(x_display_, x_window_, gfx::GetAtom(kDndSelection)) {}

OSExchangeDataProviderAuraX11Base::OSExchangeDataProviderAuraX11Base()
    : x_display_(gfx::GetXDisplay()),
      x_root_window_(DefaultRootWindow(x_display_)),
      own_window_(true),
      x_window_(XCreateWindow(x_display_,
                              x_root_window_,
                              -100,
                              -100,
                              10,
                              10,              // x, y, width, height
                              0,               // border width
                              CopyFromParent,  // depth
                              InputOnly,
                              CopyFromParent,  // visual
                              0,
                              NULL)),
      format_map_(),
      selection_owner_(x_display_, x_window_, gfx::GetAtom(kDndSelection)) {
  XStoreName(x_display_, x_window_, "Chromium Drag & Drop Window");
}

OSExchangeDataProviderAuraX11Base::~OSExchangeDataProviderAuraX11Base() {
  if (own_window_)
    XDestroyWindow(x_display_, x_window_);
}

void OSExchangeDataProviderAuraX11Base::TakeOwnershipOfSelection() const {
  selection_owner_.TakeOwnershipOfSelection(format_map_);
}

void OSExchangeDataProviderAuraX11Base::RetrieveTargets(
    std::vector<Atom>* targets) const {
  selection_owner_.RetrieveTargets(targets);
}

SelectionFormatMap OSExchangeDataProviderAuraX11Base::GetFormatMap() const {
  // We return the |selection_owner_|'s format map instead of our own in case
  // ours has been modified since TakeOwnershipOfSelection() was called.
  return selection_owner_.selection_format_map();
}

void OSExchangeDataProviderAuraX11Base::MarkOriginatedFromRenderer() {
  std::string empty;
  format_map_.Insert(gfx::GetAtom(kRendererTaint),
                     scoped_refptr<base::RefCountedMemory>(
                         base::RefCountedString::TakeString(&empty)));
}

bool OSExchangeDataProviderAuraX11Base::DidOriginateFromRenderer() const {
  return format_map_.find(gfx::GetAtom(kRendererTaint)) != format_map_.end();
}

void OSExchangeDataProviderAuraX11Base::SetString(
    const base::string16& text_data) {
  if (HasString())
    return;

  std::string utf8 = base::UTF16ToUTF8(text_data);
  scoped_refptr<base::RefCountedMemory> mem(
      base::RefCountedString::TakeString(&utf8));

  format_map_.Insert(gfx::GetAtom(Clipboard::kMimeTypeText), mem);
  format_map_.Insert(gfx::GetAtom(kText), mem);
  format_map_.Insert(gfx::GetAtom(kString), mem);
  format_map_.Insert(gfx::GetAtom(kUtf8String), mem);
}

void OSExchangeDataProviderAuraX11Base::SetURL(const GURL& url,
                                               const base::string16& title) {
  // TODO(dcheng): The original GTK code tries very hard to avoid writing out an
  // empty title. Is this necessary?
  if (url.is_valid()) {
    // Mozilla's URL format: (UTF16: URL, newline, title)
    base::string16 spec = base::UTF8ToUTF16(url.spec());

    std::vector<unsigned char> data;
    ui::AddString16ToVector(spec, &data);
    ui::AddString16ToVector(base::ASCIIToUTF16("\n"), &data);
    ui::AddString16ToVector(title, &data);
    scoped_refptr<base::RefCountedMemory> mem(
        base::RefCountedBytes::TakeVector(&data));

    format_map_.Insert(gfx::GetAtom(Clipboard::kMimeTypeMozillaURL), mem);

    // Set a string fallback as well.
    SetString(spec);

    // Return early if this drag already contains file contents (this implies
    // that file contents must be populated before URLs). Nautilus (and possibly
    // other file managers) prefer _NETSCAPE_URL over the X Direct Save
    // protocol, but we want to prioritize XDS in this case.
    if (!file_contents_name_.empty())
      return;

    // Set _NETSCAPE_URL for file managers like Nautilus that use it as a hint
    // to create a link to the URL. Setting text/uri-list doesn't work because
    // Nautilus will fetch and copy the contents of the URL to the drop target
    // instead of linking...
    // Format is UTF8: URL + "\n" + title.
    std::string netscape_url = url.spec();
    netscape_url += "\n";
    netscape_url += base::UTF16ToUTF8(title);
    format_map_.Insert(gfx::GetAtom(kNetscapeURL),
                       scoped_refptr<base::RefCountedMemory>(
                           base::RefCountedString::TakeString(&netscape_url)));
  }
}

void OSExchangeDataProviderAuraX11Base::SetFilename(
    const base::FilePath& path) {
  std::vector<FileInfo> data;
  data.push_back(FileInfo(path, base::FilePath()));
  SetFilenames(data);
}

void OSExchangeDataProviderAuraX11Base::SetFilenames(
    const std::vector<FileInfo>& filenames) {
  std::vector<std::string> paths;
  for (const auto& filename : filenames) {
    std::string url_spec = net::FilePathToFileURL(filename.path).spec();
    if (!url_spec.empty())
      paths.push_back(url_spec);
  }

  std::string joined_data = base::JoinString(paths, "\n");
  scoped_refptr<base::RefCountedMemory> mem(
      base::RefCountedString::TakeString(&joined_data));
  format_map_.Insert(gfx::GetAtom(Clipboard::kMimeTypeURIList), mem);
}

void OSExchangeDataProviderAuraX11Base::SetPickledData(
    const Clipboard::FormatType& format,
    const base::Pickle& pickle) {
  const unsigned char* data =
      reinterpret_cast<const unsigned char*>(pickle.data());

  std::vector<unsigned char> bytes;
  bytes.insert(bytes.end(), data, data + pickle.size());
  scoped_refptr<base::RefCountedMemory> mem(
      base::RefCountedBytes::TakeVector(&bytes));

  format_map_.Insert(gfx::GetAtom(format.ToString().c_str()), mem);
}

bool OSExchangeDataProviderAuraX11Base::GetString(
    base::string16* result) const {
  if (HasFile()) {
    // Various Linux file managers both pass a list of file:// URIs and set the
    // string representation to the URI. We explicitly don't want to return use
    // this representation.
    return false;
  }

  std::vector<::Atom> text_atoms = ui::GetTextAtomsFrom();
  std::vector<::Atom> requested_types;
  GetAtomIntersection(text_atoms, GetTargets(), &requested_types);

  ui::SelectionData data(format_map_.GetFirstOf(requested_types));
  if (data.IsValid()) {
    std::string text = data.GetText();
    *result = base::UTF8ToUTF16(text);
    return true;
  }

  return false;
}

bool OSExchangeDataProviderAuraX11Base::GetURLAndTitle(
    OSExchangeData::FilenameToURLPolicy policy,
    GURL* url,
    base::string16* title) const {
  std::vector<::Atom> url_atoms = ui::GetURLAtomsFrom();
  std::vector<::Atom> requested_types;
  GetAtomIntersection(url_atoms, GetTargets(), &requested_types);

  ui::SelectionData data(format_map_.GetFirstOf(requested_types));
  if (data.IsValid()) {
    // TODO(erg): Technically, both of these forms can accept multiple URLs,
    // but that doesn't match the assumptions of the rest of the system which
    // expect single types.

    if (data.GetType() == gfx::GetAtom(Clipboard::kMimeTypeMozillaURL)) {
      // Mozilla URLs are (UTF16: URL, newline, title).
      base::string16 unparsed;
      data.AssignTo(&unparsed);

      std::vector<base::string16> tokens =
          base::SplitString(unparsed, base::ASCIIToUTF16("\n"),
                            base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
      if (tokens.size() > 0) {
        if (tokens.size() > 1)
          *title = tokens[1];
        else
          *title = base::string16();

        *url = GURL(tokens[0]);
        return true;
      }
    } else if (data.GetType() == gfx::GetAtom(Clipboard::kMimeTypeURIList)) {
      std::vector<std::string> tokens = ui::ParseURIList(data);
      for (const std::string& token : tokens) {
        GURL test_url(token);
        if (!test_url.SchemeIsFile() ||
            policy == OSExchangeData::CONVERT_FILENAMES) {
          *url = test_url;
          *title = base::string16();
          return true;
        }
      }
    }
  }

  return false;
}

bool OSExchangeDataProviderAuraX11Base::GetFilename(
    base::FilePath* path) const {
  std::vector<FileInfo> filenames;
  if (GetFilenames(&filenames)) {
    *path = filenames.front().path;
    return true;
  }

  return false;
}

bool OSExchangeDataProviderAuraX11Base::GetFilenames(
    std::vector<FileInfo>* filenames) const {
  std::vector<::Atom> url_atoms = ui::GetURIListAtomsFrom();
  std::vector<::Atom> requested_types;
  GetAtomIntersection(url_atoms, GetTargets(), &requested_types);

  filenames->clear();
  ui::SelectionData data(format_map_.GetFirstOf(requested_types));
  if (data.IsValid()) {
    std::vector<std::string> tokens = ui::ParseURIList(data);
    for (const std::string& token : tokens) {
      GURL url(token);
      base::FilePath file_path;
      if (url.SchemeIsFile() && net::FileURLToFilePath(url, &file_path)) {
        filenames->push_back(FileInfo(file_path, base::FilePath()));
      }
    }
  }

  return !filenames->empty();
}

bool OSExchangeDataProviderAuraX11Base::GetPickledData(
    const Clipboard::FormatType& format,
    base::Pickle* pickle) const {
  std::vector<::Atom> requested_types;
  requested_types.push_back(gfx::GetAtom(format.ToString().c_str()));

  ui::SelectionData data(format_map_.GetFirstOf(requested_types));
  if (data.IsValid()) {
    // Note that the pickle object on the right hand side of the assignment
    // only refers to the bytes in |data|. The assignment copies the data.
    *pickle = base::Pickle(reinterpret_cast<const char*>(data.GetData()),
                           static_cast<int>(data.GetSize()));
    return true;
  }

  return false;
}

bool OSExchangeDataProviderAuraX11Base::HasString() const {
  std::vector<::Atom> text_atoms = ui::GetTextAtomsFrom();
  std::vector<::Atom> requested_types;
  GetAtomIntersection(text_atoms, GetTargets(), &requested_types);
  return !requested_types.empty() && !HasFile();
}

bool OSExchangeDataProviderAuraX11Base::HasURL(
    OSExchangeData::FilenameToURLPolicy policy) const {
  std::vector<::Atom> url_atoms = ui::GetURLAtomsFrom();
  std::vector<::Atom> requested_types;
  GetAtomIntersection(url_atoms, GetTargets(), &requested_types);

  if (requested_types.empty())
    return false;

  // The Linux desktop doesn't differentiate between files and URLs like
  // Windows does and stuffs all the data into one mime type.
  ui::SelectionData data(format_map_.GetFirstOf(requested_types));
  if (data.IsValid()) {
    if (data.GetType() == gfx::GetAtom(Clipboard::kMimeTypeMozillaURL)) {
      // File managers shouldn't be using this type, so this is a URL.
      return true;
    } else if (data.GetType() ==
               gfx::GetAtom(ui::Clipboard::kMimeTypeURIList)) {
      std::vector<std::string> tokens = ui::ParseURIList(data);
      for (const std::string& token : tokens) {
        if (!GURL(token).SchemeIsFile() ||
            policy == OSExchangeData::CONVERT_FILENAMES)
          return true;
      }

      return false;
    }
  }

  return false;
}

bool OSExchangeDataProviderAuraX11Base::HasFile() const {
  std::vector<::Atom> url_atoms = ui::GetURIListAtomsFrom();
  std::vector<::Atom> requested_types;
  GetAtomIntersection(url_atoms, GetTargets(), &requested_types);

  if (requested_types.empty())
    return false;

  // To actually answer whether we have a file, we need to look through the
  // contents of the kMimeTypeURIList type, and see if any of them are file://
  // URIs.
  ui::SelectionData data(format_map_.GetFirstOf(requested_types));
  if (data.IsValid()) {
    std::vector<std::string> tokens = ui::ParseURIList(data);
    for (const std::string& token : tokens) {
      GURL url(token);
      base::FilePath file_path;
      if (url.SchemeIsFile() && net::FileURLToFilePath(url, &file_path))
        return true;
    }
  }

  return false;
}

bool OSExchangeDataProviderAuraX11Base::HasCustomFormat(
    const Clipboard::FormatType& format) const {
  std::vector<::Atom> url_atoms;
  url_atoms.push_back(gfx::GetAtom(format.ToString().c_str()));
  std::vector<::Atom> requested_types;
  GetAtomIntersection(url_atoms, GetTargets(), &requested_types);

  return !requested_types.empty();
}

void OSExchangeDataProviderAuraX11Base::SetHtml(const base::string16& html,
                                                const GURL& base_url) {
  std::vector<unsigned char> bytes;
  // Manually jam a UTF16 BOM into bytes because otherwise, other programs will
  // assume UTF-8.
  bytes.push_back(0xFF);
  bytes.push_back(0xFE);
  ui::AddString16ToVector(html, &bytes);
  scoped_refptr<base::RefCountedMemory> mem(
      base::RefCountedBytes::TakeVector(&bytes));

  format_map_.Insert(gfx::GetAtom(Clipboard::kMimeTypeHTML), mem);
}

bool OSExchangeDataProviderAuraX11Base::GetHtml(base::string16* html,
                                                GURL* base_url) const {
  std::vector<::Atom> url_atoms;
  url_atoms.push_back(gfx::GetAtom(Clipboard::kMimeTypeHTML));
  std::vector<::Atom> requested_types;
  GetAtomIntersection(url_atoms, GetTargets(), &requested_types);

  ui::SelectionData data(format_map_.GetFirstOf(requested_types));
  if (data.IsValid()) {
    *html = data.GetHtml();
    *base_url = GURL();
    return true;
  }

  return false;
}

bool OSExchangeDataProviderAuraX11Base::HasHtml() const {
  std::vector<::Atom> url_atoms;
  url_atoms.push_back(gfx::GetAtom(Clipboard::kMimeTypeHTML));
  std::vector<::Atom> requested_types;
  GetAtomIntersection(url_atoms, GetTargets(), &requested_types);

  return !requested_types.empty();
}

void OSExchangeDataProviderAuraX11Base::SetDragImage(
    const gfx::ImageSkia& image,
    const gfx::Vector2d& cursor_offset) {
  drag_image_ = image;
  drag_image_offset_ = cursor_offset;
}

gfx::ImageSkia OSExchangeDataProviderAuraX11Base::GetDragImage() const {
  return drag_image_;
}

gfx::Vector2d OSExchangeDataProviderAuraX11Base::GetDragImageOffset() const {
  return drag_image_offset_;
}

bool OSExchangeDataProviderAuraX11Base::GetPlainTextURL(GURL* url) const {
  base::string16 text;
  if (GetString(&text)) {
    GURL test_url(text);
    if (test_url.is_valid()) {
      *url = test_url;
      return true;
    }
  }

  return false;
}

std::vector<::Atom> OSExchangeDataProviderAuraX11Base::GetTargets() const {
  return format_map_.GetTypes();
}

}  // namespace ui
