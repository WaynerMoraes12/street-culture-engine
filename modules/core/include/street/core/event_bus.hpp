#pragma once
// Type-safe synchronous publish/subscribe hub.
//
// The culture layer uses it to let otherwise independent engines react to each
// other: hoops publishes a 50-point game, kicks turns it into a hype shock,
// beats turns it into an anthem — without any of them knowing about the others.

#include <cstddef>
#include <functional>
#include <typeindex>
#include <unordered_map>
#include <utility>
#include <vector>

namespace street {

class EventBus {
public:
    using SubscriptionId = std::size_t;

    template <class Event>
    SubscriptionId subscribe(std::function<void(const Event&)> handler) {
        const SubscriptionId id = ++last_id_;
        handlers_[std::type_index(typeid(Event))].push_back(
            {id, [h = std::move(handler)](const void* e) { h(*static_cast<const Event*>(e)); }});
        return id;
    }

    /// Delivers the event to every subscriber of its exact type, in subscription order.
    template <class Event>
    std::size_t publish(const Event& event) const {
        const auto it = handlers_.find(std::type_index(typeid(Event)));
        if (it == handlers_.end()) return 0;
        const auto snapshot = it->second; // handlers may subscribe/unsubscribe while running
        for (const auto& entry : snapshot) entry.second(&event);
        return snapshot.size();
    }

    bool unsubscribe(SubscriptionId id) {
        for (auto& [type, list] : handlers_) {
            for (auto it = list.begin(); it != list.end(); ++it) {
                if (it->first == id) {
                    list.erase(it);
                    return true;
                }
            }
        }
        return false;
    }

    template <class Event>
    std::size_t subscriber_count() const {
        const auto it = handlers_.find(std::type_index(typeid(Event)));
        return it == handlers_.end() ? 0 : it->second.size();
    }

private:
    using Erased = std::function<void(const void*)>;
    std::unordered_map<std::type_index, std::vector<std::pair<SubscriptionId, Erased>>> handlers_;
    SubscriptionId last_id_ = 0;
};

} // namespace street
