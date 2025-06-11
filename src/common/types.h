#pragma once
#include <vector>
#include <string>
#include <chrono>

namespace pascal {
    namespace common {
        enum Side {
            BID = '0',
            OFFER,
            TRADE
        };

        enum UpdateAction {
            NEW = '0',
            CHANGE,
            DELETE
        };

        struct PriceLevel {
            double Price;
            int Quantity;
        };
        struct MarketDataEntry {
            Side side;
            PriceLevel priceLevel;
        };

        struct MarketDataIncrement {
            std::string symbol;
            std::vector<MarketDataEntry> md_entries;
            UpdateAction update_action;
            std::chrono::high_resolution_clock::time_point recv_time;
        };

        struct MarketDataSnapshot {
            std::string symbol;
            std::vector<PriceLevel> bids;
            std::vector<PriceLevel> asks;
            std::chrono::high_resolution_clock::time_point recv_time;
        };
    };
};