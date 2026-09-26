#pragma once

#include <rclcpp/rclcpp.hpp>
#include <geometry_msgs/msg/twist_stamped.hpp>
#include <geometry_msgs/msg/pose_stamped.hpp>
#include <nav_msgs/msg/odometry.hpp>
#include <std_msgs/msg/string.hpp>
#include <tf2_ros/transform_listener.h>
#include <tf2_ros/buffer.h>
#include <Eigen/Dense>
#include <memory>
#include <chrono>

namespace cogidrone_navigation {

/**
 * @brief Mission states for the autonomous drone
 */
enum class MissionState {
    IDLE,              // Waiting for commands
    SEARCHING,         // Looking for person
    PERSON_DETECTED,   // Person found, preparing to follow
    FOLLOWING,         // Actively following person
    PERSON_LOST,       // Lost sight of person, hovering/searching
    OBSTACLE_AVOIDANCE,// Emergency obstacle avoidance mode
    RETURNING,         // Returning to starting position
    LANDING,           // Landing sequence
    EMERGENCY          // Emergency/failsafe mode
};

/**
 * @brief Represents a person detection from the perception system
 */
struct PersonDetection {
    Eigen::Vector3f position;  // Position in camera frame
    float confidence;          // Detection confidence [0-1]
    rclcpp::Time timestamp;
    bool valid;
    
    PersonDetection() : position(0, 0, 0), confidence(0.0f), valid(false) {}
};

/**
 * @brief Drone state information
 */
struct DroneState {
    Eigen::Vector3f position;      // Position in world frame
    Eigen::Vector3f velocity;      // Velocity in world frame
    Eigen::Quaternionf orientation; // Orientation quaternion
    float yaw;                     // Yaw angle in radians
    rclcpp::Time timestamp;
    
    DroneState() : position(0, 0, 0), velocity(0, 0, 0), 
                   orientation(1, 0, 0, 0), yaw(0.0f) {}
};

/**
 * @brief Navigation parameters
 */
struct NavigationParams {
    // Following parameters
    float target_follow_distance = 4.0f;  // meters
    float target_height = 2.5f;           // meters above ground
    float max_follow_speed = 3.0f;        // m/s
    float min_follow_distance = 2.0f;     // safety minimum
    float max_follow_distance = 10.0f;    // lose tracking beyond this
    
    // Safety limits
    float min_altitude = 0.5f;            // meters
    float max_altitude = 10.0f;           // meters
    float safety_distance = 1.0f;         // meters from obstacles
    
    // Detection parameters
    float detection_timeout = 2.0f;       // seconds
    float min_detection_confidence = 0.7f;
    
    // Control gains
    float position_p_gain = 1.0f;
    float yaw_p_gain = 2.0f;
    
    // Update rates
    float control_frequency = 30.0f;      // Hz
};

/**
 * @brief Main navigation node for CogiDrone
 * 
 * This node handles:
 * - Mission state management
 * - Person tracking and following
 * - Path planning and trajectory generation
 * - Safety monitoring
 */
class NavigationNode : public rclcpp::Node {
public:
    /**
     * @brief Constructor
     */
    NavigationNode();
    
    /**
     * @brief Destructor
     */
    ~NavigationNode();

private:
    // ========== Callback Methods ==========
    
    /**
     * @brief Main control loop callback
     */
    void control_loop_callback();
    
    /**
     * @brief Handle person detection messages
     */
    void person_detection_callback(const geometry_msgs::msg::PoseStamped::SharedPtr msg);
    
    /**
     * @brief Handle drone odometry updates
     */
    void odometry_callback(const nav_msgs::msg::Odometry::SharedPtr msg);
    
    /**
     * @brief Handle manual override commands
     */
    void manual_override_callback(const std_msgs::msg::String::SharedPtr msg);
    
    // ========== State Machine Methods ==========
    
    /**
     * @brief Update the mission state machine
     */
    void update_state_machine();
    
    /**
     * @brief Get string representation of current state
     */
    std::string state_to_string(MissionState state) const;
    
    /**
     * @brief Transition to a new state
     */
    void transition_to_state(MissionState new_state);
    
    // ========== Navigation Logic Methods ==========
    
    /**
     * @brief Execute follow behavior
     * Computes velocity commands to follow detected person
     */
    void execute_follow_behavior();
    
    /**
     * @brief Execute search behavior
     * Look for lost person
     */
    void execute_search_behavior();
    
    /**
     * @brief Execute hover behavior
     * Maintain current position
     */
    void execute_hover_behavior();
    
    /**
     * @brief Compute target position for following
     * @return Target position in world frame
     */
    Eigen::Vector3f compute_follow_target_position();
    
    /**
     * @brief Generate velocity command towards target
     * @param target_position Target position in world frame
     * @return Velocity command
     */
    Eigen::Vector3f compute_velocity_command(const Eigen::Vector3f& target_position);
    
    /**
     * @brief Compute desired yaw to face target
     * @param target_position Target position in world frame
     * @return Desired yaw angle in radians
     */
    float compute_desired_yaw(const Eigen::Vector3f& target_position);
    
    /**
     * @brief Transform person position from camera frame to world frame
     * @param person_camera Position in camera frame
     * @return Position in world frame
     */
    Eigen::Vector3f transform_to_world_frame(const Eigen::Vector3f& person_camera);
    
    // ========== Safety Methods ==========
    
    /**
     * @brief Check if current command is safe
     * @param velocity_cmd Velocity command to check
     * @return true if safe, false otherwise
     */
    bool check_safety(const Eigen::Vector3f& velocity_cmd);
    
    /**
     * @brief Check if person detection is valid
     * @return true if valid, false otherwise
     */
    bool is_person_detection_valid() const;
    
    /**
     * @brief Check if person has been lost for too long
     * @return true if lost, false otherwise
     */
    bool is_person_lost() const;
    
    // ========== Publishing Methods ==========
    
    /**
     * @brief Publish velocity setpoint
     */
    void publish_velocity_setpoint(const Eigen::Vector3f& velocity, float yaw_rate);
    
    /**
     * @brief Publish mission status
     */
    void publish_mission_status();
    
    // ========== Member Variables ==========
    
    // ROS2 communication
    rclcpp::TimerBase::SharedPtr control_timer_;
    rclcpp::Subscription<geometry_msgs::msg::PoseStamped>::SharedPtr person_detection_sub_;
    rclcpp::Subscription<nav_msgs::msg::Odometry>::SharedPtr odometry_sub_;
    rclcpp::Subscription<std_msgs::msg::String>::SharedPtr manual_override_sub_;
    
    rclcpp::Publisher<geometry_msgs::msg::TwistStamped>::SharedPtr velocity_pub_;
    rclcpp::Publisher<std_msgs::msg::String>::SharedPtr mission_status_pub_;
    
    // TF2 for coordinate transformations
    std::shared_ptr<tf2_ros::Buffer> tf_buffer_;
    std::shared_ptr<tf2_ros::TransformListener> tf_listener_;
    
    // State variables
    MissionState current_state_;
    NavigationParams params_;
    PersonDetection last_person_detection_;
    DroneState drone_state_;
    
    // Tracking state
    Eigen::Vector3f person_world_position_;  // Estimated person position in world frame
    Eigen::Vector3f person_world_velocity_;  // Estimated person velocity
    rclcpp::Time last_detection_time_;
    rclcpp::Time state_entry_time_;
    
    // Home position for return behavior
    Eigen::Vector3f home_position_;
    bool home_position_set_;
};

} // namespace cogidrone_navigation
