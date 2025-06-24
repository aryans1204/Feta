#pragma once
#include "common/types.h"
#include <shared_mutex>
#include <atomic>
#include <vector>
#include <unordered_map>
#include <memory>
#include <algorithm>


#define MAX_ORDERS 10000

namespace pascal {
    namespace market_data {
        class FIXOrderBook {
        public:

            FIXOrderBook(const std::string& symbol) : symbol(symbol) {
                //prevent resizing
                bids.reserve(MAX_ORDERS);
                asks.reserve(MAX_ORDERS);
            }
            
            //Book reconstruction interface
            void initialize_from_snapshot(const pascal::common::MarketDataSnapshot& snapshot);
            void update_from_increment(const pascal::common::MarketDataIncrement& update);

            //Query interface
            pascal::common::PriceLevel get_best_bid() const;
            pascal::common::PriceLevel get_best_ask() const;
            std::vector<pascal::common::PriceLevel> get_bids(size_t depth = 10) const;
            std::vector<pascal::common::PriceLevel> get_asks(size_t depth = 10) const;
            double get_bid_quantity_at_price(double price);
            double get_ask_quantity_at_price(double price);

            //Book state
            bool is_synchronized() const;
            std::chrono::high_resolution_clock::time_point get_last_update_time() const;

            //Statistics
            size_t get_total_bid_levels() const;
            size_t get_total_ask_levels() const;
            uint64_t get_total_updates_processed() const;

        private:
            using BidMap = std::vector<pascal::common::PriceLevel>; //ascending bids
            using AskMap = std::vector<pascal::common::PriceLevel>; //descending asks

            std::atomic<uint64_t> version_{0};
            std::string symbol;
            BidMap bids;
            AskMap asks;

            std::atomic<bool> is_synchronized_{false};
            std::atomic<uint64_t> total_updates_processed{0};
            std::chrono::high_resolution_clock::time_point last_update_time;

            inline void apply_price_level(pascal::common::Side side, const pascal::common::PriceLevel& priceLevel) {
                if (side == pascal::common::Side::BID) {
                    auto bestIt = bids.end()-1;
                    if (bestIt->Price == priceLevel.Price) {
                        bestIt->Quantity += priceLevel.Quantity;
                    }
                    else {
                        bids.emplace_back(priceLevel);
                    }
                }
                else {
                    auto bestIt = asks.end()-1;
                    if (bestIt->Price == priceLevel.Price) {
                        bestIt->Quantity += priceLevel.Quantity;
                    }
                    else {
                        asks.emplace_back(priceLevel);
                    }
                }
            }
            inline void delete_price_level(pascal::common::Side side, const pascal::common::PriceLevel& priceLevel) {
                if (side == pascal::common::Side::BID) {
                    auto bestIt = bids.end()-1;
                    bestIt->Quantity -= priceLevel.Quantity;
                    if (!bestIt->Quantity) {
                        bids.pop_back();
                    }
                }
                else {
                    auto bestIt = asks.end()-1;
                    bestIt->Quantity -= priceLevel.Quantity;
                    if (!bestIt->Quantity) {
                        asks.pop_back();
                    }
                }
            }
            inline void change_best_quote(pascal::common::Side side, const pascal::common::PriceLevel& priceLevel) {
                if (side == pascal::common::Side::BID) {
                    auto bestIt = bids.end()-1;
                    *bestIt = priceLevel;
                }
                else {
                    auto bestIt = asks.rbegin();
                    *bestIt = priceLevel;
                }
            }
            inline void change_quote(pascal::common::Side side, const pascal::common::PriceLevel& priceLevel) {
                if (side == pascal::common::Side::BID) {
                    auto it = std::find_if(bids.begin(), bids.end(), [priceLevel](auto& a) {
                        return a.Price == priceLevel.Price;
                    });
                    if (priceLevel.Quantity == 0) {
                        bids.erase(it);
                    }
                    else {
                        it->Quantity = priceLevel.Quantity;
                    }
                }
                else {
                    auto it = std::find_if(asks.begin(), asks.end(), [priceLevel](auto& a) {
                        return a.Price == priceLevel.Price;
                    });
                    if (priceLevel.Quantity == 0) {
                        asks.erase(it);
                    }
                    else {
                        it->Quantity = priceLevel.Quantity;
                    }
                }
            }

        };

        class FIXOrderBookManager {
        public:
            FIXOrderBookManager();

            //Book manager
            void add_symbol(const std::string& symbol);
            void remove_symbol(const std::string& symbol);

            //Book processors
            void process_snapshot(const pascal::common::MarketDataSnapshot& snapshot);
            void process_increment(const pascal::common::MarketDataIncrement& update);

            //Query interface
            std::shared_ptr<FIXOrderBook> get_book_by_symbol(const std::string& symbol);
            std::vector<std::string> get_symbols() const;

            //Statistics
            size_t get_total_books() const;
            uint64_t get_total_updates_processed() const;

        private:
            std::unordered_map<std::string, std::shared_ptr<FIXOrderBook>> books;
            mutable std::shared_mutex book_mtx;

            std::atomic<uint64_t> total_updates_processed{0};

        };
    };
};