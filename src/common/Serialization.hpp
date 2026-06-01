#pragma once

#include "common/__cpo.hpp"
#include <fstream>
#include <sstream> // IWYU pragma: keep
#include <string>
#include <utility>

namespace Synera::serialization {

struct serialize_t {};
struct deserialize_t {};

namespace __fn {
struct serialize_fn {
    template <typename Stream, typename T>
    constexpr auto operator()(Stream &&stream, T &&val) const
        noexcept(noexcept(Synera::tag_invoke(serialize_t{},
                                             std::forward<Stream>(stream),
                                             std::forward<T>(val))))
            -> decltype(Synera::tag_invoke(serialize_t{},
                                           std::forward<Stream>(stream),
                                           std::forward<T>(val))) {
        return Synera::tag_invoke(serialize_t{}, std::forward<Stream>(stream),
                                  std::forward<T>(val));
    }
};

struct deserialize_fn {
    template <typename Stream, typename T>
    constexpr auto operator()(Stream &&stream, T &&val) const
        noexcept(noexcept(Synera::tag_invoke(deserialize_t{},
                                             std::forward<Stream>(stream),
                                             std::forward<T>(val))))
            -> decltype(Synera::tag_invoke(deserialize_t{},
                                           std::forward<Stream>(stream),
                                           std::forward<T>(val))) {
        return Synera::tag_invoke(deserialize_t{}, std::forward<Stream>(stream),
                                  std::forward<T>(val));
    }
};
} // namespace __fn

inline constexpr __fn::serialize_fn serialize{};
inline constexpr __fn::deserialize_fn deserialize{};

template <typename T, typename Stream>
concept Serializable = requires(Stream &stream, T &val) {
    { serialize(stream, val) };
    { deserialize(stream, val) };
};

} // namespace Synera::serialization

namespace Synera::engine {

struct save_fn {
    template <typename T>
    bool operator()(const T &val, const std::string &filepath) const {
        std::ofstream out(filepath);
        if (!out)
            return false;
        Synera::serialization::serialize(out, val);
        return true;
    }
};

struct load_fn {
    template <typename T>
    bool operator()(T &val, const std::string &filepath) const {
        std::ifstream in(filepath);
        if (!in)
            return false;
        Synera::serialization::deserialize(in, val);
        return true;
    }
};

inline constexpr save_fn save{};
inline constexpr load_fn load{};

} // namespace Synera::engine
