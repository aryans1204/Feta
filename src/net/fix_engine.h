#pragma once
#include <memory>
#include <atomic>
#include <unordered_map>
#include <string>
#include <function>

#include "quickfix/Application.h"
#include "quickfix/MessageCracker.h"
#include "quickfix/Mutex.h"
#include "quickfix/Utility.h"
#include "quickfix/Values.h"

#include "quickfix/fix44/MarketDataRequestReject.h"
#include "quickfix/fix44/MarketDataRequest.h"
#include "quickfix/fix44/MarketDataSnapshotFullRefresh.h"
#include "quickfix/fix44/MarketDataIncrementalRefresh.h"


namespace pascal {
    namespace fix {

        class FIXMarketDataEngine : public FIX::Application, public FIX::MessageCracker {
        public:

            using MarketDataCallback = std::function<void(const std::string& symbol, FIX::message& message)>;

            FIXMarketDataEngine(const std::string& fixConfig);
            ~FIXMarketDataEngine();

            //Application overloads
            void onCreate(const FIX::SessionID& ) override;
            void onLogon(const FIX::SessionID& ) override;
            void onLogout(const FIX::SessionID& )override;
            void toAdmin(FIX::Message&, const FIX::SessionID& ) override;
            void toApp(FIX::Message&, const FIX::SessionID& ) EXCEPT(FIX::DoNotSend) override;
            void fromAdmin(FIX::Message&, const FIX::SessionID& ) EXCEPT(FIX::FieldNotFound, FIX::IncorrectDataFormat, FIX::IncorrectTagValue, FIX::RejectLogon) override;
            void fromApp(FIX::Message&, const FIX::SessionID&) EXCEPT(FIX::FieldNotFound, FIX::IncorrectDataFormat, FIX::IncorrectTagValue, FIX::UnsupportedMessageType) override;

            //Message overloads
            void onMessage(const FIX44::MarketDataRequestReject&, const FIX::SessionID& );
            void onMessage(const FIX44::MarketDataSnapshotFullRefresh&, const FIX::SessionID& );
            void onMessage(const FIX44::MarketDataIncrementalRefresh&, const FIX::SessionID& );
            
            //Engine logic
            void sub_to_symbol(const std::string& symbol, MarketDataSubscriptionType = FULL_BOOK);
            void unsub_to_symbol(const std::string& symbol);
            void set_market_data_callback(MarketDataCallack clbk);

            //Application lifecycle
            bool start();
            bool stop();
            bool is_logged() const;

        
        private:
            //Market data subscription types
            enum MarketDataSubscriptionType {
                RAW_TRADE,
                TOP_OF_BOOK,
                FULL_BOOK
            };
            //Initiators and Settings
            std::unique_ptr<FIX::SocketInitiator> initiator_;
            std::unique_ptr<FIX::SessionSettings> settings_;
            std::unique_ptr<FIX::FileStoreFactory> store_factory_;
            std::unique_ptr<FIX::FileLogFactory> log_factory_;

            FIX::SessionID sessionID;
            std::atomic<bool> is_logged_on{false};
            std::atomic<bool> is_running{false};

            std::unordered_map<std::string, std::string> active_subscriptions;  //{Symbol Name: MDReqID}
            FIX::Mutex subscription_mtx;

            MarketDataCallback callback; //callback to process market data as it arrives in fromApp
            
            std::atomic<int> next_req_id{1};

            void send_market_data_request(const std::string& symbol, MarketDataSubscriptionType stream);
            std::string generate_request_id(); //generate MDReqID
        };
    };
};