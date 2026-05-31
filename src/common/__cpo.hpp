#pragma once

#include <utility>

namespace Synera {

namespace __detail {
void tag_invoke() = delete;
struct tag_invoke_t {
    template <typename Tag, typename... Args>
    constexpr auto operator()(Tag, Args &&...args) const
        noexcept(noexcept(tag_invoke(std::declval<Tag>(),
                                     std::forward<Args>(args)...)))
            -> decltype(auto) {
        return tag_invoke(Tag{}, std::forward<Args>(args)...);
    }
};
} // namespace __detail

inline constexpr __detail::tag_invoke_t tag_invoke{};

} // namespace Synera
