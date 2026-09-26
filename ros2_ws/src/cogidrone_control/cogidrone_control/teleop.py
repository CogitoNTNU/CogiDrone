"""Fly the sim drone from the keyboard (PX4 offboard, position control).

    ros2 run cogidrone_control teleop

Each key press moves the target position one step; PX4 flies there and holds.
Run the sim with autonomy:=false so the autonomous controller doesn't fight you.
"""

import math
import select
import sys
import termios
import tty

import rclpy

from cogidrone_control.px4_node import RATE_HZ, Px4Node, wrap_pi

HELP = """
  CogiDrone keyboard control
  --------------------------
  t : take off (arm + offboard, climb to 2.5 m)
  w/s : forward/back     a/d : left/right
  r/f : up/down          q/e : turn left/right
  space : stop and hold here
  l : land               x : quit
"""
STEP_M = 0.5
YAW_STEP = math.radians(15)
TAKEOFF_M = 2.5


class Teleop(Px4Node):
    def __init__(self):
        super().__init__("teleop")
        self.flying = False
        self.sp = [0.0, 0.0, 0.0]
        self.yaw_sp = 0.0
        self.create_timer(1.0 / RATE_HZ, self.tick)

    def tick(self):
        # PX4 needs a steady setpoint stream to enter and stay in offboard mode.
        if self.flying:
            self.publish_offboard_mode()
            self.publish_setpoint(*self.sp, self.yaw_sp)

    def hold_here(self):
        p = self.position
        self.sp = [p.x, p.y, p.z]
        self.yaw_sp = p.heading

    def on_key(self, k: str) -> str:
        if self.position is None or self.status is None:
            return "waiting for PX4..."
        p = self.position
        if k == "t":
            self.hold_here()
            self.sp[2] = p.z - TAKEOFF_M
            self.flying = True
            self.arm_offboard()
        elif k == "l":
            self.flying = False
            self.land()
        elif not self.flying:
            return "press t to take off first"
        elif k == " ":
            self.hold_here()
        else:
            fwd = {"w": 1, "s": -1}.get(k, 0) * STEP_M
            right = {"d": 1, "a": -1}.get(k, 0) * STEP_M
            # Body frame -> NED using the target heading.
            c, s = math.cos(self.yaw_sp), math.sin(self.yaw_sp)
            self.sp[0] += fwd * c - right * s
            self.sp[1] += fwd * s + right * c
            self.sp[2] += {"r": -STEP_M, "f": STEP_M}.get(k, 0.0)
            self.yaw_sp = wrap_pi(
                self.yaw_sp + {"e": YAW_STEP, "q": -YAW_STEP}.get(k, 0.0)
            )
        return (
            f"{'ARMED' if self.is_armed() else 'disarmed'}  "
            f"alt {-p.z:4.1f} m -> {-self.sp[2]:4.1f} m  "
            f"heading {math.degrees(p.heading):4.0f} deg"
        )


def main():
    if not sys.stdin.isatty():
        print("teleop needs an interactive terminal (use ./scripts/sim.sh shell)")
        return
    rclpy.init()
    node = Teleop()
    print(HELP)
    old = termios.tcgetattr(sys.stdin)
    try:
        tty.setcbreak(sys.stdin.fileno())
        while rclpy.ok():
            rclpy.spin_once(node, timeout_sec=0.02)
            if select.select([sys.stdin], [], [], 0)[0]:
                k = sys.stdin.read(1).lower()
                if k == "x":
                    break
                print("\r" + node.on_key(k).ljust(70), end="", flush=True)
    except KeyboardInterrupt:
        pass
    finally:
        termios.tcsetattr(sys.stdin, termios.TCSADRAIN, old)
        print()
        node.destroy_node()
        rclpy.try_shutdown()
