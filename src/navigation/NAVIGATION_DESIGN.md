# Navigation System Design for CogiDrone

## Overview
This document outlines the design and implementation strategy for the autonomous person-following navigation system using ROS2.

## Core Mission: GPS-Free Person Following

### Key Objectives
1. **Detect** a person using YOLO vision system
2. **Track** the person's position relative to the drone
3. **Plan** a safe trajectory to follow the person
4. **Avoid** obstacles while maintaining target tracking
5. **React** to dynamic changes in person movement

---

## Architecture Design

```
┌─────────────────────────────────────────────────────────────┐
│                    NAVIGATION MODULE                         │
├─────────────────────────────────────────────────────────────┤
│                                                               │
│  ┌────────────────┐      ┌────────────────┐                │
│  │  Mission       │      │  Behavior      │                │
│  │  State Machine │─────▶│  Planner       │                │
│  └────────────────┘      └────────────────┘                │
│         │                        │                           │
│         │                        ▼                           │
│         │              ┌────────────────┐                   │
│         │              │  Local Path    │                   │
│         └─────────────▶│  Planner       │                   │
│                        └────────────────┘                   │
│                                │                             │
│                                ▼                             │
│                      ┌────────────────┐                     │
│                      │  Trajectory    │                     │
│                      │  Generator     │                     │
│                      └────────────────┘                     │
│                                │                             │
└────────────────────────────────┼─────────────────────────────┘
                                 │
                                 ▼
                    ┌────────────────────┐
                    │  CONTROL MODULE    │
                    │  (velocity/position│
                    │   setpoints)       │
                    └────────────────────┘
```

### ROS2 Node Architecture

```
Input Topics (Subscribed):
├─ /perception/person_detection (Person pose in camera frame)
├─ /estimation/drone_state (Position, velocity, orientation)
├─ /representation/obstacle_map (Local occupancy grid/point cloud)
├─ /estimation/vio_odometry (Visual-inertial odometry)
└─ /manual_override (RC controller input)

Output Topics (Published):
├─ /navigation/trajectory (Desired trajectory)
├─ /navigation/velocity_setpoint (Target velocity)
├─ /navigation/mission_status (Current state, diagnostics)
└─ /navigation/waypoint (Next target position)

Services:
├─ /navigation/start_mission
├─ /navigation/stop_mission
├─ /navigation/set_follow_distance
└─ /navigation/emergency_land
```

---

## Component Design Details

### 1. Mission State Machine

**Purpose**: High-level mission management and safety

**States**:
```cpp
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
```

**Transitions**:
- `IDLE → SEARCHING`: Mission start command received
- `SEARCHING → PERSON_DETECTED`: Person detection confidence > threshold
- `PERSON_DETECTED → FOLLOWING`: Stable tracking for N frames
- `FOLLOWING → PERSON_LOST`: Detection lost for > timeout
- `FOLLOWING → OBSTACLE_AVOIDANCE`: Collision imminent
- `* → EMERGENCY`: Critical failure or manual override

**ROS2 Implementation**:
```cpp
class MissionStateMachine : public rclcpp::Node {
private:
    MissionState current_state_;
    rclcpp::TimerBase::SharedPtr state_machine_timer_;
    
public:
    void update_state_machine();
    void handle_person_detection(const PersonDetection::SharedPtr msg);
    void handle_manual_override(const ManualControl::SharedPtr msg);
};
```

---

### 2. Behavior Planner

**Purpose**: Decide *what* to do based on current state and inputs

**Key Behaviors for Person Following**:

#### a) **Follow Behavior**
- Maintain optimal following distance (e.g., 3-5 meters behind)
- Maintain optimal height (e.g., 2-3 meters above ground)
- Keep person centered in camera FOV
- Adjust speed based on person's velocity

```cpp
struct FollowParameters {
    float target_distance = 4.0f;      // meters behind person
    float target_height = 2.5f;        // meters above ground
    float max_follow_speed = 3.0f;     // m/s
    float min_follow_distance = 2.0f;  // safety distance
    float max_follow_distance = 10.0f; // lose tracking beyond this
};
```

#### b) **Search Behavior**
When person is lost:
- Hover in place for T seconds
- Execute expanding spiral search pattern
- Slowly rotate to scan environment
- Lower altitude slightly for better view

#### c) **Obstacle Avoidance Behavior**
- Dynamic window approach (DWA)
- Vector field histogram (VFH+)
- Potential field method
- Emergency stop if collision imminent

**ROS2 Implementation**:
```cpp
class BehaviorPlanner : public rclcpp::Node {
private:
    FollowParameters params_;
    
public:
    Behavior plan_behavior(
        const MissionState& state,
        const PersonDetection& person,
        const DroneState& drone_state,
        const ObstacleMap& obstacles
    );
};
```

---

### 3. Local Path Planner

**Purpose**: Generate collision-free paths to waypoints

**Recommended Algorithms**:

#### Option A: **Dynamic Window Approach (DWA)** [RECOMMENDED]
- Fast, reactive planning
- Considers dynamic constraints (max velocity, acceleration)
- Works well for following moving targets
- Evaluates velocity commands in real-time

**DWA Cost Function**:
```
cost = α·heading(v,ω) + β·velocity(v,ω) + γ·obstacle_distance(v,ω)

where:
- heading: alignment with goal direction
- velocity: preference for faster movement
- obstacle_distance: clearance from obstacles
```

#### Option B: **ROS2 Nav2 with Custom Plugins**
- Use Nav2's `nav2_regulated_pure_pursuit_controller`
- Integrate with DWA local planner
- Leverage existing ROS2 navigation stack

#### Option C: **Model Predictive Control (MPC)**
- Predict future trajectory
- Optimize over prediction horizon
- Better for smooth, optimal paths
- More computationally expensive

**ROS2 Implementation**:
```cpp
class LocalPathPlanner : public rclcpp::Node {
private:
    // DWA parameters
    struct DWAConfig {
        float max_vel_x = 2.0f;
        float max_vel_y = 2.0f;
        float max_vel_z = 1.0f;
        float max_yaw_rate = 1.0f;
        float vel_resolution = 0.1f;
        float prediction_time = 2.0f;
    } config_;
    
public:
    std::vector<Waypoint> plan_path(
        const Eigen::Vector3f& current_pos,
        const Eigen::Vector3f& goal_pos,
        const ObstacleMap& obstacles
    );
    
    VelocityCommand compute_velocity_command(
        const DroneState& state,
        const Waypoint& target,
        const ObstacleMap& obstacles
    );
};
```

---

### 4. Trajectory Generator

**Purpose**: Convert high-level waypoints into smooth, dynamically feasible trajectories

**Key Features**:
- Minimum-snap trajectory generation
- Respect drone dynamics constraints
- Smooth acceleration profiles
- Continuous derivatives for stable control

**Methods**:

#### a) **Polynomial Trajectory Generation**
Generate 5th or 7th order polynomials between waypoints

#### b) **B-Spline Trajectories**
Smooth curves with local control

#### c) **Ruckig** (ROS2 library)
Real-time trajectory generation with jerk limits

**ROS2 Implementation**:
```cpp
class TrajectoryGenerator : public rclcpp::Node {
private:
    struct TrajectoryConstraints {
        float max_velocity = 3.0f;
        float max_acceleration = 2.0f;
        float max_jerk = 5.0f;
    } constraints_;
    
public:
    Trajectory generate_trajectory(
        const std::vector<Waypoint>& waypoints,
        const DroneState& current_state
    );
};
```

---

## Person Following Algorithm

### Core Algorithm Concept

**1. Person Detection → Camera Frame**
```
YOLO Detection → Bounding box [x, y, width, height]
                → Depth from RealSense → 3D position in camera frame
```

**2. Transform to Drone Body Frame**
```cpp
// Using TF2
geometry_msgs::msg::TransformStamped transform;
transform = tf_buffer_->lookupTransform(
    "base_link",      // drone body frame
    "camera_link",    // camera frame
    tf2::TimePointZero
);

geometry_msgs::msg::PointStamped person_camera_frame;
geometry_msgs::msg::PointStamped person_body_frame;
tf2::doTransform(person_camera_frame, person_body_frame, transform);
```

**3. Transform to World Frame (VIO)**
```cpp
// Using VIO odometry to get drone position in world
Eigen::Vector3f person_world = drone_position_world + 
                                drone_orientation * person_body_frame;
```

**4. Compute Target Position**
```cpp
// Calculate desired drone position (behind person)
Eigen::Vector3f person_to_drone = -person_velocity_normalized * target_distance;
person_to_drone.z() = target_height;  // Set desired height

Eigen::Vector3f target_position = person_world + person_to_drone;
```

**5. Command Generation**
```cpp
// Generate velocity command toward target
Eigen::Vector3f error = target_position - drone_position;
float distance = error.norm();

// Proportional control with velocity limits
Eigen::Vector3f velocity_cmd = K_p * error;
velocity_cmd = velocity_cmd.cwiseMin(max_velocity);
velocity_cmd = velocity_cmd.cwiseMax(-max_velocity);

// Also align yaw to face person
float desired_yaw = atan2(person_world.y() - drone_position.y(),
                          person_world.x() - drone_position.x());
float yaw_rate = K_yaw * (desired_yaw - current_yaw);
```

---

## Critical Implementation Details

### 1. Coordinate Frame Management

**Frames Used**:
- `world`: Fixed world frame (VIO origin)
- `base_link`: Drone body frame (FLU - Forward-Left-Up)
- `camera_link`: Camera frame
- `map`: Optional SLAM map frame

**TF2 Tree**:
```
world
  └─ base_link (from VIO/SLAM)
       ├─ camera_link
       ├─ imu_link
       └─ (other sensors)
```

### 2. Person Tracking Stability

**Challenges**:
- Detection can be noisy/intermittent
- Need to filter detections
- Predict person position during occlusion

**Solutions**:

#### a) Kalman Filter for Person Tracking
```cpp
class PersonTracker {
private:
    Eigen::VectorXf state_;  // [px, py, pz, vx, vy, vz]
    Eigen::MatrixXf covariance_;
    
public:
    void predict(float dt);
    void update(const Eigen::Vector3f& measurement);
    Eigen::Vector3f get_filtered_position();
    Eigen::Vector3f get_predicted_position(float time_ahead);
};
```

#### b) Detection Confidence Filtering
```cpp
// Only trust detections with high confidence and consistent positions
bool is_detection_valid(const PersonDetection& det) {
    return det.confidence > 0.7 &&
           det.distance > min_distance &&
           det.distance < max_distance &&
           abs(det.position.z()) < max_height;
}
```

### 3. Safety Mechanisms

**Critical Safety Checks**:
```cpp
class SafetyMonitor : public rclcpp::Node {
public:
    bool check_safety(const DroneState& state, 
                      const VelocityCommand& cmd) {
        // Check altitude limits
        if (state.position.z() < min_altitude ||
            state.position.z() > max_altitude) {
            trigger_emergency();
            return false;
        }
        
        // Check velocity limits
        if (cmd.velocity.norm() > max_velocity) {
            return false;
        }
        
        // Check obstacle proximity
        if (min_obstacle_distance < safety_distance) {
            trigger_stop();
            return false;
        }
        
        // Check battery level
        if (battery_level < min_battery) {
            trigger_return_home();
            return false;
        }
        
        return true;
    }
};
```

---

## ROS2 Integration Strategy

### Package Structure
```
cogidrone_navigation/
├── CMakeLists.txt
├── package.xml
├── include/cogidrone_navigation/
│   ├── mission_state_machine.hpp
│   ├── behavior_planner.hpp
│   ├── local_planner.hpp
│   ├── trajectory_generator.hpp
│   ├── person_tracker.hpp
│   └── safety_monitor.hpp
├── src/
│   ├── mission_state_machine.cpp
│   ├── behavior_planner.cpp
│   ├── local_planner.cpp
│   ├── trajectory_generator.cpp
│   ├── person_tracker.cpp
│   ├── safety_monitor.cpp
│   └── navigation_node.cpp (main)
├── config/
│   ├── navigation_params.yaml
│   └── behavior_params.yaml
├── launch/
│   └── navigation.launch.py
└── test/
    └── test_navigation.cpp
```

### Key ROS2 Dependencies
```xml
<!-- package.xml -->
<depend>rclcpp</depend>
<depend>std_msgs</depend>
<depend>geometry_msgs</depend>
<depend>nav_msgs</depend>
<depend>sensor_msgs</depend>
<depend>tf2_ros</depend>
<depend>tf2_geometry_msgs</depend>
<depend>eigen3_cmake_module</depend>
<depend>vision_msgs</depend>  <!-- For person detections -->
```

### Launch File Example
```python
# launch/navigation.launch.py
from launch import LaunchDescription
from launch_ros.actions import Node
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration

def generate_launch_description():
    return LaunchDescription([
        DeclareLaunchArgument(
            'use_sim_time',
            default_value='false',
            description='Use simulation time'
        ),
        
        Node(
            package='cogidrone_navigation',
            executable='navigation_node',
            name='navigation',
            parameters=[
                {'use_sim_time': LaunchConfiguration('use_sim_time')},
                'config/navigation_params.yaml'
            ],
            output='screen'
        ),
    ])
```

### Parameter Configuration
```yaml
# config/navigation_params.yaml
navigation:
  ros__parameters:
    # Update rates
    control_frequency: 30.0  # Hz
    
    # Following parameters
    target_follow_distance: 4.0  # meters
    target_height: 2.5  # meters
    max_follow_speed: 3.0  # m/s
    
    # Safety limits
    min_altitude: 0.5  # meters
    max_altitude: 10.0  # meters
    safety_distance: 1.0  # meters from obstacles
    
    # Person tracking
    detection_timeout: 2.0  # seconds before person considered lost
    min_detection_confidence: 0.7
    
    # DWA parameters
    max_vel_x: 2.0
    max_vel_y: 2.0
    max_vel_z: 1.0
    max_yaw_rate: 1.0
    
    # Control gains
    position_p_gain: 1.0
    yaw_p_gain: 2.0
```

---

## Integration with PX4 via MAVSDK/uXRCE-DDS

### Option 1: MAVSDK (Simpler, Recommended for Start)
```cpp
#include <mavsdk/mavsdk.h>
#include <mavsdk/plugins/offboard/offboard.h>
#include <mavsdk/plugins/telemetry/telemetry.h>

class FlightInterface {
private:
    std::shared_ptr<mavsdk::System> system_;
    std::shared_ptr<mavsdk::Offboard> offboard_;
    std::shared_ptr<mavsdk::Telemetry> telemetry_;
    
public:
    void send_velocity_command(const Eigen::Vector3f& velocity, float yaw_rate) {
        mavsdk::Offboard::VelocityNedYaw cmd{};
        cmd.north_m_s = velocity.x();
        cmd.east_m_s = velocity.y();
        cmd.down_m_s = -velocity.z();  // NED frame: down is positive
        cmd.yaw_deg = yaw_rate;
        
        offboard_->set_velocity_ned(cmd);
    }
};
```

### Option 2: uXRCE-DDS (Direct ROS2 Integration)
```cpp
// Publish directly to PX4 topics
auto trajectory_pub = this->create_publisher<px4_msgs::msg::TrajectorySetpoint>(
    "/fmu/in/trajectory_setpoint", 10);

px4_msgs::msg::TrajectorySetpoint msg;
msg.position = {target_pos.x(), target_pos.y(), target_pos.z()};
msg.velocity = {target_vel.x(), target_vel.y(), target_vel.z()};
msg.yaw = target_yaw;
trajectory_pub->publish(msg);
```

---

## Testing Strategy

### 1. Unit Tests
- Test individual components (state machine, planners)
- Mock ROS2 interfaces
- Use Google Test / Catch2

### 2. Simulation Testing
Use PX4 SITL + Gazebo:
```bash
# Terminal 1: Start PX4 SITL
cd PX4-Autopilot
make px4_sitl gz_x500

# Terminal 2: Start ROS2 navigation
ros2 launch cogidrone_navigation navigation.launch.py use_sim_time:=true

# Terminal 3: Publish mock person detections
ros2 topic pub /perception/person_detection ...
```

### 3. Hardware-in-Loop (HIL)
- Test on Jetson with simulated flight controller
- Validate timing and computational performance

### 4. Flight Tests
Progressive validation:
1. Manual position holding
2. Waypoint following
3. Static object tracking
4. Moving person following (slow)
5. Moving person following (fast)

---

## Performance Considerations

### Computational Budget (Jetson Orin Nano)
- **Navigation update**: ~30 Hz (33ms budget)
- **Local planning**: ~10-20 Hz
- **Trajectory generation**: ~10 Hz
- **State machine**: ~10 Hz

### Optimization Tips
1. Use Eigen for linear algebra (fast, optimized)
2. Avoid dynamic memory allocation in loops
3. Use const references
4. Profile with `perf` or `valgrind`
5. Consider GPU offload for complex planning (CUDA)

---

## Development Roadmap

### Phase 1: Foundation (Weeks 1-2)
- [ ] Set up ROS2 package structure
- [ ] Implement basic state machine
- [ ] Create simple hovering behavior
- [ ] Test in simulation

### Phase 2: Core Navigation (Weeks 3-4)
- [ ] Implement person tracker (Kalman filter)
- [ ] Develop follow behavior logic
- [ ] Add DWA local planner
- [ ] Integrate with mock detections

### Phase 3: Safety & Robustness (Weeks 5-6)
- [ ] Add obstacle avoidance
- [ ] Implement safety monitor
- [ ] Handle person loss scenarios
- [ ] Extensive simulation testing

### Phase 4: Hardware Integration (Weeks 7-8)
- [ ] Integrate with real YOLO detections
- [ ] Connect to MAVSDK/PX4
- [ ] Tune parameters on hardware
- [ ] Conduct flight tests

---

## References & Resources

### ROS2 Navigation
- [Nav2 Documentation](https://navigation.ros.org/)
- [ROS2 TF2 Tutorial](https://docs.ros.org/en/humble/Tutorials/Intermediate/Tf2/Tf2-Main.html)

### Path Planning Algorithms
- Dynamic Window Approach: Fox et al. (1997)
- Vector Field Histogram: Borenstein & Koren (1991)

### Person Following
- Visual Servo Control for person tracking
- Model Predictive Control for trajectory tracking

### PX4 Integration
- [MAVSDK Documentation](https://mavsdk.mavlink.io/main/en/)
- [PX4 ROS2 User Guide](https://docs.px4.io/main/en/ros/ros2_comm.html)

---

## Next Steps

1. **Review this design** with your team
2. **Set up development environment** (ROS2 Humble/Jazzy)
3. **Create the package structure**
4. **Start with simulation** before hardware
5. **Implement progressively** (state machine → behavior → planning)

Good luck with your implementation! 🚁
