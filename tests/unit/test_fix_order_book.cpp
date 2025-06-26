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
                increment.md_entries.push_back({.side = side, .update_action = update_action, .priceLevel = priceLevel});
                increment.recv-time = recv_time;
                increment.marketDepth = 1;
            }
        };

        CREATE_TEST_METHOD(FIXOrderBookTestFeature, "FIX Order Book - Initialize from Snapshot", "[fix_order_book]") {
            SECTION("Initialize from snapshot") {
                auto snapshot = create_test_snapshot();
                manager.process_snapshot(snapshot);

                auto book = manager.get_book_by_symbol("BTCUSDT");

                CHECK(book->get_total_bid_levels() == 3);
                CHECK(book->get_total_ask_levels() == 3);
                CHECK(book->get_best_bid() == {51000.1, 2.0});
                CHECK(book->get_best_ask() == {48005.1, 2.0});
                CHECK(book->get_bid_quantity_at_price(47005.6) == 1.4);
                CHECK(book->get_ask_quantity_at_price(51000.5) == 1.0);
            }
        }
        
        CREATE_TEST_METHOD(FIXOrderBookTestFeature, "FIX Order Book - Update from Increment", "[fix_order_book]") {
            auto snapshot = create_test_snapshot();
            manager.process_snapshot(snapshot);
            SECTION("Apply bid") {
                auto increment = create_test_increment("BTCUSDT", pascal::common::Side::BID, pascal::common::UpdateAction::NEW, {51000.1, 3.2});

                manager.process_increment(increment);
                auto book = manager.get_book_by_symbol("BTCUSDT");

                CHECK(book->get_total_bid_levels() == 3);
                CHECK(book->get_best_bid() == {51000.1, 5.2});
                CHECK(book->get_total_ask_levels() == 3);
            }
            SECTION("Apply ask") {
                auto increment = create_test_increment("BTCUSDT", pascal::common::Side::OFFER, pascal::common::UpdateAction::NEW, {48005.1, 3.2});

                manager.process_increment(increment);
                auto book = manager.get_book_by_symbol("BTCUSDT");

                CHECK(book->get_total_ask_levels() == 3);
                CHECK(book->get_best_ask() == {48005.1, 5.2});
                CHECK(book->get_total_bid_levels() == 3);
            }
            SECTION("Change bid") {
                auto increment = create_test_increment("BTCUSDT", pascal::common::Side::BID, pascal::common::UpdateAction::CHANGE, {51000.1, 3.2});

                manager.process_increment(increment);
                auto book = manager.get_book_by_symbol("BTCUSDT");

                CHECK(book->get_total_bid_levels() == 3);
                CHECK(book->get_best_bid() == {51000.1, 3.2});
                CHECK(book->get_total_ask_levels() == 3);
            }
            SECTION("Change ask") {
                auto increment = create_test_increment("BTCUSDT", pascal::common::Side::OFFER, pascal::common::UpdateAction::CHANGE, {48005.1, 3.2});

                manager.process_increment(increment);
                auto book = manager.get_book_by_symbol("BTCUSDT");

                CHECK(book->get_total_ask_levels() == 3);
                CHECK(book->get_best_ask() == {48005.1, 3.2});
                CHECK(book->get_total_bid_levels() == 3);
            }
            SECTION("Delete bid") {
                auto increment = create_test_increment("BTCUSDT", pascal::common::Side::BID, pascal::common::UpdateAction::DELETE, {51000.1, 1.4});

                manager.process_increment(increment);
                auto book = manager.get_book_by_symbol("BTCUSDT");

                CHECK(book->get_total_bid_levels() == 3);
                CHECK(book->get_best_bid() == {51000.1, 0.6});
                CHECK(book->get_total_ask_levels() == 3);
            }
            SECTION("Delete ask") {
                auto increment = create_test_increment("BTCUSDT", pascal::common::Side::OFFER, pascal::common::UpdateAction::DELETE, {48005.1, 1.4});

                manager.process_increment(increment);
                auto book = manager.get_book_by_symbol("BTCUSDT");

                CHECK(book->get_total_ask_levels() == 3);
                CHECK(book->get_best_ask() == {48005.1, 0.6});
                CHECK(book->get_total_bid_levels() == 3);
            }
        }
    }
}