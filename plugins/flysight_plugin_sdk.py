from __future__ import annotations
import numpy as np
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
