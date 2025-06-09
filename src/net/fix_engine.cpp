
#include "fix_engine.h"
#include "ed25519_signer.h"

namespace pascal {
    namespace fix {
        bool FIXMarketDataEngine::start() {
            try {
                initiator_->start();
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
                return true;
            }
            catch (std::exception& e) {
                std::cerr << "Failed to stop QuickFIX initiator" << e.what() << std::endl;
                return false;
            }
        }
        void FIXMarketDataEngine::onLogon(const FIX::SessionID& sessionID) {
            this->sessionID = sessionID;
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
            crack(message, sessionID);
        }
        void FIXMarketDataEngine::onMessage(const FIX44::MarketDataSnapshotFullRefresh& message, const FIX::SessionID& sessionID) {
            FIX::Symbol symbol;
            message.get(symbol);
            FIX::Message msg = message;
            callback(symbol.getString(), msg);
        }
        void FIXMarketDataEngine::onMessage(const FIX44::MarketDataIncrementalRefresh& message, const FIX::SessionID& sessionID) {
            FIX::Symbol symbol;
            message.getField(symbol);
            FIX::Message msg = message;
            callback(symbol.getString(), msg);
        }
    }
}

