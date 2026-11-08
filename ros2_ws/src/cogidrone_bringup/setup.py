from glob import glob

from setuptools import find_packages, setup

package_name = "cogidrone_bringup"

setup(
    name=package_name,
    version="0.0.1",
    packages=find_packages(exclude=["test"]),
    data_files=[
        ("share/ament_index/resource_index/packages", ["resource/" + package_name]),
        ("share/" + package_name, ["package.xml"]),
        ("share/" + package_name + "/launch", glob("launch/*.launch.py")),
        ("share/" + package_name + "/config", glob("config/*.yaml")),
    ],
    install_requires=["setuptools"],
    zip_safe=True,
    maintainer="CogiDrone team",
    maintainer_email="cogidrone@example.com",
    description="Launch files and config for the CogiDrone simulation.",
    license="MIT",
)
