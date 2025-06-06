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


FIXMarketDataEngine::onLogon(FIX::SessionID& sessionID) {
    
}
