import numpy as np
from datetime import datetime, timezone
from flysight_plugin_sdk import AttributePlugin, attr, register_attribute
from flysight_plugin_sdk import MeasurementPlugin, meas, register_measurement
from flysight_plugin_sdk import SimplePlot, register_plot
from flysight_plugin_sdk import DefaultStartTime, DefaultDuration, DefaultExitTime, DefaultTimeFromExit

for s in ["PITOT"]:
    register_attribute(DefaultStartTime(s))
    register_attribute(DefaultDuration(s))
    register_attribute(DefaultExitTime(s))
    register_measurement(DefaultTimeFromExit(s))

class PitotTime(MeasurementPlugin):
    sensor = "PITOT"
    name   = "_time"
    units  = "s"

    def inputs(self):
        # declare that we depend on the raw PITOT/time measurement
        return [ meas(self.sensor, "time") ]

    def compute(self, session):
        # getMeasurement returns a Python list of floats
        data = session.getMeasurement(self.sensor, "time")
        if not data:
            # no data => no calculated value
            return None
        # return as a numpy array of floats
        return np.array(data, dtype=float)

register_measurement(PitotTime())

register_plot(SimplePlot(
    category    = "Pitot tube",
    name        = "Air pressure",
    units       = "Pa",
    color       = "#1E88E5",
    sensor      = "PITOT",
    measurement = "pressure",
))
