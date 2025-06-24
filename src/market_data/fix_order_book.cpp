#include "market_data/fix_order_book.h"
#include <algorithm>
#include <mutex>

namespace pascal {
    namespace market_data {
        void FIXOrderBook::initialize_from_snapshot(const pascal::common::MarketDataSnapshot& snapshot) {
            uint64_t newVersion = version_.load()+1;
            bids = std::move(snapshot.bids);
            std::sort(bids.begin(), bids.end(), [](auto a, auto b) {
                return a.Price < b.Price;
            });
            asks = std::move(snapshot.asks);
            std::sort(bids.begin(), bids.end(), [](auto a, auto b) {
                return a.Price > b.Price;
            });
            is_synchronized_.store(true, std::memory_order_release);
            total_updates_processed.fetch_add(1, std::memory_order_release);
            last_update_time = std::chrono::high_resolution_clock::now();
            version_.store(newVersion, std::memory_order_release);
        }
        void FIXOrderBook::update_from_increment(const pascal::common::MarketDataIncrement& update) {
            uint64_t newVersion = version_.load()+1;
            if (update.marketDepth == 1) {
                auto md = update.md_entries.front();
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
            else {
                for (auto md : update.md_entries) {
                    std::vector<pascal::common::PriceLevel>::iterator it;
                    if (md.side == pascal::common::BID) {
                        auto priceLevel = md.priceLevel;
                        it = std::find_if(bids.begin(), bids.end(), [priceLevel](auto& price) {
                            return priceLevel.Price <= price.Price;
                        });
                    }
                    else {
                        auto priceLevel = md.priceLevel;
                        it = std::find_if(asks.begin(), asks.end(), [priceLevel](auto& price) {
                            return priceLevel.Price >= price.Price;
                        });
                    }

                    switch (md.update_action) {
                        case pascal::common::UpdateAction::NEW :
                            if (md.side == pascal::common::BID) {
                                if (it == bids.end()) {
                                    bids.emplace_back(md.priceLevel);
                                }
                                else if (it->Price == md.priceLevel.Price) {
                                    it->Quantity += md.priceLevel.Quantity;
                                }
                                else {
                                    bids.emplace(it, md.priceLevel);
                                }
                            }
                            else {
                                if (it == asks.end()) {
                                    asks.emplace_back(md.priceLevel);
                                }
                                else if (it->Price == md.priceLevel.Price) {
                                    it->Quantity += md.priceLevel.Quantity;
                                }
                                else {
                                    asks.emplace(it, md.priceLevel);
                                }
                            }
                            break;

                        case pascal::common::UpdateAction::DELETE :
                            it->Quantity -= md.priceLevel.Quantity;
                            if (md.side == pascal::common::BID) {
                                if (it->Quantity == 0) bids.erase(it);
                            }
                            else {
                                if (it->Quantity == 0) asks.erase(it);
                            }
                            break;

                        case pascal::common::UpdateAction::CHANGE :
                            it->Quantity = md.priceLevel.Quantity;
                            break; 
                    }
                }
            }
            is_synchronized_.store(true, std::memory_order_relaxed);
            total_updates_processed.fetch_add(1, std::memory_order_relaxed);
            last_update_time = std::chrono::high_resolution_clock::now();
            version_.store(newVersion, std::memory_order_release);
        }
        
        pascal::common::PriceLevel FIXOrderBook::get_best_bid() const {
            uint64_t v1, v2;
            pascal::common::PriceLevel result;
            do {
                v1 = version_.load(std::memory_order_acquire);
                result = bids.back();
                v2 = version_.load(std::memory_order_acquire);
            } while (v1 != v2);

            return result;
        }
        pascal::common::PriceLevel FIXOrderBook::get_best_ask() const {
            uint64_t v1, v2;
            pascal::common::PriceLevel result;
            do {
                v1 = version_.load(std::memory_order_acquire);
                result = asks.back();
                v2 = version_.load(std::memory_order_acquire);
            } while (v1 != v2);

            return result;
        }
        std::vector<pascal::common::PriceLevel> FIXOrderBook::get_bids(size_t depth = 10) const {
            uint64_t v1, v2;
            do {
                v1 = version_.load(std::memory_order_acquire);
                v2 = version_.load(std::memory_order_acquire);
            } while (v1 != v2);
            
            return bids;
        }
        std::vector<pascal::common::PriceLevel> FIXOrderBook::get_asks(size_t depth = 10) const {
            uint64_t v1, v2;
            do {
                v1 = version_.load(std::memory_order_acquire);
                v2 = version_.load(std::memory_order_acquire);
            } while (v1 != v2);
            
            return asks;
        }
        double FIXOrderBook::get_bid_quantity_at_price(double price) {
            uint64_t v1, v2;
            double quantity = 0;
            do {
                v1 = version_.load(std::memory_order_acquire);
                auto it = std::find_if(bids.begin(), bids.end(), [price](auto priceLvl) {
                    return priceLvl.Price == price;
                });
                quantity = it->Quantity;
                v2 = version_.load(std::memory_order_acquire);
            } while (v1 != v2);
            
            return quantity;
        }
        double FIXOrderBook::get_ask_quantity_at_price(double price) {
            uint64_t v1, v2;
            double quantity = 0;
            do {
                v1 = version_.load(std::memory_order_acquire);
                auto it = std::find_if(asks.begin(), asks.end(), [price](auto priceLvl) {
                    return priceLvl.Price == price;
                });
                quantity = it->Quantity;
                v2 = version_.load(std::memory_order_acquire);
            } while (v1 != v2);
            
            return quantity;
        }
        bool FIXOrderBook::is_synchronized() const {
            return is_synchronized_.load(std::memory_order_acquire);
        }
        std::chrono::high_resolution_clock::time_point FIXOrderBook::get_last_update_time() const {
            return last_update_time;
        }
        size_t FIXOrderBook::get_total_bid_levels() const {
            uint64_t v1, v2;
            size_t size;
            do {
                v1 = version_.load(std::memory_order_acquire);
                size = bids.size();
                v2 = version_.load(std::memory_order_acquire);
            } while (v1 != v2);

            return size;
        }
        size_t FIXOrderBook::get_total_ask_levels() const {
            uint64_t v1, v2;
            size_t size;
            do {
                v1 = version_.load(std::memory_order_acquire);
                size = asks.size();
                v2 = version_.load(std::memory_order_acquire);
            } while (v1 != v2);

            return size;
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