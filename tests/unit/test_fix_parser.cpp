#include "catch2/catch_test_macros.hpp"
#include "catch2/catch_approx.hpp"
#include "market_data/fix_parser.h"
#include <thread>
#include <vector>
#include "quickfix/fix44/MarketDataIncrementalRefresh.h"
#include "quickfix/fix44/MarketDataSnapshotFullRefresh.h"
#include "common/types.h"
#include <chrono>
#include <string>

namespace pascal {
    namespace test {

        class FIXParserTestFeature {
        public:
            pascal::market_data::FIXParser parser;
            std::vector<pascal::common::MarketDataSnapshot> snapshots;
            std::vector<pascal::common::MarketDataIncrement> increments;

            FIXParserTestFeature() {
                parser.register_callback([this](const pascal::common::MarketDataSnapshot& snapshot) {
                    snapshots.push_back(snapshot);
                });

                parser.register_callback([this](const pascal::common::MarketDataIncrement& increment) {
                    increments.push_back(increment);
                });

            }

            FIX44::MarketDataSnapshotFullRefresh create_test_snapshot(
                const std::string& symbol = "BTCUSDT",
                const std::vector<std::pair<double, double>>& bids = {{50000.0, 1.5}, {49999.0, 2.0}},
                const std::vector<std::pair<double, double>>& asks = {{50001.0, 1.2}, {50002.0, 1.8}}) {
                
                FIX44::MarketDataSnapshotFullRefresh message;
                message.setField(FIX::Symbol(symbol));
                
                int entryCount = 0;
                
                // Add bids
                for (const auto& [price, qty] : bids) {
                    FIX44::MarketDataSnapshotFullRefresh::NoMDEntries group;
                    group.setField(FIX::MDEntryType('0')); // Bid
                    group.setField(FIX::MDEntryPx(price));
                    group.setField(FIX::MDEntrySize(qty));
                    message.addGroup(group);
                    entryCount++;
                }
                
                // Add asks
                for (const auto& [price, qty] : asks) {
                    FIX44::MarketDataSnapshotFullRefresh::NoMDEntries group;
                    group.setField(FIX::MDEntryType('1')); // Ask
                    group.setField(FIX::MDEntryPx(price));
                    group.setField(FIX::MDEntrySize(qty));
                    message.addGroup(group);
                    entryCount++;
                }
                
                message.setField(FIX::NoMDEntries(entryCount));
                return message;
            }
            
            FIX44::MarketDataIncrementalRefresh create_test_increment(
                const std::string& symbol = "BTCUSDT",
                char updateAction = '0', // NEW
                char entryType = '0', // BID
                double price = 50000.5,
                double qty = 1.0) {
                
                FIX44::MarketDataIncrementalRefresh message;
                message.setField(FIX::Symbol(symbol));
                message.setField(FIX::MDUpdateAction(updateAction));
                message.setField(FIX::NoMDEntries(1));
                
                FIX44::MarketDataIncrementalRefresh::NoMDEntries group;
                group.setField(FIX::MDEntryType(entryType));
                group.setField(FIX::MDEntryPx(price));
                group.setField(FIX::MDEntrySize(qty));
                message.addGroup(group);
                
                return message;
            }
        };

        CREATE_TEST_METHOD(FIXParserTestFeature, "FIX Parser - Snapshot Parsing", "[fix_parser]") {
            SECTION("Parse valid snapshot message") {
                auto testSnapshot = create_test_snapshot();
                auto recv_time = std::chrono::high_resolution_clock::now();
                parser.parse_message(testSnapshot, recv_time);

                REQUIRE(snapshots.size() == 1);
                auto& snapshot = snapshots[0];
                
                CHECK(snapshot.symbol == "BTCUSDT");
                CHECK(snapshot.bids.size() == 2);
                CHECK(snapshot.asks.size() == 2);
                CHECK(snapshot.bids[0].Price == 50000.0);
                CHECK(snapshot.bids[0].Quantity == 1.5);
                CHECK(snapshot.asks[0].Price == 50001.0);
                CHECK(snapshot.asks[0].Quantity == 1.2);
            }
            SECTION("Parse empty snapshot message") {
                auto testSnapshot = create_test_snapshot("ETHUSDT", {}, {});
                auto recv_time = std::chrono::high_resolution_clock::now();
                parser.parse_message(testSnapshot, recv_time);

                REQUIRE(snapshots.size() == 1);
                auto& snapshot = snapshots[0];

                CHECK(snapshot.symbol == "ETHUSDT");
                CHECK(snapshot.bids.empty());
                CHECK(snapshot.asks.empty());
            }
        }

        CREATE_TEST_METHOD(FIXParserTestFeature, "FIX Parser - Increment Parsing", "[fix_parser]") {
            SECTION("Parse bid update") {
                auto testIncrement = create_test_increment();

                auto recv_time = std::chrono::high_resolution_clock::now();
                parser.parse_message(testSnapshot, recv_time);

                REQUIRE(increments.size() == 1);
                auto& increment = increments[0];

                CHECK(increment.symbol == "BTCUSDT");
                CHECK(increment.marketDepth == 1);
                CHECK(increment.md_entries[0].update_action == pascal::common::UpdateAction::NEW);
                CHECK(increment.md_entries[0].side == pascal::common::Side::BID);
                CHECK(increment.md_entries[0].priceLevel.Price == 50000.5);
                CHECK(increment.md_entries[0].priceLevel.Quantity == 1.0);
            }
            SECTION("Parse ask update") {
                auto testIncrement = create_test_increment("ETHUSDT", '0', '1', 500001.0, 1.0);

                auto recv_time = std::chrono::high_resolution_clock::now();
                parser.parse_message(testSnapshot, recv_time);

                REQUIRE(increments.size() == 1);
                auto& increment = increments[0];

                CHECK(increment.md_entries[0].side == pascal::common::SIde::OFFER);
            }
        }
    }
}