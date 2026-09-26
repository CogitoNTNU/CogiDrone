#pragma once

#include "link/vehicle_link.h"

#include <rclcpp/rclcpp.hpp>
#include <px4_msgs/msg/offboard_control_mode.hpp>
#include <px4_msgs/msg/trajectory_setpoint.hpp>
#include <px4_msgs/msg/vehicle_command.hpp>
#include <px4_msgs/msg/vehicle_odometry.hpp>
#include <px4_msgs/msg/vehicle_status.hpp>

#include <expected>
#include <memory>
#include <mutex>


// The real link: PX4 over uXRCE-DDS.
//
// ? This is the ONLY file in the project allowed to mention px4_msgs. Everything
// ? upstream of it speaks the plain structs in vehicle_link.h, so the px4_msgs
// ? version pin (see third-party/ros2/PX4_MSGS.md) is contained to this one
// ? translation layer instead of being spread through the flight logic.
//
// ! Requires a running MicroXRCEAgent on the companion computer AND a PX4 client
// ! configured to talk to it. With no agent, every topic here simply stays silent
// ! - there is no error and no warning. See third-party/uxrce-agent/AGENT.md.
class Px4DdsLink final : public IVehicleLink {
public:
    // ? A local error type rather than Drone::InitError: link/ must not depend on
    // ? drone.h, or the adapter and the backbone become circularly coupled.
    // ? Drone::initiate maps this onto InitError::LinkFailed.
    enum class LinkError {
        NodeCreationFailed,
        RosNotInitialized,
    };

    [[nodiscard]] static std::expected<std::unique_ptr<Px4DdsLink>, LinkError> create();

    ~Px4DdsLink() override = default;

    // * IVehicleLink
    [[nodiscard]] std::optional<Odometry> odometry() const override;
    [[nodiscard]] FlightState state() const override;
    void send(const Setpoint& setpoint) override;
    void arm() override;
    void disarm() override;
    void tick() override;

    // The node this link owns, so Drone::start() can spin it. Non-owning use only.
    [[nodiscard]] rclcpp::Node::SharedPtr node() const { return node_; }

private:
    explicit Px4DdsLink(rclcpp::Node::SharedPtr node);                                   // ctor private: use create()

    void wire_up();                                                                      // create the pubs/subs
    [[nodiscard]] uint64_t now_us() const;                                               // PX4-style microsecond stamp
    void send_vehicle_command(uint32_t command, float param1);

    rclcpp::Node::SharedPtr node_;

    rclcpp::Subscription<px4_msgs::msg::VehicleOdometry>::SharedPtr odometry_sub_;
    rclcpp::Subscription<px4_msgs::msg::VehicleStatus>::SharedPtr status_sub_;

    rclcpp::Publisher<px4_msgs::msg::OffboardControlMode>::SharedPtr offboard_pub_;
    rclcpp::Publisher<px4_msgs::msg::TrajectorySetpoint>::SharedPtr setpoint_pub_;
    rclcpp::Publisher<px4_msgs::msg::VehicleCommand>::SharedPtr command_pub_;

    // ! Guards the cached telemetry below. Subscription callbacks run on the
    // ! executor thread (whoever calls rclcpp::spin), while odometry()/state() are
    // ! called by the flight logic. Even under a single-threaded executor those
    // ! can be different threads, so the cache is not safe to touch unlocked.
    mutable std::mutex telemetry_mutex_;
    std::optional<Odometry> latest_odometry_{};
    std::optional<px4_msgs::msg::VehicleStatus> latest_status_{};
};
