
#include "fix_engine.h"
#include <chrono>
#include <thread>
#include "ed25519_signer.h"

namespace pascal {
    namespace fix {
        bool FIXMarketDataEngine::start() {
            try {
                initiator_->start();
                is_running.store(true);
                for (auto symbol : tradedSymbols) {
                    symbolsThreads[symbol] = std::make_unique<std::thread>([this, symbol]() {
                        process_market_data(symbol);
                    });
                }
                return true;
            }
            catch (std::exception& e) {
                std::cerr << "Failed to start QUickFIX initiator" << e.what() << std::endl;
                return false;
            }
        }
        bool FIXMarketDataEngine::stop() {
            try {
                initiator_->stop();
                is_running.store(false);
                for (auto symbol : tradedSymbols) {
                    symbolsThreads[symbol]->join();
                }
                return true;
            }
            catch (std::exception& e) {
                std::cerr << "Failed to stop QuickFIX initiator" << e.what() << std::endl;
                return false;
            }
        }
        bool FIXMarketDataEngine::is_logged() const {
            return is_logged_on.load(std::memory_order_relaxed);
        }
        void FIXMarketDataEngine::onLogon(const FIX::SessionID& sessionID) {
            this->sessionID = sessionID;
            is_logged_on.store(true);
        }
        void FIXMarketDataEngine::toAdmin(FIX::Message& message, const FIX::SessionID& sessionID) {
            FIX::MsgType msgType;
            message.getHeader().getField(msgType);

            if (msgType == FIX::MsgType_Logon) {
                sign_logon_message(message);
            }
        }
        void FIXMarketDataEngine::sign_logon_message(FIX::Message& message) {
            std::string payload = create_logon_payload(message);
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
            message.getHeader().getField(sendingTime);
            const char SOH = '\x01';
            return msgType.getString()+SOH+senderCompId.getString()+SOH+targetCompId.getString()+SOH+msgSeqNum.getString()+SOH+sendingTime.getString();
        }
        void FIXMarketDataEngine::fromApp(const FIX::Message& message, const FIX::SessionID& sessionID) {
            FIX::Symbol symbol;
            message.getField(symbol); 
            auto recv_time = std::chrono::high_resolution_clock::now();
            symbolQueues[symbol].push(QueuedFIXMessage(message, recv_time));
        }
        std::string FIXMarketDataEngine::generate_request_id() {
            int req_id = next_req_id.fetch_add(1, std::memory_order_relaxed);
            return std::to_string(req_id);

        }
        std::string FIXMarketDataEngine::send_market_data_request(const MarketDataRequest& request) {
            FIX44::MarketDataRequest req;
            FIX::Header& header = req.getHeader();
            header.setField(FIX::BeginString("FIX.4.4"));
            header.setField(FIX::SenderCompID("ARYAN"));
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
                case MarketDataSubscriptionType::RAW_TRADE:
                    req.setField(FIX::MDEntryType('2'));
                    break;
                
                case MarketDataSubscriptionType::TOP_OF_BOOK:
                    req.setField(FIX::MDEntryType(request.MDEntryType));
                    req.setField(FIX::MarketDepth(1));
                    break;

                case MarketDataSubscriptionType::FULL_BOOK:
                    req.setField(FIX::MDEntryType(request.MDEntryType));
                    req.setField(FIX::MarketDepth(request.MarketDepth));
                    break;
                
                default:
                    break;
            }
            FIX::Session::sendToTarget(req, sessionID);
            return req_id;
        }
        void FIXMarketDataEngine::sub_to_symbol(MarketDataRequest& request) {
            subscription_mtx.lock();
            request.Subscribe = '1';
            std::string req_id = send_market_data_request(request);
            active_subscriptions[request.Symbol] = req_id;
            subscription_mtx.unlock();
        }
        void FIXMarketDataEngine::unsub_to_symbol(const std::string& symbol) {
            subscription_mtx.lock();
            std::string req_id = active_subscriptions[symbol];
            MarketDataRequest request;
            request.ReqID = req_id;
            request.Subscribe = '2';
            request.Symbol = symbol;
            send_market_data_request(request);
            active_subscriptions.erase(symbol);
            subscription_mtx.unlock();
        }
        void FIXMarketDataEngine::process_market_data(const std::string& symbol) {
            QueuedFIXMessage message;
            while (true) {
                if (symbolQueues[symbol].pop(message)) {
                    parser->parse_message(message.message, message.recv_time);
                }
            }
        }
    }
}

