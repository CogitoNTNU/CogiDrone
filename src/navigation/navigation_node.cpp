#include "navigation_node.hpp"
#include <tf2_geometry_msgs/tf2_geometry_msgs.hpp>
#include <cmath>

namespace cogidrone_navigation {

NavigationNode::NavigationNode() 
    : Node("navigation_node"),
      current_state_(MissionState::IDLE),
      home_position_set_(false)
{
    RCLCPP_INFO(this->get_logger(), "Initializing Navigation Node...");
    
    // Load parameters from ROS2 parameter server
    this->declare_parameter("target_follow_distance", params_.target_follow_distance);
    this->declare_parameter("target_height", params_.target_height);
    this->declare_parameter("max_follow_speed", params_.max_follow_speed);
    this->declare_parameter("control_frequency", params_.control_frequency);
    this->declare_parameter("detection_timeout", params_.detection_timeout);
    this->declare_parameter("min_detection_confidence", params_.min_detection_confidence);
    
    // Get parameters
    this->get_parameter("target_follow_distance", params_.target_follow_distance);
    this->get_parameter("target_height", params_.target_height);
    this->get_parameter("max_follow_speed", params_.max_follow_speed);
    this->get_parameter("control_frequency", params_.control_frequency);
    this->get_parameter("detection_timeout", params_.detection_timeout);
    this->get_parameter("min_detection_confidence", params_.min_detection_confidence);
    
    // Initialize TF2
    tf_buffer_ = std::make_shared<tf2_ros::Buffer>(this->get_clock());
    tf_listener_ = std::make_shared<tf2_ros::TransformListener>(*tf_buffer_);
    
    // Create subscribers
    person_detection_sub_ = this->create_subscription<geometry_msgs::msg::PoseStamped>(
        "/perception/person_detection", 10,
        std::bind(&NavigationNode::person_detection_callback, this, std::placeholders::_1)
    );
    
    odometry_sub_ = this->create_subscription<nav_msgs::msg::Odometry>(
        "/estimation/vio_odometry", 10,
        std::bind(&NavigationNode::odometry_callback, this, std::placeholders::_1)
    );
    
    manual_override_sub_ = this->create_subscription<std_msgs::msg::String>(
        "/manual_override", 10,
        std::bind(&NavigationNode::manual_override_callback, this, std::placeholders::_1)
    );
    
    // Create publishers
    velocity_pub_ = this->create_publisher<geometry_msgs::msg::TwistStamped>(
        "/navigation/velocity_setpoint", 10
    );
    
    mission_status_pub_ = this->create_publisher<std_msgs::msg::String>(
        "/navigation/mission_status", 10
    );
    
    // Create control timer
    auto control_period = std::chrono::duration<double>(1.0 / params_.control_frequency);
    control_timer_ = this->create_wall_timer(
        std::chrono::duration_cast<std::chrono::milliseconds>(control_period),
        std::bind(&NavigationNode::control_loop_callback, this)
    );
    
    // Initialize state
    person_world_position_ = Eigen::Vector3f::Zero();
    person_world_velocity_ = Eigen::Vector3f::Zero();
    last_detection_time_ = this->now();
    state_entry_time_ = this->now();
    
    RCLCPP_INFO(this->get_logger(), "Navigation Node initialized successfully");
}

NavigationNode::~NavigationNode() {
    RCLCPP_INFO(this->get_logger(), "Shutting down Navigation Node");
}

// ========== Callback Methods ==========

void NavigationNode::control_loop_callback() {
    // Main control loop - runs at control_frequency Hz
    
    // Update state machine
    update_state_machine();
    
    // Execute behavior based on current state
    switch (current_state_) {
        case MissionState::IDLE:
            // Do nothing, waiting for mission start
            break;
            
        case MissionState::SEARCHING:
            execute_search_behavior();
            break;
            
        case MissionState::PERSON_DETECTED:
            // Transition to following will happen in state machine
            execute_hover_behavior();
            break;
            
        case MissionState::FOLLOWING:
            execute_follow_behavior();
            break;
            
        case MissionState::PERSON_LOST:
            execute_search_behavior();
            break;
            
        case MissionState::OBSTACLE_AVOIDANCE:
            // TODO: Implement obstacle avoidance
            execute_hover_behavior();
            break;
            
        case MissionState::RETURNING:
            // TODO: Implement return to home
            execute_hover_behavior();
            break;
            
        case MissionState::LANDING:
            // TODO: Implement landing
            break;
            
        case MissionState::EMERGENCY:
            // Emergency stop
            publish_velocity_setpoint(Eigen::Vector3f::Zero(), 0.0f);
            break;
    }
    
    // Publish mission status
    publish_mission_status();
}

void NavigationNode::person_detection_callback(
    const geometry_msgs::msg::PoseStamped::SharedPtr msg) 
{
    // Update person detection
    last_person_detection_.position = Eigen::Vector3f(
        msg->pose.position.x,
        msg->pose.position.y,
        msg->pose.position.z
    );
    
    // Assume confidence is encoded in orientation.w for now
    // TODO: Use proper detection message with confidence field
    last_person_detection_.confidence = std::abs(msg->pose.orientation.w);
    last_person_detection_.timestamp = this->now();
    last_person_detection_.valid = true;
    
    // Transform to world frame
    try {
        person_world_position_ = transform_to_world_frame(last_person_detection_.position);
        last_detection_time_ = this->now();
        
        RCLCPP_DEBUG(this->get_logger(), 
            "Person detected at (%.2f, %.2f, %.2f) with confidence %.2f",
            person_world_position_.x(), person_world_position_.y(), 
            person_world_position_.z(), last_person_detection_.confidence
        );
    } catch (const tf2::TransformException& ex) {
        RCLCPP_WARN(this->get_logger(), "TF transform failed: %s", ex.what());
    }
}

void NavigationNode::odometry_callback(const nav_msgs::msg::Odometry::SharedPtr msg) {
    // Update drone state
    drone_state_.position = Eigen::Vector3f(
        msg->pose.pose.position.x,
        msg->pose.pose.position.y,
        msg->pose.pose.position.z
    );
    
    drone_state_.velocity = Eigen::Vector3f(
        msg->twist.twist.linear.x,
        msg->twist.twist.linear.y,
        msg->twist.twist.linear.z
    );
    
    drone_state_.orientation = Eigen::Quaternionf(
        msg->pose.pose.orientation.w,
        msg->pose.pose.orientation.x,
        msg->pose.pose.orientation.y,
        msg->pose.pose.orientation.z
    );
    
    // Extract yaw from quaternion
    tf2::Quaternion q(
        msg->pose.pose.orientation.x,
        msg->pose.pose.orientation.y,
        msg->pose.pose.orientation.z,
        msg->pose.pose.orientation.w
    );
    tf2::Matrix3x3 m(q);
    double roll, pitch, yaw;
    m.getRPY(roll, pitch, yaw);
    drone_state_.yaw = static_cast<float>(yaw);
    
    drone_state_.timestamp = this->now();
    
    // Set home position on first odometry message
    if (!home_position_set_) {
        home_position_ = drone_state_.position;
        home_position_set_ = true;
        RCLCPP_INFO(this->get_logger(), 
            "Home position set to (%.2f, %.2f, %.2f)",
            home_position_.x(), home_position_.y(), home_position_.z()
        );
    }
}

void NavigationNode::manual_override_callback(const std_msgs::msg::String::SharedPtr msg) {
    RCLCPP_WARN(this->get_logger(), "Manual override received: %s", msg->data.c_str());
    
    if (msg->data == "EMERGENCY_STOP") {
        transition_to_state(MissionState::EMERGENCY);
    } else if (msg->data == "START_MISSION") {
        transition_to_state(MissionState::SEARCHING);
    } else if (msg->data == "RETURN_HOME") {
        transition_to_state(MissionState::RETURNING);
    } else if (msg->data == "LAND") {
        transition_to_state(MissionState::LANDING);
    }
}

// ========== State Machine Methods ==========

void NavigationNode::update_state_machine() {
    MissionState next_state = current_state_;
    
    switch (current_state_) {
        case MissionState::IDLE:
            // Wait for start command (handled in manual_override_callback)
            break;
            
        case MissionState::SEARCHING:
            if (is_person_detection_valid()) {
                next_state = MissionState::PERSON_DETECTED;
            }
            break;
            
        case MissionState::PERSON_DETECTED:
            // Wait for stable tracking before following
            if ((this->now() - state_entry_time_).seconds() > 1.0) {
                if (is_person_detection_valid()) {
                    next_state = MissionState::FOLLOWING;
                } else {
                    next_state = MissionState::SEARCHING;
                }
            }
            break;
            
        case MissionState::FOLLOWING:
            if (is_person_lost()) {
                next_state = MissionState::PERSON_LOST;
            }
            // TODO: Check for obstacles
            break;
            
        case MissionState::PERSON_LOST:
            if (is_person_detection_valid()) {
                next_state = MissionState::PERSON_DETECTED;
            }
            // TODO: Add timeout to return home
            break;
            
        default:
            break;
    }
    
    if (next_state != current_state_) {
        transition_to_state(next_state);
    }
}

std::string NavigationNode::state_to_string(MissionState state) const {
    switch (state) {
        case MissionState::IDLE: return "IDLE";
        case MissionState::SEARCHING: return "SEARCHING";
        case MissionState::PERSON_DETECTED: return "PERSON_DETECTED";
        case MissionState::FOLLOWING: return "FOLLOWING";
        case MissionState::PERSON_LOST: return "PERSON_LOST";
        case MissionState::OBSTACLE_AVOIDANCE: return "OBSTACLE_AVOIDANCE";
        case MissionState::RETURNING: return "RETURNING";
        case MissionState::LANDING: return "LANDING";
        case MissionState::EMERGENCY: return "EMERGENCY";
        default: return "UNKNOWN";
    }
}

void NavigationNode::transition_to_state(MissionState new_state) {
    RCLCPP_INFO(this->get_logger(), "State transition: %s -> %s",
        state_to_string(current_state_).c_str(),
        state_to_string(new_state).c_str()
    );
    
    current_state_ = new_state;
    state_entry_time_ = this->now();
}

// ========== Navigation Logic Methods ==========

void NavigationNode::execute_follow_behavior() {
    if (!is_person_detection_valid()) {
        return;
    }
    
    // Compute target position
    Eigen::Vector3f target_pos = compute_follow_target_position();
    
    // Compute velocity command
    Eigen::Vector3f velocity_cmd = compute_velocity_command(target_pos);
    
    // Compute desired yaw
    float desired_yaw = compute_desired_yaw(person_world_position_);
    float yaw_error = desired_yaw - drone_state_.yaw;
    
    // Normalize yaw error to [-pi, pi]
    while (yaw_error > M_PI) yaw_error -= 2.0f * M_PI;
    while (yaw_error < -M_PI) yaw_error += 2.0f * M_PI;
    
    float yaw_rate = params_.yaw_p_gain * yaw_error;
    
    // Check safety
    if (!check_safety(velocity_cmd)) {
        RCLCPP_WARN(this->get_logger(), "Safety check failed! Stopping.");
        velocity_cmd = Eigen::Vector3f::Zero();
        yaw_rate = 0.0f;
    }
    
    // Publish command
    publish_velocity_setpoint(velocity_cmd, yaw_rate);
    
    RCLCPP_DEBUG(this->get_logger(), 
        "Following: velocity=(%.2f, %.2f, %.2f), yaw_rate=%.2f",
        velocity_cmd.x(), velocity_cmd.y(), velocity_cmd.z(), yaw_rate
    );
}

void NavigationNode::execute_search_behavior() {
    // Simple search: hover and rotate slowly
    Eigen::Vector3f zero_velocity = Eigen::Vector3f::Zero();
    float search_yaw_rate = 0.3f;  // Slow rotation
    
    publish_velocity_setpoint(zero_velocity, search_yaw_rate);
    
    RCLCPP_DEBUG(this->get_logger(), "Searching for person...");
}

void NavigationNode::execute_hover_behavior() {
    // Maintain position
    publish_velocity_setpoint(Eigen::Vector3f::Zero(), 0.0f);
}

Eigen::Vector3f NavigationNode::compute_follow_target_position() {
    // Compute target position behind the person
    
    // Get person's direction of motion (simplified - use velocity if available)
    Eigen::Vector3f person_direction = person_world_velocity_.normalized();
    if (person_world_velocity_.norm() < 0.1f) {
        // If person is stationary, maintain current relative position
        person_direction = (drone_state_.position - person_world_position_).normalized();
    }
    
    // Target position is behind the person at target_follow_distance
    Eigen::Vector3f target_pos = person_world_position_ - 
                                  person_direction * params_.target_follow_distance;
    
    // Set target height
    target_pos.z() = params_.target_height;
    
    return target_pos;
}

Eigen::Vector3f NavigationNode::compute_velocity_command(
    const Eigen::Vector3f& target_position) 
{
    // Simple proportional controller
    Eigen::Vector3f error = target_position - drone_state_.position;
    Eigen::Vector3f velocity = params_.position_p_gain * error;
    
    // Limit velocity
    float speed = velocity.norm();
    if (speed > params_.max_follow_speed) {
        velocity = velocity * (params_.max_follow_speed / speed);
    }
    
    return velocity;
}

float NavigationNode::compute_desired_yaw(const Eigen::Vector3f& target_position) {
    // Face towards the target (person)
    Eigen::Vector3f to_target = target_position - drone_state_.position;
    return std::atan2(to_target.y(), to_target.x());
}

Eigen::Vector3f NavigationNode::transform_to_world_frame(
    const Eigen::Vector3f& person_camera) 
{
    // Transform from camera frame to base_link (drone body)
    geometry_msgs::msg::TransformStamped transform;
    
    try {
        transform = tf_buffer_->lookupTransform(
            "base_link", "camera_link", tf2::TimePointZero
        );
        
        // Apply transform
        geometry_msgs::msg::PointStamped point_camera;
        point_camera.header.frame_id = "camera_link";
        point_camera.point.x = person_camera.x();
        point_camera.point.y = person_camera.y();
        point_camera.point.z = person_camera.z();
        
        geometry_msgs::msg::PointStamped point_body;
        tf2::doTransform(point_camera, point_body, transform);
        
        // Transform from body to world using drone orientation and position
        Eigen::Vector3f person_body(
            point_body.point.x, point_body.point.y, point_body.point.z
        );
        
        Eigen::Vector3f person_world = drone_state_.position + 
                                        drone_state_.orientation * person_body;
        
        return person_world;
        
    } catch (const tf2::TransformException& ex) {
        RCLCPP_ERROR(this->get_logger(), "Transform error: %s", ex.what());
        throw;
    }
}

// ========== Safety Methods ==========

bool NavigationNode::check_safety(const Eigen::Vector3f& velocity_cmd) {
    // Check altitude limits
    if (drone_state_.position.z() < params_.min_altitude) {
        RCLCPP_WARN(this->get_logger(), "Below minimum altitude!");
        return false;
    }
    
    if (drone_state_.position.z() > params_.max_altitude) {
        RCLCPP_WARN(this->get_logger(), "Above maximum altitude!");
        return false;
    }
    
    // Check velocity limits
    if (velocity_cmd.norm() > params_.max_follow_speed) {
        RCLCPP_WARN(this->get_logger(), "Velocity exceeds maximum!");
        return false;
    }
    
    // TODO: Check obstacle distances
    
    return true;
}

bool NavigationNode::is_person_detection_valid() const {
    if (!last_person_detection_.valid) {
        return false;
    }
    
    // Check confidence
    if (last_person_detection_.confidence < params_.min_detection_confidence) {
        return false;
    }
    
    // Check if detection is recent
    double time_since_detection = (this->now() - last_detection_time_).seconds();
    if (time_since_detection > params_.detection_timeout) {
        return false;
    }
    
    // Check distance limits
    float distance = (person_world_position_ - drone_state_.position).norm();
    if (distance < params_.min_follow_distance || 
        distance > params_.max_follow_distance) {
        return false;
    }
    
    return true;
}

bool NavigationNode::is_person_lost() const {
    double time_since_detection = (this->now() - last_detection_time_).seconds();
    return time_since_detection > params_.detection_timeout;
}

// ========== Publishing Methods ==========

void NavigationNode::publish_velocity_setpoint(
    const Eigen::Vector3f& velocity, float yaw_rate) 
{
    geometry_msgs::msg::TwistStamped msg;
    msg.header.stamp = this->now();
    msg.header.frame_id = "base_link";
    
    msg.twist.linear.x = velocity.x();
    msg.twist.linear.y = velocity.y();
    msg.twist.linear.z = velocity.z();
    msg.twist.angular.z = yaw_rate;
    
    velocity_pub_->publish(msg);
}

void NavigationNode::publish_mission_status() {
    std_msgs::msg::String msg;
    msg.data = state_to_string(current_state_);
    mission_status_pub_->publish(msg);
}

} // namespace cogidrone_navigation

// Main function
int main(int argc, char** argv) {
    rclcpp::init(argc, argv);
    auto node = std::make_shared<cogidrone_navigation::NavigationNode>();
    rclcpp::spin(node);
    rclcpp::shutdown();
    return 0;
}
