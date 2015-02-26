/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_TEST_CONFIGURABLE_FRAME_SIZE_ENCODER_H_
#define WEBRTC_TEST_CONFIGURABLE_FRAME_SIZE_ENCODER_H_

#include <vector>

#include "webrtc/base/scoped_ptr.h"
#include "webrtc/video_encoder.h"

namespace webrtc {
namespace test {

class ConfigurableFrameSizeEncoder : public VideoEncoder {
 public:
  explicit ConfigurableFrameSizeEncoder(size_t max_frame_size);
  virtual ~ConfigurableFrameSizeEncoder();

  virtual int32_t InitEncode(const VideoCodec* codec_settings,
                             int32_t number_of_cores,
                             size_t max_payload_size) OVERRIDE;

  virtual int32_t Encode(const I420VideoFrame& input_image,
                         const CodecSpecificInfo* codec_specific_info,
                         const std::vector<VideoFrameType>* frame_types)
      OVERRIDE;

  virtual int32_t RegisterEncodeCompleteCallback(EncodedImageCallback* callback)
      OVERRIDE;

  virtual int32_t Release() OVERRIDE;

  virtual int32_t SetChannelParameters(uint32_t packet_loss,
                                       int64_t rtt) OVERRIDE;

  virtual int32_t SetRates(uint32_t new_bit_rate, uint32_t frame_rate) OVERRIDE;

  virtual int32_t SetPeriodicKeyFrames(bool enable) OVERRIDE;

  virtual int32_t CodecConfigParameters(uint8_t* buffer, int32_t size) OVERRIDE;

  int32_t SetFrameSize(size_t size);

 private:
  EncodedImageCallback* callback_;
  const size_t max_frame_size_;
  size_t current_frame_size_;
  rtc::scoped_ptr<uint8_t[]> buffer_;
};

}  // namespace test
}  // namespace webrtc

#endif  // WEBRTC_TEST_CONFIGURABLE_FRAME_SIZE_ENCODER_H_
