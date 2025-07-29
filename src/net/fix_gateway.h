#include "net/fix_engine.h"

namespace pascal {
    namespace gateway {
        class FIXMarketDataGateway {
        public:

            FIXMarketDataGateway();

            //Lifecycle management
            bool start();
            bool stop();

            

        }
    }
}