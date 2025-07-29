#include "catch2/catch_test_macros.hpp"
#include "catch2/catch_approx.hpp"
#include "market_data/fix_order_book.h"
#include <thread>
#include <vector>
#include "quickfix/fix44/MarketDataIncrementalRefresh.h"
#include "quickfix/fix44/MarketDataSnapshotFullRefresh.h"
#include "common/types.h"
#include <chrono>
#include <string>
#include <iostream>

namespace pascal {
    namespace test {

        class FIXOrderBookTestFeature {
        public:
            pascal::market_data::FIXOrderBookManager manager;

            FIXOrderBookTestFeature() {
                manager.add_symbol("BTCUSDT");
            }

            pascal::common::MarketDataSnapshot create_test_snapshot(
                const std::string& symbol = "BTCUSDT",
                std::vector<pascal::common::PriceLevel> bids = {{50000.5, 1.0}, {51000.1, 2.0}, {47005.6, 1.4}},
                std::vector<pascal::common::PriceLevel> asks = {{51000.5, 1.0}, {48005.1, 2.0}, {50005.6, 1.4}}
            ) {
                auto recv_time = std::chrono::high_resolution_clock::now();
                return pascal::common::MarketDataSnapshot{.symbol = symbol, .bids = bids, .asks = asks, .recv_time = recv_time};
            }

            pascal::common::MarketDataIncrement create_test_increment(
                const std::string& symbol,
                const pascal::common::Side& side,
                const pascal::common::UpdateAction& update_action,
                const pascal::common::PriceLevel& priceLevel
            )  
            {
                auto recv_time = std::chrono::high_resolution_clock::now();
                pascal::common::MarketDataIncrement increment;
                increment.symbol = symbol;
                increment.md_entries.push_back(pascal::common::MarketDataEntry{.side = side, .priceLevel = priceLevel, .update_action = update_action});
                increment.recv_time = recv_time;
                increment.marketDepth = 1;
                return increment;
            }
            pascal::common::MarketDataIncrement create_test_increments(
                const std::string& symbol,
                const std::vector<pascal::common::MarketDataEntry> md_entries
            )
            {
                auto recv_time = std::chrono::high_resolution_clock::now();
                pascal::common::MarketDataIncrement increment;
                increment.symbol = symbol;
                increment.md_entries = md_entries;
                increment.recv_time = recv_time;
                increment.marketDepth = md_entries.size();
                return increment;
            }
        };

        TEST_CASE("FIX Order Book - Initialize from Snapshot", "[fix_order_book]") {
            FIXOrderBookTestFeature feature;
            SECTION("Initialize from snapshot") {
                auto snapshot = feature.create_test_snapshot();
                feature.manager.process_snapshot(snapshot);

                auto book = feature.manager.get_book_by_symbol("BTCUSDT");

                CHECK(book->get_total_bid_levels() == 3);
                CHECK(book->get_total_ask_levels() == 3);
                CHECK(book->get_best_bid().Price == 51000.1);
                CHECK(book->get_best_bid().Quantity == 2.0);
                CHECK(book->get_best_ask().Price == 48005.1);
                CHECK(book->get_best_ask().Quantity == 2.0);
                CHECK(book->get_bid_quantity_at_price(47005.6) == 1.4);
                CHECK(book->get_ask_quantity_at_price(51000.5) == 1.0);
            }
        }
        
        TEST_CASE("FIX Order Book - Update from Increment", "[fix_order_book]") {
            FIXOrderBookTestFeature feature;
            auto snapshot = feature.create_test_snapshot();
            feature.manager.process_snapshot(snapshot);
            SECTION("Apply bid") {
                auto increment = feature.create_test_increment("BTCUSDT", pascal::common::Side::BID, pascal::common::UpdateAction::NEW, pascal::common::PriceLevel{51000.1, 3.2});
                
                feature.manager.process_increment(increment);
                auto book = feature.manager.get_book_by_symbol("BTCUSDT");

                CHECK(book->get_total_bid_levels() == 3);
                CHECK(book->get_best_bid().Price == 51000.1);
                CHECK(book->get_best_bid().Quantity == 5.2);
                CHECK(book->get_total_ask_levels() == 3);
            }
            SECTION("Apply ask") {
                auto increment = feature.create_test_increment("BTCUSDT", pascal::common::Side::OFFER, pascal::common::UpdateAction::NEW, pascal::common::PriceLevel{48005.1, 3.2});

                feature.manager.process_increment(increment);
                auto book = feature.manager.get_book_by_symbol("BTCUSDT");

                CHECK(book->get_total_ask_levels() == 3);
                CHECK(book->get_best_ask().Price == 48005.1);
                CHECK(book->get_best_ask().Quantity == 5.2);
                CHECK(book->get_total_bid_levels() == 3);
            }
            SECTION("Change bid") {
                auto increment = feature.create_test_increment("BTCUSDT", pascal::common::Side::BID, pascal::common::UpdateAction::CHANGE, pascal::common::PriceLevel{51000.1, 3.2});

                feature.manager.process_increment(increment);
                auto book = feature.manager.get_book_by_symbol("BTCUSDT");

                CHECK(book->get_total_bid_levels() == 3);
                CHECK(book->get_best_bid().Price == 51000.1);
                CHECK(book->get_best_bid().Quantity == 3.2);
                CHECK(book->get_total_ask_levels() == 3);
            }
            SECTION("Change ask") {
                auto increment = feature.create_test_increment("BTCUSDT", pascal::common::Side::OFFER, pascal::common::UpdateAction::CHANGE, pascal::common::PriceLevel{48005.1, 3.2});

                feature.manager.process_increment(increment);
                auto book = feature.manager.get_book_by_symbol("BTCUSDT");

                CHECK(book->get_total_ask_levels() == 3);
                CHECK(book->get_best_ask().Price == 48005.1);
                CHECK(book->get_best_ask().Quantity == 3.2);
                CHECK(book->get_total_bid_levels() == 3);
            }
            SECTION("Delete bid") {
                auto increment = feature.create_test_increment("BTCUSDT", pascal::common::Side::BID, pascal::common::UpdateAction::DELETE, pascal::common::PriceLevel{51000.1, 1.4});

                feature.manager.process_increment(increment);
                auto book = feature.manager.get_book_by_symbol("BTCUSDT");

                CHECK(book->get_total_bid_levels() == 3);
                CHECK(book->get_best_bid().Price == 51000.1);
                CHECK(book->get_best_bid().Quantity == Catch::Approx(0.6));
                CHECK(book->get_total_ask_levels() == 3);
            }
            SECTION("Delete ask") {
                auto increment = feature.create_test_increment("BTCUSDT", pascal::common::Side::OFFER, pascal::common::UpdateAction::DELETE, pascal::common::PriceLevel{48005.1, 1.4});

                feature.manager.process_increment(increment);
                auto book = feature.manager.get_book_by_symbol("BTCUSDT");

                CHECK(book->get_total_ask_levels() == 3);
                CHECK(book->get_best_ask().Price == 48005.1);
                CHECK(book->get_best_ask().Quantity == Catch::Approx(0.6));
                CHECK(book->get_total_bid_levels() == 3);
            }
        }
        TEST_CASE("FIX Order Book - Update from Multiple Increment", "[fix_order_book]") {
            FIXOrderBookTestFeature feature;
            auto snapshot = feature.create_test_snapshot();
            feature.manager.process_snapshot(snapshot);
            pascal::common::MarketDataEntry e1, e2, e3;
            e2.side = pascal::common::Side::BID;
            e2.update_action = pascal::common::UpdateAction::NEW;
            e2.priceLevel = {52000.1, 3.2};
            e1.side = pascal::common::Side::OFFER;
            e1.update_action = pascal::common::UpdateAction::CHANGE;
            e1.priceLevel = {48005.1, 3.2};
            e3.side = pascal::common::Side::BID;
            e3.update_action = pascal::common::UpdateAction::DELETE;
            e3.priceLevel = {51000.1, 2.0};
            SECTION("Apply increments") {
                auto increment = feature.create_test_increments("BTCUSDT", {e1, e2, e3});
                
                feature.manager.process_increment(increment);
                auto book = feature.manager.get_book_by_symbol("BTCUSDT");

                CHECK(book->get_total_bid_levels() == 3);
                CHECK(book->get_best_bid().Price == 52000.1);
                CHECK(book->get_best_bid().Quantity == 3.2);
                CHECK(book->get_total_ask_levels() == 3);
                CHECK(book->get_best_ask().Price == 48005.1);
                CHECK(book->get_best_ask().Quantity == 3.2);
            }
        }
    }
}