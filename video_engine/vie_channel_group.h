/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VIDEO_ENGINE_VIE_CHANNEL_GROUP_H_
#define WEBRTC_VIDEO_ENGINE_VIE_CHANNEL_GROUP_H_

#include <set>

#include "webrtc/base/scoped_ptr.h"
#include "webrtc/modules/bitrate_controller/include/bitrate_controller.h"

namespace webrtc {

class BitrateAllocator;
class CallStats;
class Config;
class EncoderStateFeedback;
class ProcessThread;
class RemoteBitrateEstimator;
class ViEChannel;
class ViEEncoder;
class VieRemb;

// Channel group contains data common for several channels. All channels in the
// group are assumed to send/receive data to the same end-point.
class ChannelGroup : public BitrateObserver {
 public:
  ChannelGroup(ProcessThread* process_thread, const Config* config);
  ~ChannelGroup();

  void AddChannel(int channel_id);
  void RemoveChannel(int channel_id, unsigned int ssrc);
  bool HasChannel(int channel_id);
  bool Empty();

  void SetChannelRembStatus(int channel_id,
                            bool sender,
                            bool receiver,
                            ViEChannel* channel);

  BitrateAllocator* GetBitrateAllocator();
  BitrateController* GetBitrateController();
  CallStats* GetCallStats();
  RemoteBitrateEstimator* GetRemoteBitrateEstimator();
  EncoderStateFeedback* GetEncoderStateFeedback();

  // Implements BitrateObserver.
  void OnNetworkChanged(uint32_t target_bitrate_bps,
                        uint8_t fraction_loss,
                        int64_t rtt) override;

 private:
  typedef std::set<int> ChannelSet;

  rtc::scoped_ptr<VieRemb> remb_;
  rtc::scoped_ptr<BitrateAllocator> bitrate_allocator_;
  rtc::scoped_ptr<BitrateController> bitrate_controller_;
  rtc::scoped_ptr<CallStats> call_stats_;
  rtc::scoped_ptr<RemoteBitrateEstimator> remote_bitrate_estimator_;
  rtc::scoped_ptr<EncoderStateFeedback> encoder_state_feedback_;
  ChannelSet channels_;
  const Config* config_;
  // Placeholder for the case where this owns the config.
  rtc::scoped_ptr<Config> own_config_;

  // Registered at construct time and assumed to outlive this class.
  ProcessThread* process_thread_;
};

}  // namespace webrtc

#endif  // WEBRTC_VIDEO_ENGINE_VIE_CHANNEL_GROUP_H_
