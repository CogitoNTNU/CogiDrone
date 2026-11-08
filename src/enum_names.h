#pragma once

#include <string_view>
#include <type_traits>
#include <concepts>
#include <cstddef>
#include <array>
#include <span>


namespace cogidrone {

// * Enum -> name
// ? A concept constrains the SHAPE of a mapping; it cannot enumerate
// ? enumerators. C++ has no reflection before C++26, so the names must be
// ? written down once per enum. The concept is what lets ONE toString() serve
// ? every enum: it guarantees the author produced a constexpr array of
// ? string_views indexed by the enumerator's value.
template <typename E>
concept NamedEnum = std::is_enum_v<E> && requires(E value) {
    { enumNames(value) } -> std::convertible_to<std::span<const std::string_view>>;
};

// ? `enumNames` must be constexpr: the table is materialized at compile time
// ? and only the selected string_view reaches the binary.
template <NamedEnum E>
[[nodiscard]] constexpr std::string_view enumName(E value) noexcept {
    constexpr auto names = enumNames(E{});
    const auto INDEX = static_cast<std::size_t>(value);
    return INDEX < names.size() ? names[INDEX] : std::string_view{"UNKNOWN"};
}

template <NamedEnum E>
[[nodiscard]] constexpr std::string_view toString(E value) noexcept {
    return enumName(value);
}

} // namespace cogidrone