#include "catch2/catch_test_macros.hpp"
#include "catch2/catch_approx.hpp"
#include "net/fix_engine.h"
#include <thread>
#include <vector>
#include "quickfix/fix44/MarketDataIncrementalRefresh.h"
#include "quickfix/fix44/MarketDataSnapshotFullRefresh.h"
#include "common/types.h"
#include <chrono>
#include <string>
#include <cstdlib>

namespace pascal {
    namespace test {
        TEST_CASE("FIX Engine - Application lifecycle", "[fix_engine]") {
            SECTION("Test session start and logon") {
                std::string fixConfig = std::string(std::getenv("BINANCE_FIX_CONFIG"));
                std::string api_key = std::string(std::getenv("BINANCE_API_KEY"));
                std::string private_key_pem = std::string(std::getenv("BINANCE_PRIVATE_KEY_PATH"));
                std::vector<std::string> tradedSymbols = {"BTCUSDT"};
                pascal::net::FIXMarketDataEngine engine(fixConfig, private_key_pem, api_key, tradedSymbols);

                engine.start();
                std::this_thread::sleep_for(std::chrono::milliseconds(10000));
                CHECK(engine.is_logged());
                engine.stop();
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                CHECK(!engine.is_logged());
            }
        }
        TEST_CASE("FIX Engine - Symbol subscription", "[fix_engine]") {
            SECTION("Limit Order Book Subscribe") {
                std::string fixConfig = std::string(std::getenv("BINANCE_FIX_CONFIG"));
                std::string api_key = std::string(std::getenv("BINANCE_API_KEY"));
                std::string private_key_pem = std::string(std::getenv("BINANCE_PRIVATE_KEY_PATH"));
                std::vector<std::string> tradedSymbols = {"BTCUSDT"};
                pascal::net::FIXMarketDataEngine engine(fixConfig, private_key_pem, api_key, tradedSymbols);

                engine.start();
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                CHECK(engine.is_logged());
                pascal::common::MarketDataRequest req;
                req.Stream = pascal::common::MarketDataSubscriptionType::FULL_BOOK;
                req.Symbol = "BTCUSDT";
                req.MarketDepth = 100;
                req.MDEntryType = pascal::common::BID;
                std::string symbol = "DEF";
                int bidSize = 0;
                int askSize = -1;
                engine.register_parser_callback([&symbol, &bidSize, &askSize](const pascal::common::MarketDataSnapshot& snapshot) {
                    symbol = snapshot.symbol;
                    bidSize = snapshot.bids.size();
                    askSize = snapshot.asks.size();
                });
                engine.sub_to_symbol(req);
                std::this_thread::sleep_for(std::chrono::milliseconds(150));
                CHECK(symbol == "BTCUSDT");
                CHECK(bidSize == 100);
                CHECK(askSize == 0);
                engine.stop();
            }
            SECTION("Increment Order Book Subscription") {
                std::string fixConfig = std::string(std::getenv("BINANCE_FIX_CONFIG"));
                std::string api_key = std::string(std::getenv("BINANCE_API_KEY"));
                std::string private_key_pem = std::string(std::getenv("BINANCE_PRIVATE_KEY_PATH"));
                std::vector<std::string> tradedSymbols = {"BTCUSDT"};
                pascal::net::FIXMarketDataEngine engine(fixConfig, private_key_pem, api_key, tradedSymbols);

                engine.start();
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                CHECK(engine.is_logged());
                pascal::common::MarketDataRequest req;
                req.Stream = pascal::common::MarketDataSubscriptionType::TOP_OF_BOOK;
                req.Symbol = "BTCUSDT";
                req.MarketDepth = 1;
                req.MDEntryType = pascal::common::OFFER;
                std::string symbol = "DEF";
                engine.register_parser_callback([](const pascal::common::MarketDataSnapshot& snapshot) {
                    //empty function
                });
                engine.register_parser_callback([&symbol](const pascal::common::MarketDataIncrement& increment) {
                    symbol = increment.symbol;
                });
                engine.sub_to_symbol(req);
                std::this_thread::sleep_for(std::chrono::milliseconds(150));
                CHECK(symbol == "BTCUSDT");
                engine.stop();
            }
        }
    }
}

