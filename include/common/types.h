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
            double Quantity;
        };
        struct MarketDataEntry {
            Side side;
            PriceLevel priceLevel;
            UpdateAction update_action;
        };

        struct MarketDataIncrement {
            std::string symbol;
            std::vector<MarketDataEntry> md_entries;
            std::chrono::high_resolution_clock::time_point recv_time;
            uint32_t marketDepth;
        };

        struct MarketDataSnapshot {
            std::string symbol;
            std::vector<PriceLevel> bids;
            std::vector<PriceLevel> asks;
            std::chrono::high_resolution_clock::time_point recv_time;
        };

        enum MarketDataSubscriptionType {
            RAW_TRADE,
            TOP_OF_BOOK,
            FULL_BOOK
        };

        struct MarketDataRequest {
            MarketDataSubscriptionType Stream;
            std::string Symbol;
            int MarketDepth;
            Side MDEntryType;
            char Subscribe;
            std::string ReqID;
        };
    };
};