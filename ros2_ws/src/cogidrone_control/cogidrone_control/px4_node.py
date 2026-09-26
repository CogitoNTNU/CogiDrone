"""Shared PX4 plumbing for our control nodes (uXRCE-DDS topics via px4_msgs).

Frames are PX4's local NED: x north, y east, z down (negative is up).
"""

import math

from px4_msgs.msg import (
    OffboardControlMode,
    TrajectorySetpoint,
    VehicleCommand,
    VehicleLocalPosition,
    VehicleStatus,
)
from rclpy.node import Node
from rclpy.qos import DurabilityPolicy, HistoryPolicy, QoSProfile, ReliabilityPolicy

RATE_HZ = 10.0

# PX4 publishes best-effort; our subscriptions must match or nothing arrives.
PX4_QOS = QoSProfile(
    reliability=ReliabilityPolicy.BEST_EFFORT,
    durability=DurabilityPolicy.TRANSIENT_LOCAL,
    history=HistoryPolicy.KEEP_LAST,
    depth=1,
)


def wrap_pi(angle: float) -> float:
    return math.atan2(math.sin(angle), math.cos(angle))


class Px4Node(Node):
    """A node that reads PX4 status/position and can send offboard setpoints + commands."""

    def __init__(self, name: str):
        super().__init__(name)
        # PX4 >= 1.16 appends _vN to versioned message topics.
        self.declare_parameter("status_topic", "/fmu/out/vehicle_status_v1")
        self.declare_parameter(
            "local_position_topic", "/fmu/out/vehicle_local_position_v1"
        )

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
            VehicleStatus,
            self.get_parameter("status_topic").value,
            self.on_status,
            PX4_QOS,
        )
        self.create_subscription(
            VehicleLocalPosition,
            self.get_parameter("local_position_topic").value,
            self.on_position,
            PX4_QOS,
        )
        self.status: VehicleStatus | None = None
        self.position: VehicleLocalPosition | None = None

    def on_status(self, msg: VehicleStatus):
        self.status = msg

    def on_position(self, msg: VehicleLocalPosition):
        self.position = msg

    def is_armed(self) -> bool:
        return (
            self.status is not None
            and self.status.arming_state == VehicleStatus.ARMING_STATE_ARMED
        )

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

    def arm_offboard(self):
        self.command(VehicleCommand.VEHICLE_CMD_DO_SET_MODE, param1=1.0, param2=6.0)
        self.command(VehicleCommand.VEHICLE_CMD_COMPONENT_ARM_DISARM, param1=1.0)

    def land(self):
        self.command(VehicleCommand.VEHICLE_CMD_NAV_LAND)
