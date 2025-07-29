#pragma once
#include <memory>
#include <atomic>
#include <unordered_map>
#include <string>
#include <functional>
#include <thread>
#include <stdexcept>

#include "net/fix_parser.h"
#include "net/ed25519_signer.h"
#include "common/lockfree_spsc_queue.h"
#include "common/types.h"
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
    namespace net {

        class FIXMarketDataEngine : public FIX::Application {
        public:

            struct QueuedFIXMessage {
                FIX::Message message;
                std::chrono::high_resolution_clock::time_point recv_time;
                
                QueuedFIXMessage() = default;
                QueuedFIXMessage(const FIX::Message& message, const std::chrono::high_resolution_clock::time_point& recv_time) : message(message), recv_time(recv_time) {}
                QueuedFIXMessage(QueuedFIXMessage&& msg) = default;
                QueuedFIXMessage& operator=(QueuedFIXMessage&& msg) = default;
            };
            
            using MessageQueue = pascal::common::SPSCQueue<QueuedFIXMessage, 16384>;

            std::unique_ptr<pascal::crypto::Ed25519Signer> signer_; //Key signer for Logon
            std::string api_key;
            

            FIXMarketDataEngine(const std::string& fixConfig, const std::string& private_key_pem, const std::string& api_key, const std::vector<std::string>& tradedSymbols) : api_key(api_key), tradedSymbols(tradedSymbols) 
            {
                settings_ = std::make_unique<FIX::SessionSettings>(fixConfig);
                store_factory_ = std::make_unique<FIX::FileStoreFactory>(*settings_);
                log_factory_ = std::make_unique<FIX::FileLogFactory>(*settings_);
                initiator_ = std::make_unique<FIX::SocketInitiator>(*this, *store_factory_, *settings_, *log_factory_);
                parser = std::make_unique<pascal::market_data::FIXMarketDataParser>();

                signer_ = std::make_unique<pascal::crypto::Ed25519Signer>();
                if (!signer_->loadPrivateKeyFromFile(private_key_pem)) {
                    std::runtime_error("Private Key cannot be loaded from file");
                } 
            };
            ~FIXMarketDataEngine() {
                if (is_running.load(std::memory_order_acquire)) {
                    stop();
                }
            }

            //Application overloads
            void onCreate(const FIX::SessionID& ) {}
            void onLogon(const FIX::SessionID& ) override;
            void onLogout(const FIX::SessionID& ) override;
            void toAdmin(FIX::Message&, const FIX::SessionID& ) override;
            void toApp(FIX::Message&, const FIX::SessionID& ) {}
            void fromAdmin(const FIX::Message&, const FIX::SessionID& ) override;
            void fromApp(const FIX::Message&, const FIX::SessionID&) override;

            
            //Engine logic
            void sub_to_symbol(pascal::common::MarketDataRequest& req);
            void unsub_to_symbol(const std::string& symbol);
            
            template<typename T>
            void register_parser_callback(T&& clbk) {
                parser->register_callback(std::forward<T>(clbk));
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
            std::unordered_map<std::string, MessageQueue> symbolQueues;
            std::unordered_map<std::string, std::thread> symbolsThreads;
            std::vector<std::string> tradedSymbols;
            FIX::SessionID sessionID;
            std::atomic<bool> is_logged_on{false};
            std::atomic<bool> is_running{false};

            std::unordered_map<std::string, std::string> active_subscriptions;  //{Symbol Name: MDReqID}
            FIX::Mutex subscription_mtx;

            std::unique_ptr<pascal::market_data::FIXMarketDataParser> parser;
            
            std::atomic<int> next_req_id{1};

            std::string send_market_data_request(const pascal::common::MarketDataRequest& req);
            std::string generate_request_id(); //generate MDReqID
            
            //Thread lifecycle management
            void start_symbol_processing();
            void stop_symbol_processing();
            void process_market_data(const std::string& symbol);
            void bind_thread_to_core(std::thread& thread, int core_id);
        };
    };
};