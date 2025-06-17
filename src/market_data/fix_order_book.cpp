#include "market_data/fix_order_book.h"
#include <algorithm>
#include <mutex>

namespace pascal {
    namespace market_data {
        void FIXOrderBook::initialize_from_snapshot(const pascal::common::MarketDataSnapshot& snapshot) {
            std::unique_lock(book_mtx);
            for (auto priceLevel : snapshot.bids) {
                bids[priceLevel.Price] = priceLevel.Quantity;
            }
            for (auto priceLevel: snapshot.asks) {
                asks[priceLevel.Price] = priceLevel.Quantity;
            }
            is_synchronized_.store(true, std::memory_order_relaxed);
            total_updates_processed.fetch_add(1, std::memory_order_relaxed);
            last_update_time = std::chrono::high_resolution_clock::now();
        }
        void FIXOrderBook::update_from_increment(const pascal::common::MarketDataIncrement& update) {
            std::unique_lock(book_mtx);
            for (auto md: update.md_entries) {
                switch (md.update_action) {
                    case pascal::common::UpdateAction::NEW :
                        apply_price_level(md.side, md.priceLevel);
                        break;

                    case pascal::common::UpdateAction::DELETE :
                        delete_price_level(md.side, md.priceLevel);
                        break;

                    case pascal::common::UpdateAction::CHANGE :
                        if (update.marketDepth == 1) {
                            change_best_quote(md.side, md.priceLevel);
                        }
                        else {
                            change_quote(md.side, md.priceLevel);
                        }
                        break; 
                }
            }
            is_synchronized_.store(true, std::memory_order_relaxed);
            total_updates_processed.fetch_add(1, std::memory_order_relaxed);
            last_update_time = std::chrono::high_resolution_clock::now();
        }
        void FIXOrderBook::apply_price_level(pascal::common::Side side, const pascal::common::PriceLevel& priceLevel) {
            if (side == pascal::common::Side::BID) {
                bids[priceLevel.Price] += priceLevel.Quantity;
            }
            else {
                asks[priceLevel.Price] += priceLevel.Quantity;
            }
        }
        void FIXOrderBook::delete_price_level(pascal::common::Side side, const pascal::common::PriceLevel& priceLevel) {
            if (side == pascal::common::Side::BID) {
                bids[priceLevel.Price] -= priceLevel.Quantity;
                if (!bids[priceLevel.Price]) bids.erase(priceLevel.Price);
            }
            else {
                asks[priceLevel.Price] -= priceLevel.Quantity;
                if (!asks[priceLevel.Price]) asks.erase(priceLevel.Price);
            }
        }
        void FIXOrderBook::change_best_quote(pascal::common::Side side, const pascal::common::PriceLevel& priceLevel) {
            if (side == pascal::common::Side::BID) {
                auto iter = bids.begin();
                auto priceQty = *iter;
                bids.erase(priceQty.first);
                bids[priceLevel.Price] = priceLevel.Quantity;
            }
            else {
                auto iter = asks.begin();
                auto priceQty = *iter;
                asks.erase(priceQty.first);
                asks[priceLevel.Price] = priceLevel.Quantity;
            }
        }
        void FIXOrderBook::change_quote(pascal::common::Side side, const pascal::common::PriceLevel& priceLevel) {
            if (side == pascal::common::Side::BID) {
                if (priceLevel.Quantity == 0) {
                    bids.erase(priceLevel.Price);
                }
                else {
                    bids[priceLevel.Price] = priceLevel.Quantity;
                }
            }
            else {
                if (priceLevel.Quantity == 0) {
                    asks.erase(priceLevel.Price);
                }
                else {
                    asks[priceLevel.Price] = priceLevel.Quantity;
                }
            }
        }
        pascal::common::PriceLevel FIXOrderBook::get_best_bid() const {
            std::shared_lock(book_mtx);
            return pascal::common::PriceLevel{Price: bids.begin()->first, Quantity: bids.begin()->second};
        }
        pascal::common::PriceLevel FIXOrderBook::get_best_ask() const {
            std::shared_lock(book_mtx);
            return pascal::common::PriceLevel{Price: asks.begin()->first, Quantity: asks.begin()->second};
        }
        std::vector<pascal::common::PriceLevel> FIXOrderBook::get_bids(size_t depth = 10) const {
            std::shared_lock(book_mtx);
            std::vector<pascal::common::PriceLevel> prices;
            auto it = bids.begin();
            for (int i = 0; i < depth; i++) {
                prices.push_back(pascal::common::PriceLevel{Price: it->first, Quantity: it->second});
                it++;
                if (it == bids.end()) return prices;
            }
            return prices;
        }
        std::vector<pascal::common::PriceLevel> FIXOrderBook::get_asks(size_t depth = 10) const {
            std::shared_lock(book_mtx);
            std::vector<pascal::common::PriceLevel> prices;
            auto it = asks.begin();
            for (int i = 0; i < depth; i++) {
                prices.push_back(pascal::common::PriceLevel{Price: it->first, Quantity: it->second});
                it++;
                if (it == asks.end()) return prices;
            }
            return prices;
        }
        double FIXOrderBook::get_bid_quantity_at_price(double price) {
            std::shared_lock(book_mtx);
            return bids[price];
        }
        double FIXOrderBook::get_ask_quantity_at_price(double price) {
            std::shared_lock(book_mtx);
            return asks[price];
        }
        bool FIXOrderBook::is_synchronized() const {
            return is_synchronized_.load(std::memory_order_relaxed);
        }
        std::chrono::high_resolution_clock::time_point FIXOrderBook::get_last_update_time() const {
            return last_update_time;
        }
        size_t FIXOrderBook::get_total_bid_levels() const {
            std::shared_lock(book_mtx);
            return bids.size();
        }
        size_t FIXOrderBook::get_total_ask_levels() const {
            std::shared_lock(book_mtx);
            return asks.size();
        }
        uint64_t FIXOrderBook::get_total_updates_processed() const {
            return total_updates_processed.load(std::memory_order_relaxed);
        }

        void FIXOrderBookManager::add_symbol(const std::string& symbol) {
            std::unique_lock(book_mtx);
            books[symbol] = std::make_shared<FIXOrderBook>();
        }
        void FIXOrderBookManager::remove_symbol(const std::string& symbol) {
            std::unique_lock(book_mtx);
            books.erase(symbol);
        }
        void FIXOrderBookManager::process_snapshot(const pascal::common::MarketDataSnapshot& snapshot) {
            std::string symbol = snapshot.symbol;
            books[symbol]->initialize_from_snapshot(snapshot);
            total_updates_processed.fetch_add(1, std::memory_order_relaxed);
        }
        void FIXOrderBookManager::process_increment(const pascal::common::MarketDataIncrement& update) {
            std::string symbol = update.symbol;
            books[symbol]->update_from_increment(update);
            total_updates_processed.fetch_add(1, std::memory_order_relaxed);
        }
        std::shared_ptr<FIXOrderBook> FIXOrderBookManager::get_book_by_symbol(const std::string& symbol) {
            std::shared_lock(book_mtx);
            return books[symbol];
        }   
        std::vector<std::string> FIXOrderBookManager::get_symbols() const {
            std::shared_lock(book_mtx);
            std::vector<std::string> symbols(books.size());
            std::transform(books.begin(), books.end(), symbols.begin(), [](auto a) {
                return a.first;
            });
            return symbols;
        }
        size_t FIXOrderBookManager::get_total_books() const {
            std::shared_lock(book_mtx);
            return books.size();
        }
        uint64_t FIXOrderBookManager::get_total_updates_processed() const {
            return total_updates_processed.load(std::memory_order_relaxed);
        }
    }   
}