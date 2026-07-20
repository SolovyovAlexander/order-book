#include "ob/order_book.hpp"

#include <algorithm>

namespace ob {

// Defined before its callers so the implicit instantiation sees the body.
template <class SideBook>
void OrderBook::matchAgainst(AddOrderRequest& order, SideBook& side_book) {
    while (order.quantity > 0 && !side_book.empty()) {
        auto best = side_book.begin();            // best price level, O(1)
        if (!crosses(order, best->first)) break;  // no cross -> done matching

        const Price price = best->first;  // trade price = the level's price
        Level& level = best->second;
        while (order.quantity > 0 && !level.empty()) {
            Order& resting = level.front();  // oldest at this level (time priority)
            const Quantity qty = std::min(order.quantity, resting.quantity);

            // Protocol order per match: trade, aggressive fill, resting fill.
            reporter_.onTrade(TradeEvent{qty, price});
            order.quantity -= qty;
            resting.quantity -= qty;

            if (order.quantity == 0)
                reporter_.onFullyFilled(OrderFullyFilled{order.id});
            else
                reporter_.onPartiallyFilled(OrderPartiallyFilled{order.id, order.quantity});

            if (resting.quantity == 0) {
                reporter_.onFullyFilled(OrderFullyFilled{resting.id});
                index_.erase(resting.id);
                level.popFront();
            } else {
                reporter_.onPartiallyFilled(OrderPartiallyFilled{resting.id, resting.quantity});
            }
        }

        if (level.empty()) side_book.erase(best);  // drop emptied level, O(log P)
    }
}

void OrderBook::rest(const AddOrderRequest& order) {
    // try_emplace, not operator[], because Level isn't default-constructible: a
    // new level is built in place with a reference to the shared pool.
    const Order resting{order.id, order.quantity};
    Level* level = nullptr;
    Level::Handle handle;
    if (order.side == Side::Buy) {
        Level& lvl = bids_.try_emplace(order.price, pool_).first->second;
        level = &lvl;
        handle = lvl.add(resting);
    } else {
        Level& lvl = asks_.try_emplace(order.price, pool_).first->second;
        level = &lvl;
        handle = lvl.add(resting);
    }
    index_[order.id] = Location{level, handle, order.side, order.price};
}

bool OrderBook::add(const AddOrderRequest& req) {
    if (index_.count(req.id) != 0) return false;  // duplicate live id -> ignore

    AddOrderRequest order = req;  // mutable working copy; its quantity is the remainder

    if (order.side == Side::Buy)
        matchAgainst(order, asks_);
    else
        matchAgainst(order, bids_);

    if (order.quantity > 0) rest(order);
    return true;
}

bool OrderBook::cancel(const CancelOrderRequest& req) {
    auto it = index_.find(req.id);
    if (it == index_.end()) return false;  // unknown id -> silent no-op

    const Location& loc = it->second;
    loc.level->remove(loc.handle);  // O(1) via the back-pointer, no tree walk
    if (loc.level->empty()) {       // level emptied -> drop it from its map (rare)
        if (loc.side == Side::Buy)
            bids_.erase(loc.price);
        else
            asks_.erase(loc.price);
    }
    index_.erase(it);
    return true;
}

}  // namespace ob
