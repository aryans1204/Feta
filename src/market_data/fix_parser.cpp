#include "fix_parser.h"
#include "quickfix/fix44/MarketDataSnapshotFullRefresh.h"
#include <algorithm>

namespace pascal {
    namespace market_data {
        uint64_t FIXMarketDataParser::get_messages_processed() const {
            return messaged_processed.load(std::memory_order_relaxed);
        }
        double FIXMarketDataParser::get_average_processing_time() const {
            return time_spent_processing.load(std::memory_order_relaxed)/messaged_processed.load(std::memory_order_relaxed);
        }
        void FIXMarketDataParser::parse_message(const FIX::Message& message, std::chrono::high_resolution_clock::time_point recv_time) {
            FIX::MsgType type;
            message.getField(type);
            if (type == FIX::MsgType_MarketDataSnapshotFullRefresh) {
                pascal::common::MarketDataSnapshot snapshot = parse_snapshot(message, recv_time);
                snapshotClbk(snapshot);
            }
            else if (type == FIX::MsgType_MarketDataIncrementalRefresh) {
                pascal::common::MarketDataIncrement update = parse_increment(message, recv_time);
                incrementalClbk(update);
            }
            else {
                pascal::common::MarketDataEntry trade = parse_raw_trade(message, recv_time);
                tradeClbk(trade);
            }
        }
        pascal::common::MarketDataSnapshot FIXMarketDataParser::parse_snapshot(const FIX::Message& message, std::chrono::high_resolution_clock::time_point recv_time) {
            FIX::Symbol symbol;
            message.getField(symbol);
            FIX::NoMDEntries numEntries;
            message.getField(numEntries);
            std::vector<pascal::common::PriceLevel> bids, asks;
            FIX44::MarketDataSnapshotFullRefresh::NoMDEntries group;
            FIX::MDEntryType MDEntryType;
            FIX::MDEntryPx MDEntryPx;
            FIX::MDEntrySize MDEntrySize;

            pascal::common::MarketDataSnapshot snapshot;
            snapshot.symbol = symbol;
            for (int i = 1; i <= numEntries; i++) {
                message.getGroup(i, group);
                group.get(MDEntryType);
                group.get(MDEntryPx);
                group.get(MDEntrySize);
                double price = MDEntryPx.getValue();
                double qty = MDEntrySize.getValue();
                pascal::common::Side side = static_cast<pascal::common::Side>(MDEntryType.getValue());
                if (side == pascal::common::Side::BID) bids.push_back(pascal::common::PriceLevel{Price: price, Quantity: qty});
                else asks.push_back(pascal::common::PriceLevel{Price: price, Quantity: qty});
            }

            snapshot.asks = std::move(asks);
            snapshot.bids = std::move(bids);
            snapshot.recv_time = recv_time;

            auto end_time = std::chrono::high_resolution_clock::now();
            uint64_t processing_time = std::chrono::duration_cast<std::chrono::microseconds>(end_time-recv_time).count();
            messaged_processed.fetch_add(1, std::memory_order_relaxed);
            time_spent_processing.fetch_add(processing_time, std::memory_order_relaxed);
            return snapshot;
        }
        pascal::common::MarketDataIncrement FIXMarketDataParser::parse_increment(const FIX::Message& message, std::chrono::high_resolution_clock::time_point recv_time) {
            FIX::Symbol symbol;
            message.getField(symbol);
            FIX::MDUpdateAction action;
            message.getField(action);
            FIX::NoMDEntries numEntries;
            message.getField(numEntries);
            std::vector<pascal::common::MarketDataEntry> entries;
            FIX44::MarketDataSnapshotFullRefresh::NoMDEntries group;
            FIX::MDEntryType MDEntryType;
            FIX::MDEntryPx MDEntryPx;
            FIX::MDEntrySize MDEntrySize;

            pascal::common::MarketDataIncrement update;
            update.symbol = symbol;
            update.recv_time = recv_time;
            for (int i = 1; i <= numEntries; i++) {
                message.getGroup(i, group);
                group.get(MDEntryType);
                group.get(MDEntryPx);
                group.get(MDEntrySize);
                double price = MDEntryPx.getValue();
                double qty = MDEntrySize.getValue();
                pascal::common::Side side = static_cast<pascal::common::Side>(MDEntryType.getValue());
                entries.push_back(pascal::common::MarketDataEntry{side: side, priceLevel: pascal::common::PriceLevel{Price: price, Quantity: qty}, update_action: static_cast<pascal::common::UpdateAction>(action.getValue())});
            }

            update.md_entries = std::move(entries);
            update.marketDepth = static_cast<uint32_t>(numEntries);
            return update;
        }

    }
}