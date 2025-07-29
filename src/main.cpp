#include "common/types.h"
#include "market_data/fix_order_book.h"

int main() {
    pascal::market_data::FIXOrderBookManager orderBookManager;
    orderBookManager.add_symbol("BTCUSDT");
    return 0;    
}