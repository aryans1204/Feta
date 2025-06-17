#pragma once
#include <memory>
#include <atomic>
#include <unordered_map>
#include <string>
#include <functional>
#include <thread>

#include "market_data/fix_parser.h"
#include "ed25519_signer.h"
#include "common/lockfree_spsc_queue.h"
#include "quickfix/Application.h"
#include "quickfix/Mutex.h"
#include "quickfix/Utility.h"
#include "quickfix/Values.h"
#include "quickfix/FileStore.h"
#include "quickfix/FileLog.h"
#include "quickfix/SocketInitiator.h"
#include "quickfix/Session.h"
#include "quickfix/SessionSettings.h"

#include "quickfix/fix44/MarketDataRequestReject.h"
#include "quickfix/fix44/MarketDataRequest.h"
#include "quickfix/fix44/MarketDataSnapshotFullRefresh.h"
#include "quickfix/fix44/MarketDataIncrementalRefresh.h"


namespace pascal {
    namespace fix {

        class FIXMarketDataEngine : public FIX::Application {
        public:

            struct QueuedFIXMessage {
                FIX::Message message;
                std::chrono::high_resolution_clock::time_point recv_time;

                QueuedFIXMessage() = default;
                QueuedFIXMessage(const FIX::Message& message, const std::chrono::high_resolution_clock::time_point& recv_time) : message(message), recv_time(recv_time) {}
            };

            std::unique_ptr<pascal::crypto::Ed25519Signer> signer_; //Key signer for Logon
            std::string api_key;
            
            typedef enum MarketDataSubscriptionType {
                RAW_TRADE,
                TOP_OF_BOOK,
                FULL_BOOK
            };

            struct MarketDataRequest {
                MarketDataSubscriptionType Stream;
                std::string Symbol;
                int MarketDepth;
                char MDEntryType;
                char Subscribe;
                std::string ReqID;
            };

            FIXMarketDataEngine(const std::string& fixConfig, const std::string& private_key_pem, const std::string& api_key, const std::vector<std::string>& tradedSymbols) : api_key(api_key), signer_(std::make_unique<pascal::crypto::Ed25519Signer>(private_key_pem)), tradedSymbols(tradedSymbols) 
            {
                settings_ = std::make_unique<FIX::SessionSettings>(fixConfig);
                store_factory_ = std::make_unique<FIX::FileStoreFactory>(settings_);
                log_factory_ = std::make_unique<FIX::FileLogFactory>(settings_);
                initiator_ = std::make_unique<FIX::SocketInitiator>(*this, *store_factory_, *settings_, *log_factory_);
                parser = std::make_unique<pascal::market_data::FIXMarketDataParser>(); 
            };
            ~FIXMarketDataEngine();

            //Application overloads
            void onCreate(const FIX::SessionID& ) override;
            void onLogon(const FIX::SessionID& ) override;
            void onLogout(const FIX::SessionID& )override;
            void toAdmin(FIX::Message&, const FIX::SessionID& ) override;
            void toApp(FIX::Message&, const FIX::SessionID& ) override;
            void fromAdmin(const FIX::Message&, const FIX::SessionID& ) override;
            void fromApp(const FIX::Message&, const FIX::SessionID&) override;

            
            //Engine logic
            void sub_to_symbol(MarketDataRequest& req);
            void unsub_to_symbol(const std::string& symbol);
            
            template<class T>
            void register_parser_callback(T clbk) {
                parser->register_callback(clbk);
            }

            //Application lifecycle
            bool start();
            bool stop();
            bool is_logged() const;

        
        private:
            //Market data subscription types
            void sign_logon_message(FIX::Message& message);
            std::string create_logon_payload(const FIX::Message& message);

            //Initiators and Settings
            std::unique_ptr<FIX::SocketInitiator> initiator_;
            std::unique_ptr<FIX::SessionSettings> settings_;
            std::unique_ptr<FIX::FileStoreFactory> store_factory_;
            std::unique_ptr<FIX::FileLogFactory> log_factory_;

            //Thread level data queue
            std::unordered_map<std::string, pascal::common::SPSCQueue<QueuedFIXMessage, 8192>> symbolQueues;
            std::unordered_map<std::string, std::unique_ptr<std::thread>> symbolsThreads;
            std::vector<std::string> tradedSymbols;
            FIX::SessionID sessionID;
            std::atomic<bool> is_logged_on{false};
            std::atomic<bool> is_running{false};

            std::unordered_map<std::string, std::string> active_subscriptions;  //{Symbol Name: MDReqID}
            FIX::Mutex subscription_mtx;

            std::unique_ptr<pascal::market_data::FIXMarketDataParser> parser;
            
            std::atomic<int> next_req_id{1};

            std::string send_market_data_request(const MarketDataRequest& req);
            std::string generate_request_id(); //generate MDReqID
            void process_market_data(const std::string& symbol);
        };
    };
};