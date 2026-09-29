from setuptools import find_packages, setup

package_name = "cogidrone_control"

setup(
    name=package_name,
    version="0.0.1",
    packages=find_packages(exclude=["test"]),
    data_files=[
        ("share/ament_index/resource_index/packages", ["resource/" + package_name]),
        ("share/" + package_name, ["package.xml"]),
    ],
    install_requires=["setuptools"],
    zip_safe=True,
    maintainer="CogiDrone team",
    maintainer_email="cogidrone@example.com",
    description="PX4 offboard controller: takeoff, then turn to face the detected target.",
    license="MIT",
    entry_points={
        "console_scripts": [
            "offboard = cogidrone_control.offboard:main",
            "teleop = cogidrone_control.teleop:main",
        ],
    },
)
