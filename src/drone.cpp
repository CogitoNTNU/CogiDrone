#include "drone.h"
#include "link/px4_dds_link.h"

#include <rclcpp/rclcpp.hpp>

#include <utility>


// * Ctor & dtor
Drone::Drone(M&& m) : m(std::move(m)) {}
Drone::~Drone() {
    // Shutdown ROS context if it was initialized
    // ? Guarded by rclcpp::ok(), which is also what makes a fake-injected Drone
    // ? safe to destroy in a process where rclcpp::init() was never called.
    if (rclcpp::ok()) {
        rclcpp::shutdown();
    }
}

std::expected<std::unique_ptr<Drone>, Drone::InitError>
Drone::initiate(int argc, const char* argv[]) {
    // 1. Initialize ROS global state, so subsystems can create nodes
    if (!rclcpp::ok()) {
        rclcpp::init(argc, argv);
    }

    // 2. Subsystems (each may create rclcpp::Node objects)
    // auto perception = Perception::create();
    // if (!perception) return std::unexpected(InitError::CameraNotFound);
    Perception perception;                                                              // | Debug as Perception is missing implementation

    // The PX4 link. ? Px4DdsLink reports its own LinkError rather than depending
    // ? on this header - see the comment on Px4DdsLink::LinkError - so map it here.
    auto link = Px4DdsLink::create();
    if (!link) return std::unexpected(InitError::LinkFailed);
    auto node = (*link)->node();                                                        // keep the node to spin; link keeps owning it

    // 3. Assemble
    // ? We wrap the raw pointer and let a unique_ptr take ownership to prevent leaks
    // ? caused by not deleting the Drone object. Note that the wrapping is simply
    // ? here because make_unique<Drone> is unavailable as the constructor is private
    return std::unique_ptr<Drone>(
        new Drone(M{                                                                    // ? We must return a Drone pointer, as the Drone
            .perception = std::move(perception),                                        // ? object shouldn't be copyable nor movable, so
            .link = std::move(*link),                                                   // ? we can't return a Drone object by value
            .node = std::move(node),
        })
    );
}

std::expected<std::unique_ptr<Drone>, Drone::InitError>
Drone::initiate(std::unique_ptr<IVehicleLink> link) {
    // ! No rclcpp::init() here, on purpose - see the declaration in drone.h.
    if (!link) return std::unexpected(InitError::LinkFailed);

    Perception perception;                                                              // | Debug as Perception is missing implementation

    return std::unique_ptr<Drone>(
        new Drone(M{
            .perception = std::move(perception),
            .link = std::move(link),
            .node = nullptr,                                                            // nothing to spin: the link is not a ROS node
        })
    );
}



// * Public interface
void Drone::start() {
    // TODO: Ensure that this is the correct place and design choice for starting the Drone object
    // ? Nothing to spin when a fake link was injected - the fake has no executor,
    // ? no callbacks and no ROS context, so start() is a no-op in that case.
    if (!m.node) {
        return;
    }

    // Start the ROS event loop, which will run until rclcpp::shutdown() is called.
    // ! This blocks. Subscription callbacks on the link only fire while it runs, so
    // ! until something spins, odometry() and state() never change.
    // TODO: The offboard heartbeat still has no driver. IVehicleLink::tick() must be
    //     : called at >2 Hz for PX4 to accept offboard control at all - that belongs
    //     : to the `flight_manager` component described in src/NOTES.md, via a timer
    //     : on this node. Until then the link can read telemetry but not fly.
    rclcpp::spin(m.node);
}

void Drone::stop() {}
