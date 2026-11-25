#pragma once

#include <string>
#include <string_view>

// TODO: One caveat we should verify once on hardware: Current we're inferring that the bridge's 
//     : suffix equals MESSAGE_VERSION, which we need to confirm on the jetson with:
//     :    ```sh
//     :    ros2 topic list | grep fmu
//     :    # compare against: grep MESSAGE_VERSION vendored/px4_msgs/include/px4_msgs/px4_msgs/msg/detail/*_struct.hpp 
//     :    ```
//     : If they match, the template is correct. If they don't, the whole premise is wrong and we revert to literals.

// TODO: Avoid the allocation from std::string by instead using consteval + fixed-size std::array<char, N>. 

namespace cogidrone::px4 {

// * Versioned topic names
// ? PX4's uXRCE-DDS bridge appends `_v<MESSAGE_VERSION>` to every topic whose
// ? message declares a non-zero version. Hardcoding the suffix means a PX4
// ? upgrade silently breaks the subscription: the topic stops existing, the
// ? subscriber is created without error, and the node waits forever.
// ? Deriving the suffix from the message's own MESSAGE_VERSION makes the
// ? coupling explicit and self-correcting.
// ?
// ! ONE allocation per call. Call this ONCE in wire(), never per message.
// ! The returned string is copied by create_subscription, so the temporary
// ! is safe to pass directly.
template <typename Msg>
[[nodiscard]] std::string versionedTopic(std::string_view base) {
    if constexpr (Msg::MESSAGE_VERSION == 0) {
        return std::string(base);                                                       // ? version 0 messages carry no suffix
    } else {
        return std::string(base) + "_v" + std::to_string(Msg::MESSAGE_VERSION);
    }
}

} // namespace cogidrone::px4