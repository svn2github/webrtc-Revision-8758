/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/remote_bitrate_estimator/test/bwe_test.h"

#include "webrtc/base/common.h"
#include "webrtc/modules/interface/module_common_types.h"
#include "webrtc/modules/remote_bitrate_estimator/test/bwe_test_baselinefile.h"
#include "webrtc/modules/remote_bitrate_estimator/test/bwe_test_framework.h"
#include "webrtc/modules/remote_bitrate_estimator/include/remote_bitrate_estimator.h"
#include "webrtc/modules/rtp_rtcp/interface/receive_statistics.h"
#include "webrtc/system_wrappers/interface/clock.h"
#include "webrtc/system_wrappers/interface/scoped_ptr.h"

using std::string;
using std::vector;

namespace webrtc {
namespace testing {
namespace bwe {

class PacketReceiver : public RemoteBitrateObserver {
 public:
  static const uint32_t kRemoteBitrateEstimatorMinBitrateBps = 30000;
  static const int kDelayPlotIntervalMs = 100;

  PacketReceiver(const string& test_name,
                 const BweTestConfig::EstimatorConfig& config)
      : debug_name_(config.debug_name),
        delay_log_prefix_(),
        estimate_log_prefix_(),
        last_delay_plot_ms_(0),
        plot_delay_(config.plot_delay),
        plot_estimate_(config.plot_estimate),
        clock_(0),
        stats_(),
        recv_stats_(ReceiveStatistics::Create(&clock_)),
        latest_estimate_bps_(-1),
        estimator_(config.estimator_factory->Create(
            this,
            &clock_,
            config.control_type,
            kRemoteBitrateEstimatorMinBitrateBps)),
        baseline_(BaseLineFileInterface::Create(test_name + "_" + debug_name_,
                                                config.update_baseline)) {
    assert(estimator_.get());
    assert(baseline_.get());
    // Setup the prefix strings used when logging.
    std::stringstream ss;
    ss << "Delay_" << config.flow_id << "#2";
    delay_log_prefix_ = ss.str();
    ss.str("");
    ss << "Estimate_" << config.flow_id << "#1";
    estimate_log_prefix_ = ss.str();
    // Default RTT in RemoteRateControl is 200 ms ; 50 ms is more realistic.
    estimator_->OnRttUpdate(50);
  }

  void EatPacket(const Packet& packet) {
    BWE_TEST_LOGGING_CONTEXT(debug_name_);

    recv_stats_->IncomingPacket(packet.header(), packet.payload_size(), false);

    latest_estimate_bps_ = -1;

    // We're treating the send time (from previous filter) as the arrival
    // time once packet reaches the estimator.
    int64_t packet_time_ms = (packet.send_time_us() + 500) / 1000;
    BWE_TEST_LOGGING_TIME(packet_time_ms);
    if (plot_delay_) {
      if (clock_.TimeInMilliseconds() - last_delay_plot_ms_ >
          kDelayPlotIntervalMs) {
        BWE_TEST_LOGGING_PLOT(delay_log_prefix_, clock_.TimeInMilliseconds(),
                              packet_time_ms -
                              (packet.creation_time_us() + 500) / 1000);
        last_delay_plot_ms_ = clock_.TimeInMilliseconds();
      }
    }

    int64_t step_ms = std::max<int64_t>(estimator_->TimeUntilNextProcess(), 0);
    while ((clock_.TimeInMilliseconds() + step_ms) < packet_time_ms) {
      clock_.AdvanceTimeMilliseconds(step_ms);
      estimator_->Process();
      step_ms = std::max<int64_t>(estimator_->TimeUntilNextProcess(), 0);
    }
    estimator_->IncomingPacket(packet_time_ms, packet.payload_size(),
                               packet.header());
    clock_.AdvanceTimeMilliseconds(packet_time_ms -
                                   clock_.TimeInMilliseconds());
    ASSERT_TRUE(packet_time_ms == clock_.TimeInMilliseconds());
  }

  bool GetFeedback(PacketSender::Feedback* feedback) {
    assert(feedback);
    BWE_TEST_LOGGING_CONTEXT(debug_name_);
    uint32_t estimated_bps = 0;
    if (LatestEstimate(&estimated_bps)) {
      feedback->estimated_bps = estimated_bps;
      StatisticianMap statisticians = recv_stats_->GetActiveStatisticians();
      if (statisticians.empty()) {
        feedback->report_block = RTCPReportBlock();
      } else {
        feedback->report_block =
            BuildReportBlock(statisticians.begin()->second);
      }
      baseline_->Estimate(clock_.TimeInMilliseconds(), estimated_bps);

      double estimated_kbps = static_cast<double>(estimated_bps) / 1000.0;
      stats_.Push(estimated_kbps);
      if (plot_estimate_) {
        BWE_TEST_LOGGING_PLOT(estimate_log_prefix_, clock_.TimeInMilliseconds(),
                              estimated_kbps);
      }
      return true;
    }
    return false;
  }

  void LogStats() {
    BWE_TEST_LOGGING_CONTEXT(debug_name_);
    BWE_TEST_LOGGING_CONTEXT("Mean");
    stats_.Log("kbps");
  }

  void VerifyOrWriteBaseline() {
    EXPECT_TRUE(baseline_->VerifyOrWrite());
  }

  virtual void OnReceiveBitrateChanged(const vector<unsigned int>& ssrcs,
                                       unsigned int bitrate) {
  }

 private:
  static RTCPReportBlock BuildReportBlock(StreamStatistician* statistician) {
    RTCPReportBlock report_block;
    RtcpStatistics stats;
    if (!statistician->GetStatistics(&stats, true))
      return report_block;
    report_block.fractionLost = stats.fraction_lost;
    report_block.cumulativeLost = stats.cumulative_lost;
    report_block.extendedHighSeqNum = stats.extended_max_sequence_number;
    report_block.jitter = stats.jitter;
    return report_block;
  }

  bool LatestEstimate(uint32_t* estimate_bps) {
    if (latest_estimate_bps_ < 0) {
      vector<unsigned int> ssrcs;
      unsigned int bps = 0;
      if (!estimator_->LatestEstimate(&ssrcs, &bps)) {
        return false;
      }
      latest_estimate_bps_ = bps;
    }
    *estimate_bps = latest_estimate_bps_;
    return true;
  }

  string debug_name_;
  string delay_log_prefix_;
  string estimate_log_prefix_;
  int64_t last_delay_plot_ms_;
  bool plot_delay_;
  bool plot_estimate_;
  SimulatedClock clock_;
  Stats<double> stats_;
  scoped_ptr<ReceiveStatistics> recv_stats_;
  int64_t latest_estimate_bps_;
  scoped_ptr<RemoteBitrateEstimator> estimator_;
  scoped_ptr<BaseLineFileInterface> baseline_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(PacketReceiver);
};

class PacketProcessorRunner {
 public:
  explicit PacketProcessorRunner(PacketProcessor* processor)
      : processor_(processor) {}

  bool HasProcessor(const PacketProcessor* processor) const {
    return processor == processor_;
  }

  void RunFor(int64_t time_ms, int64_t time_now_ms, Packets* in_out) {
    Packets to_process;
    FindPacketsToProcess(processor_->flow_ids(), in_out, &to_process);
    processor_->RunFor(time_ms, &to_process);
    QueuePackets(&to_process, time_now_ms * 1000);
    if (!to_process.empty()) {
      processor_->Plot((to_process.back().send_time_us() + 500) / 1000);
    }
    in_out->merge(to_process);
  }

 private:
  void FindPacketsToProcess(const FlowIds& flow_ids, Packets* in,
                            Packets* out) {
    assert(out->empty());
    for (Packets::iterator it = in->begin(); it != in->end();) {
      // TODO(holmer): Further optimize this by looking for consecutive flow ids
      // in the packet list and only doing the binary search + splice once for a
      // sequence.
      if (flow_ids.find(it->flow_id()) != flow_ids.end()) {
        Packets::iterator next = it;
        ++next;
        out->splice(out->end(), *in, it);
        it = next;
      } else {
        ++it;
      }
    }
  }

  void QueuePackets(Packets* batch, int64_t end_of_batch_time_us) {
    queue_.merge(*batch);
    if (queue_.empty()) {
      return;
    }
    Packets::iterator it = queue_.begin();
    for (; it != queue_.end(); ++it) {
      if (it->send_time_us() > end_of_batch_time_us) {
        break;
      }
    }
    Packets to_transfer;
    to_transfer.splice(to_transfer.begin(), queue_, queue_.begin(), it);
    batch->merge(to_transfer);
  }

  PacketProcessor* processor_;
  Packets queue_;
};

BweTest::BweTest()
    : run_time_ms_(0),
      time_now_ms_(-1),
      simulation_interval_ms_(-1),
      estimators_(),
      processors_() {
}

BweTest::~BweTest() {
  BWE_TEST_LOGGING_GLOBAL_ENABLE(true);
  for (EstimatorMap::iterator it = estimators_.begin(); it != estimators_.end();
      ++it) {
    it->second->VerifyOrWriteBaseline();
    it->second->LogStats();
  }
  BWE_TEST_LOGGING_GLOBAL_CONTEXT("");

  for (EstimatorMap::iterator it = estimators_.begin();
       it != estimators_.end(); ++it) {
    delete it->second;
  }
}

void BweTest::SetupTestFromConfig(const BweTestConfig& config) {
  const ::testing::TestInfo* const test_info =
      ::testing::UnitTest::GetInstance()->current_test_info();
  string test_name =
      string(test_info->test_case_name()) + "_" + string(test_info->name());
  BWE_TEST_LOGGING_GLOBAL_CONTEXT(test_name);
  for (vector<BweTestConfig::EstimatorConfig>::const_iterator it =
       config.estimator_configs.begin(); it != config.estimator_configs.end();
       ++it) {
    estimators_.insert(
        std::make_pair(it->flow_id, new PacketReceiver(test_name, *it)));
  }
  BWE_TEST_LOGGING_GLOBAL_ENABLE(false);
}

void BweTest::AddPacketProcessor(PacketProcessor* processor, bool is_sender) {
  assert(processor);
  processors_.push_back(PacketProcessorRunner(processor));
  if (is_sender) {
    senders_.push_back(static_cast<PacketSender*>(processor));
  }
  for (const auto& flow_id : processor->flow_ids()) {
    RTC_UNUSED(flow_id);
    assert(estimators_.count(flow_id) == 1);
  }
}

void BweTest::RemovePacketProcessor(
    PacketProcessor* processor) {
  for (vector<PacketProcessorRunner>::iterator it = processors_.begin();
       it != processors_.end(); ++it) {
    if (it->HasProcessor(processor)) {
      processors_.erase(it);
      return;
    }
  }
  assert(false);
}

void BweTest::VerboseLogging(bool enable) {
  BWE_TEST_LOGGING_GLOBAL_ENABLE(enable);
}

void BweTest::GiveFeedbackToAffectedSenders(int flow_id,
                                            PacketReceiver* estimator) {
  std::list<PacketSender*> affected_senders;
  for (auto* sender : senders_) {
    if (sender->flow_ids().find(flow_id) != sender->flow_ids().end()) {
      affected_senders.push_back(sender);
    }
  }
  PacketSender::Feedback feedback = {0};
  if (estimator->GetFeedback(&feedback) && !affected_senders.empty()) {
    // Allocate the bitrate evenly between the senders.
    feedback.estimated_bps /= affected_senders.size();
    for (auto* sender : affected_senders) {
      sender->GiveFeedback(feedback);
    }
  }
}

void BweTest::RunFor(int64_t time_ms) {
  // Set simulation interval from first packet sender.
  // TODO(holmer): Support different feedback intervals for different flows.
  if (!senders_.empty()) {
    simulation_interval_ms_ = senders_[0]->GetFeedbackIntervalMs();
  }
  assert(simulation_interval_ms_ > 0);
  if (time_now_ms_ == -1) {
    time_now_ms_ = simulation_interval_ms_;
  }
  for (run_time_ms_ += time_ms;
       time_now_ms_ <= run_time_ms_ - simulation_interval_ms_;
       time_now_ms_ += simulation_interval_ms_) {
    Packets packets;
    for (auto& processor : processors_) {
      processor.RunFor(simulation_interval_ms_, time_now_ms_, &packets);
    }

    // Verify packets are in order between batches.
    if (!packets.empty()) {
      if (!previous_packets_.empty()) {
        packets.splice(packets.begin(), previous_packets_,
                       --previous_packets_.end());
        ASSERT_TRUE(IsTimeSorted(packets));
        packets.erase(packets.begin());
      }
      ASSERT_LE(packets.front().send_time_us(), time_now_ms_ * 1000);
      ASSERT_LE(packets.back().send_time_us(), time_now_ms_ * 1000);
    } else {
      ASSERT_TRUE(IsTimeSorted(packets));
    }

    for (const auto& packet : packets) {
      EstimatorMap::iterator est_it = estimators_.find(packet.flow_id());
      ASSERT_TRUE(est_it != estimators_.end());
      est_it->second->EatPacket(packet);
    }

    for (const auto& estimator : estimators_) {
      GiveFeedbackToAffectedSenders(estimator.first, estimator.second);
    }
  }
}

string BweTest::GetTestName() const {
  const ::testing::TestInfo* const test_info =
      ::testing::UnitTest::GetInstance()->current_test_info();
  return string(test_info->name());
}
}  // namespace bwe
}  // namespace testing
}  // namespace webrtc
