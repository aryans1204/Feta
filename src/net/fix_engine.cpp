
#include "net/fix_engine.h"
#include <chrono>
#include <thread>
#include "net/ed25519_signer.h"
#include "common/types.h"
#include <stdexcept>

namespace pascal {
    namespace net {
        bool FIXMarketDataEngine::start() {
            try {
                initiator_->start();
                is_running.store(true, std::memory_order_release);
                start_symbol_processing();
                return true;
            }
            catch (std::exception& e) {
                std::cerr << "Failed to start QuickFIX initiator" << e.what() << std::endl;
                return false;
            }
        }
        bool FIXMarketDataEngine::stop() {
            try {
                stop_symbol_processing();
                initiator_->stop();
                return true;
            }
            catch (std::exception& e) {
                std::cerr << "Failed to stop QuickFIX initiator" << e.what() << std::endl;
                return false;
            }
        }
        bool FIXMarketDataEngine::is_logged() const {
            return is_logged_on.load(std::memory_order_acquire);
        }
        void FIXMarketDataEngine::onLogon(const FIX::SessionID& sessionID) {
            this->sessionID = sessionID;
            is_logged_on.store(true, std::memory_order_release);
        }
        void FIXMarketDataEngine::onLogout(const FIX::SessionID& sessionID) {
            is_logged_on.store(false, std::memory_order_release);
        }
        void FIXMarketDataEngine::toAdmin(FIX::Message& message, const FIX::SessionID& sessionID) {
            FIX::MsgType msgType;
            message.getHeader().getField(msgType);
            std::cout << "Sending to admin: " << msgType.getValue() << std::endl;
            if (msgType == FIX::MsgType_Logon) {
                sign_logon_message(message);
                message.setField(FIX::IntField(25035, 1));
                message.setField(FIX::IntField(25036, 1));
                message.setField(FIX::IntField(25000, 5000));
                std::cout << message.toString() << std::endl;
            }
        }
        void FIXMarketDataEngine::fromAdmin(const FIX::Message& message, const FIX::SessionID& sessionID) {
            FIX::MsgType msgType;
            message.getHeader().getField(msgType);
            std::cout << "HI" << std::endl;
            if (msgType == FIX::MsgType_Reject) {
                FIX::Text text;
                message.getField(text);
                std::cerr << "Text is: " << text.getString() << std::endl;  
            }
        }
        void FIXMarketDataEngine::sign_logon_message(FIX::Message& message) {
            std::string payload = create_logon_payload(message);
            std::cout << "Payload is: " << payload << std::endl;
            std::string signature = signer_->sign_payload(payload);

            message.setField(FIX::Username(api_key));
            message.setField(FIX::RawDataLength(signature.length()));
            message.setField(FIX::RawData(signature));
        }
        std::string FIXMarketDataEngine::create_logon_payload(const FIX::Message& message) {
            FIX::MsgType msgType;
            FIX::SenderCompID senderCompId;
            FIX::TargetCompID targetCompId;
            FIX::MsgSeqNum msgSeqNum;
            FIX::SendingTime sendingTime;
            message.getHeader().getField(msgType);
            message.getHeader().getField(senderCompId);
            message.getHeader().getField(targetCompId);
            message.getHeader().getField(msgSeqNum);
            std::cout << "MsgSeqNum: " << msgSeqNum.getValue() << std::endl;
            message.getHeader().getField(sendingTime);
            const char SOH = '\x01';
            return msgType.getString()+SOH+senderCompId.getString()+SOH+targetCompId.getString()+SOH+std::to_string(msgSeqNum.getValue())+SOH+sendingTime.getString();
        }
        void FIXMarketDataEngine::fromApp(const FIX::Message& message, const FIX::SessionID& sessionID) {
            FIX::Symbol symbol; 
            if (!message.isSetField(symbol)) return;
            
            message.getField(symbol); 
            auto recv_time = std::chrono::high_resolution_clock::now();
            symbolQueues[symbol].push(message, recv_time);
        }
        std::string FIXMarketDataEngine::generate_request_id() {
            int req_id = next_req_id.fetch_add(1, std::memory_order_relaxed);
            return std::to_string(req_id);

        }
        std::string FIXMarketDataEngine::send_market_data_request(const pascal::common::MarketDataRequest& request) {
            FIX44::MarketDataRequest req;
            FIX::Header& header = req.getHeader();
            header.setField(FIX::BeginString("FIX.4.4"));
            header.setField(FIX::SenderCompID("PASCAL_MD"));
            header.setField(FIX::TargetCompID("SPOT"));
            header.setField(FIX::MsgType(FIX::MsgType_MarketDataRequest));
            req.setField(FIX::SubscriptionRequestType(request.Subscribe));
            req.setField(FIX::Symbol(request.Symbol));
            if (request.Subscribe == '2') {
                req.setField(FIX::MDReqID(request.ReqID));
                FIX::Session::sendToTarget(req, sessionID);
                return "";
            }
            std::string req_id = generate_request_id(); 
            req.setField(FIX::MDReqID(req_id));
            
            switch(request.Stream) {
                case pascal::common::MarketDataSubscriptionType::RAW_TRADE:
                    req.setField(FIX::MDEntryType(request.MDEntryType));
                    break;
                
                case pascal::common::MarketDataSubscriptionType::TOP_OF_BOOK:
                    req.setField(FIX::MDEntryType(request.MDEntryType));
                    req.setField(FIX::MarketDepth(1));
                    break;

                case pascal::common::MarketDataSubscriptionType::FULL_BOOK:
                    req.setField(FIX::MDEntryType(request.MDEntryType));
                    req.setField(FIX::MarketDepth(request.MarketDepth));
                    break;
                
                default:
                    break;
            }
            FIX::Session::sendToTarget(req, sessionID);
            return req_id;
        }
        void FIXMarketDataEngine::sub_to_symbol(pascal::common::MarketDataRequest& request) {
            FIX::Locker lock(subscription_mtx);
            request.Subscribe = '1';
            std::string req_id = send_market_data_request(request);
            active_subscriptions[request.Symbol] = req_id;
        }
        void FIXMarketDataEngine::unsub_to_symbol(const std::string& symbol) {
            FIX::Locker lock(subscription_mtx);
            auto it = active_subscriptions.find(symbol);
            if (it == active_subscriptions.end()) return;
            
            std::string req_id = it->second;
            pascal::common::MarketDataRequest request;
            request.ReqID = req_id;
            request.Subscribe = '2';
            request.Symbol = symbol;
            send_market_data_request(request);
            active_subscriptions.erase(it);
        }
        void FIXMarketDataEngine::process_market_data(const std::string& symbol) {
            QueuedFIXMessage message;
            MessageQueue& msgQueue = symbolQueues[symbol];
            while (is_running.load(std::memory_order_acquire)) {
                if (msgQueue.pop(message)) {
                    parser->parse_message(message.message, message.recv_time);
                }
                else {
                    std::this_thread::sleep_for(std::chrono::nanoseconds(100)); //sleep for 100ns
                }
            }
        }
        void FIXMarketDataEngine::start_symbol_processing() {
            int core_id = 1;
            for (const auto& symbol : tradedSymbols) {
                symbolsThreads[symbol] = std::thread([this, symbol]() {
                    process_market_data(symbol);
                });

                bind_thread_to_core(symbolsThreads[symbol], core_id++);
            }
        }
        void FIXMarketDataEngine::stop_symbol_processing() {
            is_running.store(false, std::memory_order_release);
            for (const auto& symbol : tradedSymbols) {
                symbolsThreads[symbol].join();
            }
            symbolQueues.clear();
            symbolsThreads.clear();
        }
        void FIXMarketDataEngine::bind_thread_to_core(std::thread& thread, int core_id) {
            #ifdef __linux__

            cpu_set_t cpuset;
            CPU_ZERO(&cpuset);
            CPU_SET(core_id, &cpuset);

            int rc = pthread_setaffinity_np(thread.native_handle(), sizeof(cpu_set_t), &cpuset);
            if (rc != 0) {
                std::cerr << "Failed to bind thread to core id " << core_id << std::endl;
            }
            #endif
        }
    }
}

