"""Bridge between the C++ drone core (src/, `cogidrone_core`) and PX4.

The C++ Navigation node speaks plain ROS in ENU (x east, y north, z up) and knows nothing
about PX4. This node fills the gap, so it can fly the sim (and later the real drone):

    PX4 vehicle_local_position (NED)   -> /estimation/vio_odometry      nav_msgs/Odometry (ENU)
    /navigation/velocity_setpoint      -> PX4 offboard velocity setpoint (NED)
    /navigation/mission_status LANDING -> PX4 land

It also does what Navigation does not do yet: arm, take off, then send START_MISSION on
/manual_override. If Navigation stops sending velocity (IDLE, LANDING, crash), the drone
holds position instead of drifting.

State machine:
    WAIT_FOR_PX4 -> ARMING -> TAKEOFF -> MISSION -> LAND
"""

import math
from enum import Enum, auto

import rclpy
from geometry_msgs.msg import TwistStamped
from nav_msgs.msg import Odometry
from std_msgs.msg import String

from cogidrone_control.px4_node import RATE_HZ, Px4Node


class State(Enum):
    WAIT_FOR_PX4 = auto()
    ARMING = auto()
    TAKEOFF = auto()
    MISSION = auto()
    LAND = auto()


def yaw_quaternion(yaw: float) -> tuple[float, float, float, float]:
    """(w, x, y, z) for a pure rotation about z."""
    return math.cos(yaw / 2), 0.0, 0.0, math.sin(yaw / 2)


class CppBridge(Px4Node):
    def __init__(self):
        super().__init__("cpp_bridge")
        self.declare_parameter("altitude", 2.5)  # m, Navigation's target_height default
        # Send START_MISSION after takeoff. false = wait for `ros2 topic pub /manual_override ...`
        self.declare_parameter("auto_start", True)
        # Hold position if Navigation sends nothing for this long.
        self.declare_parameter("command_timeout_s", 0.5)

        p = lambda name: self.get_parameter(name).value
        self.altitude = p("altitude")
        self.auto_start = p("auto_start")
        self.command_timeout = p("command_timeout_s")

        self.odom_pub = self.create_publisher(Odometry, "/estimation/vio_odometry", 10)
        self.override_pub = self.create_publisher(String, "/manual_override", 10)
        self.create_subscription(
            TwistStamped, "/navigation/velocity_setpoint", self.on_velocity, 10
        )
        self.create_subscription(
            String, "/navigation/mission_status", self.on_mission_status, 10
        )

        self.state = State.WAIT_FOR_PX4
        self.state_ticks = 0
        self.hold = None  # (x, y, z, yaw) NED position to hold, None while following velocity
        self.velocity = None  # (vx, vy, vz, yawspeed) NED
        self.velocity_at = None
        self.mission_status = None

        self.create_timer(1.0 / RATE_HZ, self.tick)
        self.get_logger().info("Waiting for PX4 (is the XRCE agent running?)...")

    # --- inputs -------------------------------------------------------------

    def on_position(self, msg):
        super().on_position(msg)
        self.publish_odometry(msg)

    def on_velocity(self, msg: TwistStamped):
        # Navigation fills linear with a world-frame (ENU) velocity and angular.z with the
        # yaw rate (ENU, counter-clockwise), even though frame_id says base_link.
        t = msg.twist
        self.velocity = (t.linear.y, t.linear.x, -t.linear.z, -t.angular.z)
        self.velocity_at = self.get_clock().now()

    def on_mission_status(self, msg: String):
        if msg.data != self.mission_status:
            self.get_logger().info(f"Navigation: {msg.data}")
        self.mission_status = msg.data
        if msg.data == "LANDING" and self.state == State.MISSION:
            self.get_logger().info("Navigation asked to land")
            self.land()
            self.set_state(State.LAND)

    # --- outputs ------------------------------------------------------------

    def publish_odometry(self, pos):
        if not (pos.xy_valid and pos.z_valid):
            return
        odom = Odometry()
        odom.header.stamp = self.get_clock().now().to_msg()
        odom.header.frame_id = "odom"
        odom.child_frame_id = "base_link"
        # NED -> ENU: swap x/y, flip z. Heading is from north, clockwise; ENU yaw is from east, CCW.
        odom.pose.pose.position.x = float(pos.y)
        odom.pose.pose.position.y = float(pos.x)
        odom.pose.pose.position.z = float(-pos.z)
        q = odom.pose.pose.orientation
        q.w, q.x, q.y, q.z = yaw_quaternion(math.pi / 2 - pos.heading)
        # World-frame velocity, which is what Navigation::onOdometry expects.
        odom.twist.twist.linear.x = float(pos.vy)
        odom.twist.twist.linear.y = float(pos.vx)
        odom.twist.twist.linear.z = float(-pos.vz)
        self.odom_pub.publish(odom)

    # --- main loop ------------------------------------------------------------

    def tick(self):
        self.state_ticks += 1

        if self.state == State.WAIT_FOR_PX4:
            if self.px4_ready():
                self.hold_here(z=self.position.z - self.altitude)
                self.get_logger().info(
                    "PX4 ready, streaming setpoints before switching to offboard"
                )
                self.set_state(State.ARMING)
            return

        if self.state == State.LAND:
            return  # PX4 lands on its own; we stop streaming setpoints.

        if self.state == State.MISSION and self.following_navigation():
            self.hold = None
            self.publish_offboard_mode(velocity=True)
            self.publish_velocity_setpoint(*self.velocity)
        else:
            if self.hold is None:
                self.hold_here()
            self.publish_offboard_mode()
            self.publish_setpoint(*self.hold)

        if self.state == State.ARMING:
            # Stream 1 s of setpoints first, then (re)request offboard + arm every 2 s until it sticks.
            if (
                self.state_ticks >= RATE_HZ
                and self.state_ticks % int(2 * RATE_HZ) == RATE_HZ
            ):
                self.arm_offboard()
                self.get_logger().info("Requesting offboard mode + arm")
            if self.is_armed_offboard():
                self.get_logger().info(
                    f"Armed in offboard, taking off to {self.altitude:.1f} m"
                )
                self.set_state(State.TAKEOFF)

        elif self.state == State.TAKEOFF:
            if abs(self.position.z - self.hold[2]) < 0.3:
                self.get_logger().info("At altitude, handing over to Navigation")
                self.set_state(State.MISSION)

        elif self.state == State.MISSION:
            # Navigation starts in IDLE; repeat START_MISSION until it leaves IDLE.
            if (
                self.auto_start
                and self.mission_status in (None, "IDLE")
                and self.state_ticks % int(2 * RATE_HZ) == 1
            ):
                self.override_pub.publish(String(data="START_MISSION"))
                self.get_logger().info("Sent START_MISSION")

    # --- helpers --------------------------------------------------------------

    def following_navigation(self) -> bool:
        return (
            self.velocity_at is not None
            and (self.get_clock().now() - self.velocity_at).nanoseconds * 1e-9
            < self.command_timeout
        )

    def hold_here(self, z: float | None = None):
        p = self.position
        self.hold = (p.x, p.y, p.z if z is None else z, p.heading)

    def px4_ready(self) -> bool:
        # Same check as offboard.py: a position fix is enough, PX4 runs its arming checks itself.
        return (
            self.status is not None
            and self.position is not None
            and self.position.xy_valid
            and self.position.z_valid
        )

    def set_state(self, state: State):
        self.state = state
        self.state_ticks = 0


def main():
    rclpy.init()
    node = CppBridge()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        rclpy.try_shutdown()
