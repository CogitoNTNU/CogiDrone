> [!NOTE]
> `control` has been repurposed to `controllers` to group external controllers together. Their purpouse is to be the final program output

### Control
Thin wrapper to communicate with the flightcontroller

**Today:** `Pixhawk` (`pixhawk.h/.cpp`) is the only link to PX4, over uXRCE-DDS (`/fmu/in/*`,
`/fmu/out/*`). It is the only code that knows about NED; everything else is ENU. It:

- publishes PX4's EKF2 estimate as `/estimation/vio_odometry` (stand-in until Estimation exists)
- turns Navigation's `/navigation/velocity_setpoint` into PX4 offboard velocity setpoints
- arms, switches to offboard, takes off to `target_height`, sends `START_MISSION`, and lands when
  Navigation reports `LANDING`

State machine, topics and frames: [docs/architecture.md](../../docs/architecture.md).
