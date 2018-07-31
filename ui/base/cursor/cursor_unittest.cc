// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/cursor/cursor.h"

#include "base/logging.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/cursor/cursor_type.h"
#include "ui/base/resource/data_pack.h"
#include "ui/base/resource/resource_bundle.h"

namespace ui {

namespace {

// Cursor hotspot for the kPointer type is taken from
// ui/base/cursor/cursors_aura.cc kNormalCursors.
constexpr gfx::Point kPointerHotspot1x = gfx::Point(4, 4);
constexpr gfx::Point kPointerHotspot2x = gfx::Point(7, 7);

const char kSamplePakContentsV4[] = {
    0x04, 0x00, 0x00, 0x00,              // header(version
    0x04, 0x00, 0x00, 0x00,              //        no. entries
    0x01,                                //        encoding)
    0x01, 0x00, 0x27, 0x00, 0x00, 0x00,  // index entry 1
    0x04, 0x00, 0x27, 0x00, 0x00, 0x00,  // index entry 4
    0x06, 0x00, 0x33, 0x00, 0x00, 0x00,  // index entry 6
    0x0a, 0x00, 0x3f, 0x00, 0x00, 0x00,  // index entry 10
    0x00, 0x00, 0x3f, 0x00, 0x00, 0x00,  // extra entry for the size of last
    't',  'h',  'i',  's',  ' ',  'i',  's', ' ', 'i', 'd', ' ', '4',
    't',  'h',  'i',  's',  ' ',  'i',  's', ' ', 'i', 'd', ' ', '6'};

const size_t kSamplePakSizeV4 = sizeof(kSamplePakContentsV4);

void AddScaleFactorToResourceBundle(ScaleFactor scale_factor) {
  ui::ResourceBundle::GetSharedInstance().CleanupSharedInstance();

  std::unique_ptr<DataPack> data_pack =
      std::make_unique<DataPack>(ScaleFactor::SCALE_FACTOR_200P);
  // Load sample pak contents, otherwise CheckForDuplicateResources
  // fails (called by AddDataPack). Unfortunetaly, all these steps
  // are needed to properly identify hotpoints of cursors, which
  // depend on the scale factor and SCALE_FACTOR_200P availability
  // from the ResourceBundle.
  ASSERT_TRUE(data_pack->LoadFromBuffer(
      base::StringPiece(kSamplePakContentsV4, kSamplePakSizeV4)));

  ui::ResourceBundle::InitSharedInstanceWithPakPath(base::FilePath());
  ui::ResourceBundle::GetSharedInstance().AddDataPack(std::move(data_pack));
}

}  // namespace

// Checks that default scale value of cursor is 1.0f and
// hotpoints correspond to that value regradless of availability of
// 200p resources.
TEST(CursorTest, EnsureHotspotValuesWithoutScale) {
  Cursor cursor(CursorType::kPointer);

  EXPECT_EQ(1.0f, cursor.device_scale_factor());
  EXPECT_EQ(kPointerHotspot1x, cursor.GetHotspot());

  // 1.0f scale hotpoints must be used when the device scale factor is set to
  // 1.0f regardless of the availability of 200P scale factor in the resource
  // bundle. Check SearchTable in the ui/base/cursor/cursors_aura.cc to
  // understand this better.
  AddScaleFactorToResourceBundle(ScaleFactor::SCALE_FACTOR_200P);
  EXPECT_EQ(kPointerHotspot1x, cursor.GetHotspot());
}

// Checks that explicitly set device scale value of the cursor always results
// in 2.0F hotpoints.
TEST(CursorTest, EnsureHotspotValuesWithScale) {
  Cursor cursor(CursorType::kPointer);
  cursor.set_device_scale_factor(2.0f);

  EXPECT_EQ(2.0f, cursor.device_scale_factor());

  // If device scale factor is 2.0f, hotpoints with 2x scale factor must be
  // used.
  EXPECT_EQ(kPointerHotspot2x, cursor.GetHotspot());

  // Nothing must change ones the resource bundle has max scale of 200p.
  AddScaleFactorToResourceBundle(ScaleFactor::SCALE_FACTOR_200P);
  EXPECT_EQ(kPointerHotspot2x, cursor.GetHotspot());
}

}  // namespace ui
