/*
// Copyright (c) 2016 Intel Corporation
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
*/

#include "nativesurface.h"

#include "displayplane.h"
#include "hwctrace.h"
#include "nativebufferhandler.h"

namespace hwcomposer {

NativeSurface::NativeSurface(uint32_t width, uint32_t height)
    : native_handle_(0),
      buffer_handler_(NULL),
      width_(width),
      height_(height),
      in_use_(false),
      framebuffer_format_(0) {
}

NativeSurface::~NativeSurface() {
  // Ensure we close any framebuffers before
  // releasing buffer.
  layer_.ResetBuffer();

  if (native_handle_) {
    buffer_handler_->DestroyHandle(native_handle_);
  }
}

bool NativeSurface::Init(NativeBufferHandler *buffer_handler,
                         bool cursor_layer) {
  buffer_handler_ = buffer_handler;
  buffer_handler_->CreateBuffer(width_, height_, 0, &native_handle_,
                                cursor_layer);
  if (!native_handle_) {
    ETRACE("NativeSurface: Failed to create buffer.");
    return false;
  }

  InitializeLayer(buffer_handler, native_handle_);
  if (cursor_layer) {
    layer_.GPURenderedCursor();
  }

  return true;
}

bool NativeSurface::InitializeForOffScreenRendering(
    NativeBufferHandler *buffer_handler, HWCNativeHandle native_handle) {
  InitializeLayer(buffer_handler, native_handle);
  layer_.SetSourceCrop(HwcRect<float>(0, 0, width_, height_));
  layer_.SetDisplayFrame(HwcRect<int>(0, 0, width_, height_));

  return true;
}

void NativeSurface::SetNativeFence(int32_t fd) {
  layer_.SetAcquireFence(fd);
}

void NativeSurface::SetInUse(bool inuse) {
  if (in_use_ != inuse) {
    clear_surface_ = true;
  }

  in_use_ = inuse;
}

void NativeSurface::SetPlaneTarget(DisplayPlaneState &plane, uint32_t gpu_fd) {
  uint32_t format =
      plane.plane()->GetFormatForFrameBuffer(layer_.GetBuffer()->GetFormat());

  const HwcRect<int> &display_rect = plane.GetDisplayFrame();
  const HwcRect<int> &surface_damage = plane.GetSurfaceDamage();
  layer_.SetSourceCrop(HwcRect<float>(display_rect));
  layer_.SetDisplayFrame(HwcRect<int>(display_rect));
  width_ = display_rect.right - display_rect.left;
  height_ = display_rect.bottom - display_rect.top;
  viewport_width_ = surface_damage.right - surface_damage.left;
  viewport_height_ = surface_damage.bottom - surface_damage.top;
  plane.SetOverlayLayer(&layer_);
  SetInUse(true);

  if (framebuffer_format_ == format)
    return;

  framebuffer_format_ = format;
  layer_.GetBuffer()->SetRecommendedFormat(framebuffer_format_);
  layer_.GetBuffer()->CreateFrameBuffer(gpu_fd);
}

void NativeSurface::RecycleSurface(DisplayPlaneState &plane) {
  const HwcRect<int> &surface_damage = plane.GetSurfaceDamage();
  viewport_width_ = surface_damage.right - surface_damage.left;
  viewport_height_ = surface_damage.bottom - surface_damage.top;
  plane.SetOverlayLayer(&layer_);
  SetInUse(true);
  clear_surface_ = false;
  layer_.SetAcquireFence(-1);
}

void NativeSurface::InitializeLayer(NativeBufferHandler *buffer_handler,
                                    HWCNativeHandle native_handle) {
  layer_.SetBlending(HWCBlending::kBlendingPremult);
  layer_.SetTransform(0);
  layer_.SetBuffer(buffer_handler, native_handle, -1);
}

}  // namespace hwcomposer
