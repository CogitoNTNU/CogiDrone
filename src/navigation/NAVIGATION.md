### Navigation
> [!IMPORTANT]
> this segment should be skipped if we have received manual flight instructions fromm the physical controller

Answer:
    **"Given where I am and what's around me, where should I go next?"**

This includes:
- mission logic
- path planning
- obstacle avoidance
- choosing the next waypoint/trajectory

**Today:** `Navigation` reads `/estimation/vio_odometry` and `/perception/detections`, runs the
mission state machine (`IDLE → SEARCHING → PERSON_DETECTED → FOLLOWING`, `LANDING`, `EMERGENCY`)
and publishes a world-frame ENU velocity on `/navigation/velocity_setpoint`. It starts in `IDLE`
and waits for `START_MISSION` on `/manual_override`. See [docs/architecture.md](../../docs/architecture.md).
