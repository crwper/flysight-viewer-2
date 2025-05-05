from flysight_plugin_sdk import register_attribute
from flysight_plugin_sdk import register_measurement
from flysight_plugin_sdk import SimplePlot, register_plot
from flysight_plugin_sdk import DefaultStartTime, DefaultDuration
from flysight_plugin_sdk import DefaultExitTime
from flysight_plugin_sdk import DefaultTime, DefaultTimeFromExit

for s in ["PITOT"]:
    register_attribute(DefaultStartTime(s))
    register_attribute(DefaultDuration(s))
    register_attribute(DefaultExitTime(s))
    register_measurement(DefaultTime(s, "time"))
    register_measurement(DefaultTimeFromExit(s))

register_plot(SimplePlot(
    category    = "Pitot tube",
    name        = "Air pressure",
    units       = "Pa",
    color       = "#1E88E5",
    sensor      = "PITOT",
    measurement = "pressure",
))
