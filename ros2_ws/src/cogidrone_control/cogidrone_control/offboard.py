"""Offboard controller: take off, then turn to face the detected target.

State machine:
    WAIT_FOR_PX4 -> ARMING -> TAKEOFF -> TRACK -> (LAND if land_after_s > 0)

TRACK holds position and altitude. If a target is seen it yaws to center it in the
image, otherwise it slowly spins to search. PX4 stays in charge of stabilization
and failsafes; this node only sends position + yaw setpoints.

Talks to PX4 over uXRCE-DDS (px4_msgs). Frames are PX4's local NED: z is negative up.
"""

import math
from enum import Enum, auto

import rclpy
from px4_msgs.msg import (
    OffboardControlMode,
    TrajectorySetpoint,
    VehicleCommand,
    VehicleLocalPosition,
    VehicleStatus,
)
from rclpy.node import Node
from rclpy.qos import DurabilityPolicy, HistoryPolicy, QoSProfile, ReliabilityPolicy
from vision_msgs.msg import Detection2DArray

RATE_HZ = 10.0

# PX4 publishes best-effort; our subscriptions must match or nothing arrives.
PX4_QOS = QoSProfile(
    reliability=ReliabilityPolicy.BEST_EFFORT,
    durability=DurabilityPolicy.TRANSIENT_LOCAL,
    history=HistoryPolicy.KEEP_LAST,
    depth=1,
)


class State(Enum):
    WAIT_FOR_PX4 = auto()
    ARMING = auto()
    TAKEOFF = auto()
    TRACK = auto()
    LAND = auto()


def wrap_pi(angle: float) -> float:
    return math.atan2(math.sin(angle), math.cos(angle))


class Offboard(Node):
    def __init__(self):
        super().__init__("offboard")
        self.declare_parameter("altitude", 3.0)  # m above takeoff point
        self.declare_parameter("target_class", "person")
        self.declare_parameter("image_width", 640)  # must match the camera
        self.declare_parameter("hfov_deg", 69.0)  # D435i RGB
        # Fraction of the bearing error corrected per tick.
        self.declare_parameter("yaw_gain", 0.5)
        # Spin speed when nothing is seen.
        self.declare_parameter("search_rate_deg", 20.0)
        self.declare_parameter("lost_timeout_s", 1.0)
        self.declare_parameter("land_after_s", 0.0)  # 0 = hover/track until Ctrl+C
        # PX4 >= 1.16 appends _vN to versioned message topics.
        self.declare_parameter("status_topic", "/fmu/out/vehicle_status_v1")
        self.declare_parameter(
            "local_position_topic", "/fmu/out/vehicle_local_position_v1"
        )

        p = lambda name: self.get_parameter(name).value
        self.altitude = p("altitude")
        self.target_class = p("target_class")
        self.image_width = p("image_width")
        self.focal_px = (self.image_width / 2) / math.tan(
            math.radians(p("hfov_deg")) / 2
        )
        self.yaw_gain = p("yaw_gain")
        self.search_rate = math.radians(p("search_rate_deg"))
        self.lost_timeout = p("lost_timeout_s")
        self.land_after = p("land_after_s")

        self.mode_pub = self.create_publisher(
            OffboardControlMode, "/fmu/in/offboard_control_mode", PX4_QOS
        )
        self.setpoint_pub = self.create_publisher(
            TrajectorySetpoint, "/fmu/in/trajectory_setpoint", PX4_QOS
        )
        self.command_pub = self.create_publisher(
            VehicleCommand, "/fmu/in/vehicle_command", PX4_QOS
        )
        self.create_subscription(
            VehicleStatus, p("status_topic"), self.on_status, PX4_QOS
        )
        self.create_subscription(
            VehicleLocalPosition, p("local_position_topic"), self.on_position, PX4_QOS
        )
        self.create_subscription(
            Detection2DArray, "/cogidrone/detections", self.on_detections, 10
        )

        self.status: VehicleStatus | None = None
        self.position: VehicleLocalPosition | None = None
        self.state = State.WAIT_FOR_PX4
        self.state_ticks = 0
        self.hold_xy = (0.0, 0.0)
        self.hold_z = 0.0
        self.yaw_sp = 0.0
        self.target_bearing: float | None = None
        self.target_seen_at = None

        self.create_timer(1.0 / RATE_HZ, self.tick)
        self.get_logger().info("Waiting for PX4 (is the XRCE agent running?)...")

    # --- inputs -------------------------------------------------------------

    def on_status(self, msg: VehicleStatus):
        self.status = msg

    def on_position(self, msg: VehicleLocalPosition):
        self.position = msg

    def on_detections(self, msg: Detection2DArray):
        best = None
        for det in msg.detections:
            for hyp in det.results:
                if hyp.hypothesis.class_id == self.target_class and (
                    best is None or hyp.hypothesis.score > best[1]
                ):
                    best = (det.bbox.center.position.x, hyp.hypothesis.score)
        if best is None:
            return
        # Pixel offset from image center -> bearing (rad). Right of center = positive = clockwise in NED.
        self.target_bearing = math.atan(
            (best[0] - self.image_width / 2) / self.focal_px
        )
        self.target_seen_at = self.get_clock().now()

    # --- main loop ------------------------------------------------------------

    def tick(self):
        self.state_ticks += 1

        if self.state == State.WAIT_FOR_PX4:
            if self.px4_ready():
                self.hold_xy = (self.position.x, self.position.y)
                self.hold_z = self.position.z - self.altitude
                self.yaw_sp = self.position.heading
                self.get_logger().info(
                    "PX4 ready, streaming setpoints before switching to offboard"
                )
                self.set_state(State.ARMING)
            return

        if self.state == State.LAND:
            return  # PX4 lands on its own; we stop streaming setpoints.

        # PX4 needs a steady stream of these to enter and stay in offboard mode.
        self.publish_offboard_mode()
        self.publish_setpoint(
            self.hold_xy[0], self.hold_xy[1], self.hold_z, self.yaw_sp
        )

        if self.state == State.ARMING:
            # Stream 1 s of setpoints first, then (re)request offboard + arm every 2 s until it sticks.
            if (
                self.state_ticks >= RATE_HZ
                and self.state_ticks % int(2 * RATE_HZ) == RATE_HZ
            ):
                self.command(
                    VehicleCommand.VEHICLE_CMD_DO_SET_MODE, param1=1.0, param2=6.0
                )  # 6 = offboard
                self.command(
                    VehicleCommand.VEHICLE_CMD_COMPONENT_ARM_DISARM, param1=1.0
                )
                self.get_logger().info("Requesting offboard mode + arm")
            if self.is_armed_offboard():
                self.get_logger().info(
                    f"Armed in offboard, taking off to {self.altitude:.1f} m"
                )
                self.set_state(State.TAKEOFF)

        elif self.state == State.TAKEOFF:
            if abs(self.position.z - self.hold_z) < 0.3:
                self.get_logger().info(f"At altitude, tracking '{self.target_class}'")
                self.set_state(State.TRACK)

        elif self.state == State.TRACK:
            self.update_yaw()
            if self.land_after > 0 and self.state_ticks > self.land_after * RATE_HZ:
                self.get_logger().info("Time is up, landing")
                self.command(VehicleCommand.VEHICLE_CMD_NAV_LAND)
                self.set_state(State.LAND)

    def update_yaw(self):
        dt = 1.0 / RATE_HZ
        seen = (
            self.target_seen_at is not None
            and (self.get_clock().now() - self.target_seen_at).nanoseconds * 1e-9
            < self.lost_timeout
        )
        if seen:
            self.yaw_sp = wrap_pi(
                self.position.heading + self.yaw_gain * self.target_bearing
            )
        else:
            self.yaw_sp = wrap_pi(self.yaw_sp + self.search_rate * dt)

    # --- helpers --------------------------------------------------------------

    def px4_ready(self) -> bool:
        return (
            self.status is not None
            and self.position is not None
            and self.status.pre_flight_checks_pass
            and self.position.xy_valid
            and self.position.z_valid
        )

    def is_armed_offboard(self) -> bool:
        return (
            self.status.arming_state == VehicleStatus.ARMING_STATE_ARMED
            and self.status.nav_state == VehicleStatus.NAVIGATION_STATE_OFFBOARD
        )

    def set_state(self, state: State):
        self.state = state
        self.state_ticks = 0

    def timestamp_us(self) -> int:
        return self.get_clock().now().nanoseconds // 1000

    def publish_offboard_mode(self):
        msg = OffboardControlMode()
        msg.position = True
        msg.timestamp = self.timestamp_us()
        self.mode_pub.publish(msg)

    def publish_setpoint(self, x: float, y: float, z: float, yaw: float):
        msg = TrajectorySetpoint()
        msg.position = [float(x), float(y), float(z)]
        msg.yaw = float(yaw)
        msg.timestamp = self.timestamp_us()
        self.setpoint_pub.publish(msg)

    def command(self, command: int, **params: float):
        msg = VehicleCommand()
        msg.command = command
        for i in range(1, 8):
            setattr(msg, f"param{i}", params.get(f"param{i}", 0.0))
        msg.target_system = 1
        msg.target_component = 1
        msg.source_system = 1
        msg.source_component = 1
        msg.from_external = True
        msg.timestamp = self.timestamp_us()
        self.command_pub.publish(msg)


def main():
    rclpy.init()
    node = Offboard()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        rclpy.try_shutdown()
