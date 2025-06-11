#pragma once
#include "common/types.h"
#include <vector>
#include <functional>
#include <atomic>

#include "quickfix/Message.h"

namespace pascal {
    namespace market_data {
        class FIXMarketDataParser {
        public:
            using SnapshotCallback = std::function<void(const pascal::common::MarketDataSnapshot& )>;
            using IncrementalCallback = std::function<void(const pascal::common::MarketDataIncrement& )>;
            using TradeCallback = std::function<void(const pascal::common::MarketDataEntry& )>;

            FIXMarketDataParser();
            ~FIXMarketDataParser();

            //Register callbacks
            void register_callback(const SnapshotCallback& clbk) {
                snapshotClbk = clbk;
            }
            void register_callback(const IncrementalCallback& clbk) {
                incrementalClbk = clbk;
            }
            void register_callback(const TradeCallback& clbk) {
                tradeClbk = clbk;
            }

            //Main entry point for FIX engine
            void parse_message(const FIX::Message& message, std::chrono::high_resolution_clock::time_point recv_time);

            //Performance tracking
            uint64_t get_messages_processed() const;
            double get_average_processing_time() const;

        private:
            SnapshotCallback snapshotClbk;
            IncrementalCallback incrementalClbk;
            TradeCallback tradeClbk;

            pascal::common::MarketDataSnapshot parse_snapshot(const FIX::Message& message, std::chrono::high_resolution_clock::time_point recv_time);
            pascal::common::MarketDataIncrement parse_increment(const FIX::Message& message, std::chrono::high_resolution_clock::time_point recv_time);
            pascal::common::MarketDataEntry parse_raw_trade(const FIX::Message& message, std::chrono::high_resolution_clock::time_point recv_time);

            std::atomic<uint64_t> messaged_processed{0};
            std::atomic<uint64_t> time_spent_processing{1};
        };
    };
};