### State estimation
Combine those measurements to answer: 
    **"Where am I, how am I oriented, and how am I moving?"**

Output is something like

$$
x=[position,\ velocity,\ orientation,\ angular\ velocity]
$$

This is where an EKF, VIO, etc. come in. If GPS is unavailable, VIO becomes especially important.

**Today:** not written yet. Navigation reads `/estimation/vio_odometry` (`nav_msgs/Odometry`, ENU),
and until this subsystem exists the `Pixhawk` controller publishes PX4's EKF2 estimate there. The
plan (cuVSLAM) is in [docs/jetson.md](../../docs/jetson.md#localization-and-mapping-plan).
