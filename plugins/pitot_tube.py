import numpy as np
from flysight_plugin_sdk import register_attribute
from flysight_plugin_sdk import register_measurement
from flysight_plugin_sdk import SimplePlot, register_plot
from flysight_plugin_sdk import DefaultStartTime, DefaultDuration
from flysight_plugin_sdk import DefaultExitTime
from flysight_plugin_sdk import DefaultTime, DefaultTimeFromExit

for s in ["PITOT_TUBE"]:
    register_attribute(DefaultStartTime(s))
    register_attribute(DefaultDuration(s))
    register_attribute(DefaultExitTime(s))
    register_measurement(DefaultTime(s, "IsoTime"))
    register_measurement(DefaultTimeFromExit(s))

register_plot(SimplePlot(
    category    = "Pitot tube",
    name        = "Vertical speed",
    units       = "m/s",
    color       = "#1c39bb",
    sensor      = "PITOT_TUBE",
    measurement = "Smoothed_Vertical_baro",
))

register_plot(SimplePlot(
    category    = "Pitot tube",
    name        = "Pitot speed",
    units       = "m/s",
    color       = "#e1ad01",
    sensor      = "PITOT_TUBE",
    measurement = "Smoothed_Pitot_Speed",
))

register_plot(SimplePlot(
    category    = "Pitot tube",
    name        = "Lift coefficient",
    units       = None,
    color       = "#007a5e",
    sensor      = "PITOT_TUBE",
    measurement = "Lift_Coeff",
))

register_plot(SimplePlot(
    category    = "Pitot tube",
    name        = "Drag coefficient",
    units       = None,
    color       = "#9b111e",
    sensor      = "PITOT_TUBE",
    measurement = "Drag_Coeff",
))
