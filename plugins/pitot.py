import datetime, numpy as np
from flysight_plugin_sdk import AttributePlugin, attr, register_attribute
from flysight_plugin_sdk import MeasurementPlugin, meas, register_measurement
from flysight_plugin_sdk import SimplePlot, register_plot

class PitotExitTime(AttributePlugin):
    name   = "_EXIT_TIME"

    def inputs(self):
        return [
            meas("PITOT", "_time"),
        ]

    def compute(self, session):
        # grab the raw times (list of floats)
        times = np.array(session.getMeasurement("PITOT", "_time"), dtype=float)
        if times.size == 0:
            return None

        # last timestamp
        last_sec = float(times[-1])
        dt = datetime.datetime.utcfromtimestamp(last_sec)
        # return ISO8601 with fractional seconds; C++ will parse this into QDateTime
        return dt.isoformat() + "Z"

register_attribute(PitotExitTime())

class PitotStartTime(AttributePlugin):
    name = "_START_TIME"

    def inputs(self):
        return [ meas("PITOT", "_time") ]

    def compute(self, session):
        # grab the raw times (list of floats)
        times = np.array(session.getMeasurement("PITOT", "_time"), dtype=float)
        if times.size == 0:
            return None

        # earliest timestamp
        start_sec = float(np.min(times))
        dt = datetime.datetime.utcfromtimestamp(start_sec)
        # return ISO8601 with fractional seconds; C++ will parse this into QDateTime
        return dt.isoformat() + "Z"

register_attribute(PitotStartTime())

class PitotDuration(AttributePlugin):
    name  = "_DURATION"
    units = "s"

    def inputs(self):
        return [ meas("PITOT", "_time") ]

    def compute(self, session):
        # grab the raw times (list of floats)
        times = np.array(session.getMeasurement("PITOT", "_time"), dtype=float)
        if times.size == 0:
            return None

        # track duration
        start = float(np.min(times))
        end   = float(np.max(times))
        duration = end - start
        if duration < 0:
            return None

        # return a plain float (QVariant→double)
        return duration

register_attribute(PitotDuration())

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

class PitotTimeFromExit(MeasurementPlugin):
    sensor = "PITOT"
    name   = "_time_from_exit"
    units  = "s"

    def inputs(self):
        return [
            meas(self.sensor, "_time"),
            attr("_EXIT_TIME"),
        ]

    def compute(self, session):
        # get the raw times
        raw = np.array(session.getMeasurement(self.sensor, "_time"), dtype=float)
        if raw.size == 0:
            return None

        # get exit time ISO string
        exit_iso = session.getAttribute("_EXIT_TIME")
        if exit_iso is None:
            return None

        # parse into a timestamp (float seconds since epoch)
        # strip trailing Z, then fromisoformat
        try:
            dt = datetime.datetime.fromisoformat(exit_iso.rstrip("Z"))
            exit_ts = dt.timestamp()
        except ValueError:
            return None

        # return the element-wise difference
        return raw - exit_ts

register_measurement(PitotTimeFromExit())

register_plot(SimplePlot(
    category    = "Pitot tube",
    name        = "Air pressure",
    units       = "Pa",
    color       = "#1E88E5",
    sensor      = "PITOT",
    measurement = "pressure",
))
