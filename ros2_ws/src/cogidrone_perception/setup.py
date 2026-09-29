from setuptools import find_packages, setup

package_name = "cogidrone_perception"

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
    description="YOLO object detection on the drone camera.",
    license="MIT",
    entry_points={
        "console_scripts": [
            "detector = cogidrone_perception.detector:main",
        ],
    },
)
