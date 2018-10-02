// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/client_native_pixmap_factory.h"

namespace gfx {

ClientNativePixmapFactory::ClientNativePixmapFactory(
    bool supports_native_pixmap_import_from_dmabuf)
    : supports_native_pixmap_import_from_dmabuf_(
          supports_native_pixmap_import_from_dmabuf) {}

ClientNativePixmapFactory::~ClientNativePixmapFactory() {}

}  // namespace gfx
