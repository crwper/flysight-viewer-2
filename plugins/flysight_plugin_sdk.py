from __future__ import annotations
import numpy as np
from datetime import datetime, timezone
from dataclasses import dataclass
from typing import List, Union, Optional
# Pull in the C++ bridge for DependencyKey
from flysight_cpp_bridge import DependencyKey


# ─── internal registries ────────────────────────────────────────────────
_attributes:   List[AttributePlugin]   = []
_measurements: List[MeasurementPlugin] = []
_simple_plots: List[SimplePlot]        = []


# ─── helpers to construct DependencyKey ─────────────────────────────────
def meas(sensor: str, name: str) -> DependencyKey:
    k = DependencyKey()
    k.kind            = DependencyKey.Type.Measurement
    k.sensorKey       = sensor
    k.measurementKey  = name
    return k

def attr(name: str) -> DependencyKey:
    k = DependencyKey()
    k.kind           = DependencyKey.Type.Attribute
    k.attributeKey   = name
    return k


# ─── base classes for plug-ins ─────────────────────────────────────────
class AttributePlugin:
    """Return a single QVariant-compatible value or small NumPy array."""
    name:  str
    units: Optional[str] = None

    def inputs(self) -> List[DependencyKey]:
        return []

    def compute(self, session) -> Union[float, str]:
        raise NotImplementedError

def register_attribute(plugin: AttributePlugin) -> None:
    _attributes.append(plugin)

class MeasurementPlugin:
    """Return a full-length NumPy array (one value per sample)."""
    name:   str
    units:  Optional[str] = None
    sensor: str

    def inputs(self) -> List[DependencyKey]:
        return []

    def compute(self, session) -> np.ndarray:
        raise NotImplementedError

def register_measurement(plugin: MeasurementPlugin) -> None:
    _measurements.append(plugin)

@dataclass(frozen=True)
class SimplePlot:
    category:    str
    name:        str
    units:       Optional[str]
    color:       str     # CSS color string, e.g. "#1E88E5"
    sensor:      str
    measurement: str

def register_plot(meta: SimplePlot) -> None:
    _simple_plots.append(meta)


# ─── default plug-ins ──────────────────────────────────────────────────
class DefaultStartTime(AttributePlugin):
    def __init__(self, sensor: str):
        self.name   = "_START_TIME"
        self.sensor = sensor
    def inputs(self):
        return [ meas(self.sensor, "_time") ]
    def compute(self, session):
        times = np.array(session.getMeasurement(self.sensor, "_time"), float)
        if times.size == 0: return None
        t0 = float(times.min())
        dt = datetime.fromtimestamp(t0, tz=timezone.utc)
        return dt.isoformat().replace("+00:00","Z")

class DefaultDuration(AttributePlugin):
    units = "s"
    def __init__(self, sensor: str):
        self.name   = "_DURATION"
        self.sensor = sensor
    def inputs(self):
        return [ meas(self.sensor, "_time") ]
    def compute(self, session):
        times = np.array(session.getMeasurement(self.sensor, "_time"), float)
        if times.size == 0: return None
        dur = float(times.max() - times.min())
        return dur if dur>=0 else None

class DefaultExitTime(AttributePlugin):
    def __init__(self, sensor: str):
        self.name   = "_EXIT_TIME"
        self.sensor = sensor
    def inputs(self):
        return [ meas(self.sensor, "_time") ]
    def compute(self, session):
        times = np.array(session.getMeasurement(self.sensor, "_time"), float)
        if times.size == 0: return None
        t0 = float(times[-1])
        dt = datetime.fromtimestamp(t0, tz=timezone.utc)
        return dt.isoformat().replace("+00:00","Z")

class DefaultTime(MeasurementPlugin):
    units = "s"
    def __init__(self, sensor: str, time: str):
        self.name   = "_time"
        self.sensor = sensor
        self.time   = time
    def inputs(self):
        return [
            meas(self.sensor, self.time),
        ]
    def compute(self, session):
        raw = np.array(session.getMeasurement(self.sensor, self.time), float)
        if raw.size==0: return None
        return raw 

class DefaultTimeFromExit(MeasurementPlugin):
    units = "s"
    def __init__(self, sensor: str):
        self.name   = "_time_from_exit"
        self.sensor = sensor
    def inputs(self):
        return [
            meas(self.sensor, "_time"),
            attr("_EXIT_TIME"),
        ]
    def compute(self, session):
        raw = np.array(session.getMeasurement(self.sensor, "_time"), float)
        exit_iso = session.getAttribute("_EXIT_TIME")
        if raw.size==0 or exit_iso is None: return None
        dt = datetime.fromisoformat(exit_iso.replace("Z","+00:00"))
        exit_ts = dt.timestamp()
        return raw - exit_ts
