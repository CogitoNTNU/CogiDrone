# Drone
**This directory contains ALL on-board drone code and any supporting code used for operating the drone** 

## Update loop
The drone uses the following update loop:
```
┌─> Perception
|       |
|       v
|    Estimate
|       |
|       v
|  Representaion
|       |
|       v
|   Navigation
|       |
|       v
|    Control
|       |
|       v
└──    Act
```

### Spatial perception 
- IMU: acceleration + rotation
- Cameras: visual information
- Range/depth sensor: obstacles/distances
- GPS/GNSS (when available)
- Barometer: altitude
  
### State estimation
Combine those measurements to answer: 
    **"Where am I, how am I oriented, and how am I moving?"**

Output is something like

$$
x=[position,\ velocity,\ orientation,\ angular\ velocity]
$$

This is where an EKF, VIO, etc. come in. If GPS is unavailable, VIO becomes especially important.

### Spatial representation
Answer:
    **"What’s around me?"**

For example:
- obstacle locations
- depth map
- occupancy grid
- local 3D map
  
SLAM may be used here if the drone needs to build a map while simultaneously localizing itself.

### Navigation
Answer:
    **"Given where I am and what’s around me, where should I go next?"**

This includes:
- mission logic
- path planning
- obstacle avoidance
- choosing the next waypoint/trajectory

### Control
Turn the desired trajectory into commands:
$$
\text{desired position}
\rightarrow
\text{desired velocity}
\rightarrow
\text{desired attitude/thrust}
$$

Controllers such as PID/MPC calculate what the drone needs to do.

### Act
Flight controller sends commands to ESCs → motors change speed → drone moves.
Then sensors measure the new situation, and the whole thing happens again.


## The important part for your project
These don’t necessarily all run at the same frequency.
| Part | Rough frequency |
| --- | --- |
| IMU sampling | 200–1000+ Hz |
| Attitude/rate control | 200–1000 Hz |
| State estimation | 50–400 Hz |
| VIO | ~20–60 Hz |
| Camera/depth perception | ~10–60 Hz |
| Obstacle avoidance/planning | ~5–30 Hz |
| High-level mission logic | ~1–10 Hz |

So architecturally, I wouldn’t make one giant update loop. Your flight controller should handle the extremely fast, safety-critical estimation/control loops, while your onboard computer handles heavier autonomy tasks like VIO, perception, mapping and planning.

For Cogidrone, a sensible split is basically:
```
Sensors → onboard computer (perception/VIO/planning) → desired trajectory → flight controller (estimation/control) → motors.
```

That distinction will matter a lot when you’re deciding how powerful your onboard computer actually needs to be.
