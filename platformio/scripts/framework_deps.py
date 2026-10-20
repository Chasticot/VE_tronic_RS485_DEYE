"""Expose ESP32 framework headers required indirectly by old PageBuilder."""

from os.path import join

Import("env")

framework_dir = env.PioPlatform().get_package_dir("framework-arduinoespressif32")
if framework_dir:
    env.Append(
        CPPPATH=[
            join(framework_dir, "libraries", "FS", "src"),
            join(framework_dir, "libraries", "SPIFFS", "src"),
        ]
    )
