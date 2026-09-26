# Navigation Quick Start Guide

## What I've Created For You

I've designed and implemented a complete navigation system architecture with starter code. Here's what you now have:

### 📁 Files Created

1. **NAVIGATION_DESIGN.md** - Comprehensive design document covering:
   - System architecture
   - Component breakdown (State Machine, Behavior Planner, Path Planner, Trajectory Generator)
   - Person following algorithm
   - ROS2 integration strategy
   - Safety mechanisms
   - Testing approach
   - Development roadmap

2. **navigation_node.hpp** - Header file with:
   - Mission state machine enum
   - Data structures (PersonDetection, DroneState, NavigationParams)
   - NavigationNode class interface
   - All method declarations

3. **navigation_node.cpp** - Implementation with:
   - Complete state machine logic
   - Follow, search, and hover behaviors
   - Coordinate frame transformations
   - Safety checks
   - ROS2 pub/sub setup
   - Working skeleton ready to extend

---

## 🎯 Core Concepts Explained

### The System Architecture

```
Person Detection (YOLO) → Navigation Node → Velocity Commands → Control → Motors
         ↑                      ↑
         |                      |
    Camera Frame          World Frame (VIO)
```

### Key Ideas

1. **State Machine**: Manages mission phases (IDLE → SEARCHING → FOLLOWING → etc.)
2. **Follow Behavior**: Computes where the drone should be to follow the person
3. **Velocity Commands**: Proportional control to reach target position
4. **Coordinate Transforms**: Camera frame → Drone body → World frame
5. **Safety Checks**: Altitude limits, velocity limits, detection validation

---

## 🚀 How to Get Started

### Step 1: Set Up ROS2 Environment

```bash
# Install ROS2 (Humble or Jazzy)
# Follow: https://docs.ros.org/en/humble/Installation.html

# Source ROS2
source /opt/ros/humble/setup.bash  # or jazzy

# Install dependencies
sudo apt install ros-humble-tf2-ros ros-humble-geometry-msgs \
                 ros-humble-nav-msgs ros-humble-sensor-msgs \
                 libeigen3-dev
```

### Step 2: Create ROS2 Package

```bash
# Navigate to your workspace
cd ~/cogidrone_ws/src

# Create package
ros2 pkg create cogidrone_navigation \
    --build-type ament_cmake \
    --dependencies rclcpp geometry_msgs nav_msgs std_msgs tf2_ros \
                   tf2_geometry_msgs eigen3_cmake_module

# Copy the files I created
cp /path/to/navigation_node.hpp cogidrone_navigation/include/cogidrone_navigation/
cp /path/to/navigation_node.cpp cogidrone_navigation/src/
```

### Step 3: Set Up CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.8)
project(cogidrone_navigation)

# Find dependencies
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(nav_msgs REQUIRED)
find_package(std_msgs REQUIRED)
find_package(tf2_ros REQUIRED)
find_package(tf2_geometry_msgs REQUIRED)
find_package(Eigen3 REQUIRED)

# Include directories
include_directories(
  include
  ${EIGEN3_INCLUDE_DIRS}
)

# Add executable
add_executable(navigation_node src/navigation_node.cpp)

ament_target_dependencies(navigation_node
  rclcpp
  geometry_msgs
  nav_msgs
  std_msgs
  tf2_ros
  tf2_geometry_msgs
)

# Install
install(TARGETS navigation_node
  DESTINATION lib/${PROJECT_NAME}
)

install(DIRECTORY include/
  DESTINATION include/
)

install(DIRECTORY launch config
  DESTINATION share/${PROJECT_NAME}
)

ament_package()
```

### Step 4: Create Configuration File

```bash
mkdir -p cogidrone_navigation/config
```

Create `config/navigation_params.yaml`:
```yaml
navigation_node:
  ros__parameters:
    # Following parameters
    target_follow_distance: 4.0
    target_height: 2.5
    max_follow_speed: 2.0
    
    # Control
    control_frequency: 30.0
    position_p_gain: 0.8
    yaw_p_gain: 1.5
    
    # Detection
    detection_timeout: 2.0
    min_detection_confidence: 0.7
    
    # Safety
    min_altitude: 0.5
    max_altitude: 8.0
```

### Step 5: Build and Test

```bash
# Build
cd ~/cogidrone_ws
colcon build --packages-select cogidrone_navigation

# Source
source install/setup.bash

# Run
ros2 run cogidrone_navigation navigation_node \
    --ros-args --params-file src/cogidrone_navigation/config/navigation_params.yaml
```

---

## 🧪 Testing Strategy

### Phase 1: Unit Testing (No Hardware)

```bash
# Test with mock data
ros2 topic pub /estimation/vio_odometry nav_msgs/msg/Odometry "..."
ros2 topic pub /perception/person_detection geometry_msgs/msg/PoseStamped "..."

# Monitor outputs
ros2 topic echo /navigation/velocity_setpoint
ros2 topic echo /navigation/mission_status
```

### Phase 2: Simulation (PX4 SITL)

1. Start PX4 SITL simulation
2. Run navigation node
3. Publish mock person detections
4. Observe drone behavior in Gazebo

### Phase 3: Hardware Testing

1. Start with manual control
2. Test state transitions
3. Test person detection integration
4. Test actual following behavior

---

## 🔧 Next Implementation Steps

Based on the code I provided, here's what to do **in order**:

### 1. **Get Basic Node Running** ✅ (Done - code provided)
   - Build the package
   - Run the node
   - Verify it starts without errors

### 2. **Integrate with Perception** (Week 1-2)
   ```cpp
   // TODO in perception module:
   // - Publish person detections on /perception/person_detection
   // - Include confidence scores
   // - Ensure proper timestamp and frame_id
   ```

### 3. **Integrate with Estimation** (Week 1-2)
   ```cpp
   // TODO in estimation module:
   // - Publish VIO odometry on /estimation/vio_odometry
   // - Set up TF2 transforms (world → base_link → camera_link)
   // - Ensure accurate position and orientation
   ```

### 4. **Improve Person Tracking** (Week 3)
   - Implement Kalman filter for smoother tracking
   - Add velocity estimation
   - Handle detection gaps better
   
   **File to create**: `person_tracker.hpp/cpp`

### 5. **Add Advanced Path Planning** (Week 4)
   - Implement Dynamic Window Approach (DWA)
   - Add obstacle avoidance
   - Smooth trajectory generation
   
   **Files to create**: `local_planner.hpp/cpp`, `dwa_planner.hpp/cpp`

### 6. **Integrate with Control** (Week 5)
   - Connect to MAVSDK or uXRCE-DDS
   - Send velocity commands to PX4
   - Handle offboard mode
   
   **File to create**: `flight_interface.hpp/cpp`

### 7. **Add Safety Features** (Week 6)
   - Geofencing
   - Battery monitoring
   - Obstacle detection integration
   - Emergency behaviors
   
   **File to enhance**: `navigation_node.cpp` (safety_monitor methods)

### 8. **Tune and Test** (Week 7-8)
   - Parameter tuning
   - Real-world testing
   - Edge case handling

---

## 💡 Key Design Decisions Explained

### Why a State Machine?

The state machine provides:
- **Clear mission phases**: Easy to understand what the drone is doing
- **Safe transitions**: Controlled changes between behaviors
- **Debugging**: Easy to log and monitor states
- **Extensibility**: Easy to add new states/behaviors

### Why Velocity Commands vs Position Commands?

**Velocity control** (what I implemented):
- ✅ More responsive to moving targets
- ✅ Smoother following behavior
- ✅ Better for dynamic environments
- ❌ Requires position hold in flight controller

**Position control** (alternative):
- ✅ More accurate positioning
- ✅ Easier to implement
- ❌ Less smooth for moving targets

### Why Simple Proportional Control First?

Starting simple allows you to:
1. Get something working quickly
2. Understand the basics
3. Identify problems early
4. Then upgrade to PID/MPC later

The code structure supports easy upgrade to more sophisticated control.

---

## 🎨 Customization Points

### Change Follow Distance
```yaml
# config/navigation_params.yaml
target_follow_distance: 5.0  # Increase for more distance
```

### Change Follow Position
```cpp
// navigation_node.cpp, compute_follow_target_position()
// Currently: behind the person
// Change to: in front, to the side, above, etc.

Eigen::Vector3f target_pos = person_world_position_ + 
                              person_direction * params_.target_follow_distance;  // In front
```

### Add New States
```cpp
// navigation_node.hpp
enum class MissionState {
    // ... existing states ...
    ORBITING,  // Circle around person
    LEADING,   // Fly ahead of person
};
```

---

## 🐛 Common Issues & Solutions

### Issue: TF Transform Failed

**Symptom**: Warning messages about TF lookup failures

**Solution**:
```bash
# Check TF tree
ros2 run tf2_tools view_frames

# Verify transforms are being published
ros2 topic echo /tf
```

### Issue: Person Detection Not Received

**Symptom**: Node stays in SEARCHING state

**Solution**:
```bash
# Check topic
ros2 topic list | grep person_detection
ros2 topic echo /perception/person_detection

# Check perception node is running
ros2 node list
```

### Issue: Drone Not Moving

**Symptom**: Velocity commands published but drone stationary

**Solution**:
1. Check PX4 is in OFFBOARD mode
2. Verify MAVSDK connection
3. Check control module is receiving commands
4. Verify safety checks are passing

---

## 📚 Additional Resources

### ROS2 Learning
- [ROS2 Humble Tutorials](https://docs.ros.org/en/humble/Tutorials.html)
- [TF2 Tutorial](https://docs.ros.org/en/humble/Tutorials/Intermediate/Tf2/Tf2-Main.html)
- [Navigation2](https://navigation.ros.org/)

### Drone Control
- [PX4 User Guide](https://docs.px4.io/main/en/)
- [MAVSDK Guide](https://mavsdk.mavlink.io/main/en/cpp/)
- [Offboard Control](https://docs.px4.io/main/en/flight_modes/offboard.html)

### Algorithms
- [Dynamic Window Approach Paper](https://ieeexplore.ieee.org/document/580977)
- [Visual Servoing Tutorial](https://visp-doc.inria.fr/doxygen/visp-daily/tutorial-ibvs.html)

---

## 🎯 Summary: Where to Focus

Based on your project needs (GPS-free person following), here's the priority order:

### **High Priority** (Start Here)
1. ✅ **Set up ROS2 package** - Build and run the node
2. ✅ **Integrate perception** - Get person detections flowing
3. ✅ **Integrate estimation** - Get drone state (VIO odometry)
4. ✅ **Test state machine** - Verify transitions work
5. ✅ **Test in simulation** - Use PX4 SITL + Gazebo

### **Medium Priority** (Next)
6. **Add Kalman filter tracking** - Smooth person position
7. **Tune control parameters** - Get good follow behavior
8. **Add obstacle avoidance** - Safety feature
9. **Connect to actual flight controller** - Hardware integration

### **Lower Priority** (Later)
10. Advanced path planning (DWA)
11. Trajectory optimization
12. Multiple person tracking
13. Gesture recognition

---

## 🚁 Final Tips

1. **Start in simulation** - PX4 SITL is your friend
2. **Test incrementally** - Don't try to do everything at once
3. **Log everything** - Use RCLCPP_INFO/DEBUG liberally
4. **Visualize in RViz** - See what the drone "sees"
5. **Safety first** - Always have manual override ready
6. **Parameter tuning** - Expect to spend time tuning gains
7. **Document as you go** - Future you will thank present you

---

## 🤝 Need Help?

If you get stuck:
1. Check ROS2 logs: `ros2 run cogidrone_navigation navigation_node --ros-args --log-level debug`
2. Use RViz for visualization
3. Test components individually
4. Refer to NAVIGATION_DESIGN.md for detailed explanations

**Good luck with your autonomous drone project! 🚁**

The foundation is solid - now it's time to bring it to life!
