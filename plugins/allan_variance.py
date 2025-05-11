import numpy as np
import math
import os # For environment variable
from typing import Optional, Dict, Tuple

# --- Debug Flag ---
# Read from environment variable, defaulting to False if not set or invalid
FLYSIGHT_DEBUG_ALLAN_STR = os.environ.get("FLYSIGHT_DEBUG_ALLAN", "False")
DEBUG_ALLAN = FLYSIGHT_DEBUG_ALLAN_STR.lower() in ("true", "1", "t", "yes")

if DEBUG_ALLAN:
    print("[allan_variance_plugin.py] DEBUG_ALLAN is ON")

# Attempt to import allantools
try:
    import allantools
    if DEBUG_ALLAN:
        print(f"[allan_variance_plugin.py] allantools library version: {allantools.__version__}")
except ImportError:
    print(
        "[allan_variance_plugin.py] WARNING: allantools library not found. "
        "Allan deviation calculations will not be available. "
        "Please install it (e.g., 'pip install allantools')."
    )
    allantools = None # So we can check its existence later

from flysight_plugin_sdk import (
    MeasurementPlugin,
    register_measurement,
    SimplePlot,
    register_plot,
    meas
)

# --- Configuration (largely the same) ---
ALLAN_SENSOR_KEY = "ALLAN_ADEV"
IMU_SENSOR_KEY = "IMU"
IMU_TIME_KEY = "_time" # Make sure this matches the actual key in SessionData for IMU timestamps

ACCEL_UNIT_CONVERSION: Dict[str, float] = {
    "ax": 9.80665, "ay": 9.80665, "az": 9.80665,
}
GYRO_UNIT_CONVERSION: Dict[str, float] = {
    "wx": math.pi / 180.0, "wy": math.pi / 180.0, "wz": math.pi / 180.0,
}
MIN_DATA_SAMPLES_FOR_AVAR = 2000

# Cache for _compute_allan_data to avoid redundant heavy computation
# Key: (session_id_str, imu_input_key_str), Value: (taus_array, adevs_array)
_internal_adev_cache: Dict[Tuple[str, str], Tuple[np.ndarray, np.ndarray]] = {}

def _clear_internal_adev_cache():
    """ Call this if sessions are reloaded or data fundamentally changes. """
    global _internal_adev_cache
    if DEBUG_ALLAN:
        print("[allan_variance_plugin.py] Clearing internal ADEV cache.")
    _internal_adev_cache.clear()

# --- Base Plugin for ADEV Calculation using allantools ---
class AllanDeviationAxis(MeasurementPlugin):
    sensor = ALLAN_SENSOR_KEY
    # ... (init remains similar) ...
    def __init__(self, imu_input_key: str, output_measurement_key_base: str,
                 unit_conversion_factor: float, output_adev_units_str: str):
        self.imu_input_key = imu_input_key
        self.output_key_base = output_measurement_key_base
        self.unit_conversion = unit_conversion_factor
        self.output_units = output_adev_units_str
        self.name = f"{self.output_key_base}_y"
        self.units = self.output_units
        if DEBUG_ALLAN:
            print(f"[allan_variance_plugin.py] AllanDeviationAxis initialized for {self.name} (IMU key: {self.imu_input_key})")


    def inputs(self):
        return [
            meas(IMU_SENSOR_KEY, self.imu_input_key),
            meas(IMU_SENSOR_KEY, IMU_TIME_KEY),
        ]

    def _compute_allan_data(self, session) -> Optional[Tuple[np.ndarray, np.ndarray]]:
        if not allantools:
            if DEBUG_ALLAN:
                print(f"[allan_variance_plugin.py] _compute_allan_data for {self.imu_input_key}: allantools not available.")
            return None

        session_id_qvariant = session.getAttribute("SESSION_ID")
        session_id_str = str(session_id_qvariant) if session_id_qvariant is not None else "unknown_session"

        cache_key = (session_id_str, self.imu_input_key)
        if cache_key in _internal_adev_cache:
            if DEBUG_ALLAN:
                print(f"[allan_variance_plugin.py] Returning cached ADEV data for {self.imu_input_key} in session {session_id_str}")
            return _internal_adev_cache[cache_key]

        if DEBUG_ALLAN:
            print(f"[allan_variance_plugin.py] _compute_allan_data for {self.imu_input_key} (Session: {session_id_str})")

        imu_raw_axis_data = session.getMeasurement(IMU_SENSOR_KEY, self.imu_input_key)
        imu_time_data = session.getMeasurement(IMU_SENSOR_KEY, IMU_TIME_KEY)

        if imu_raw_axis_data is None or imu_time_data is None:
            if DEBUG_ALLAN:
                print(f"[allan_variance_plugin.py] Missing raw IMU data or time data for {self.imu_input_key}.")
            return None

        data_axis_raw = np.array(imu_raw_axis_data, dtype=float)
        time_axis = np.array(imu_time_data, dtype=float)

        if DEBUG_ALLAN:
            print(f"[allan_variance_plugin.py]   Raw data samples: {data_axis_raw.size}, Time samples: {time_axis.size}")

        if data_axis_raw.size < MIN_DATA_SAMPLES_FOR_AVAR or time_axis.size != data_axis_raw.size:
            if DEBUG_ALLAN:
                print(f"[allan_variance_plugin.py]   Insufficient data or mismatched array sizes.")
            return None
        
        data_axis_converted = data_axis_raw * self.unit_conversion

        if len(time_axis) < 2:
            if DEBUG_ALLAN:
                print(f"[allan_variance_plugin.py]   Not enough time samples to calculate rate (<2).")
            return None
            
        time_span = time_axis[-1] - time_axis[0]
        if time_span == 0:
            if DEBUG_ALLAN:
                print(f"[allan_variance_plugin.py]   Time span is zero, cannot calculate rate.")
            return None
            
        rate = float(len(time_axis) -1) / time_span # More robust rate calc
        if rate <= 0:
            if DEBUG_ALLAN:
                print(f"[allan_variance_plugin.py]   Calculated rate is <= 0 ({rate}). Cannot compute ADEV.")
            return None
        
        if DEBUG_ALLAN:
            print(f"[allan_variance_plugin.py]   Calculated sample rate: {rate:.2f} Hz")
            print(f"[allan_variance_plugin.py]   Unit conversion factor: {self.unit_conversion}")
            if data_axis_converted.size > 0:
                print(f"[allan_variance_plugin.py]   First 3 converted data points: {data_axis_converted[:3]}")


        try:
            if DEBUG_ALLAN:
                print(f"[allan_variance_plugin.py]   Calling allantools.adev for {self.imu_input_key}...")
            (taus_out, adevs_out, adev_errors, ns) = allantools.adev(
                data_axis_converted,
                rate=rate,
                data_type="freq", 
                taus="octave"
            )
            if DEBUG_ALLAN:
                print(f"[allan_variance_plugin.py]   allantools.adev successful for {self.imu_input_key}.")
                print(f"[allan_variance_plugin.py]   taus_out (len {len(taus_out)}): {taus_out[:5]}...")
                print(f"[allan_variance_plugin.py]   adevs_out (len {len(adevs_out)}): {adevs_out[:5]}...")
            
            computed_result = (taus_out, adevs_out)
            _internal_adev_cache[cache_key] = computed_result # Cache the result
            return computed_result
            
        except Exception as e:
            print(f"[allan_variance_plugin.py] Error during allantools.adev for {self.imu_input_key}: {e}")
            if DEBUG_ALLAN:
                import traceback
                traceback.print_exc() # This will go to the C++ redirected stderr (qDebug)
            return None

    def compute(self, session) -> Optional[np.ndarray]:
        # This plugin instance is for ADEV values (_y)
        computed_data = self._compute_allan_data(session)
        if computed_data:
            _taus, adevs = computed_data
            return adevs
        return None

class AllanTauAxis(MeasurementPlugin):
    sensor = ALLAN_SENSOR_KEY
    units = "s"

    def __init__(self, imu_input_key_for_cache_logic: str, output_tau_key_base: str):
        self.imu_input_key_for_cache = imu_input_key_for_cache_logic
        self.name = f"{output_tau_key_base}_x"
        # This instance is only used to call _compute_allan_data.
        # It shares the same imu_input_key, so the caching in _compute_allan_data will be effective.
        self._adev_computer = AllanDeviationAxis(imu_input_key_for_cache_logic, "", 1.0, "")
        if DEBUG_ALLAN:
            print(f"[allan_variance_plugin.py] AllanTauAxis initialized for {self.name} (IMU key: {self.imu_input_key_for_cache})")

    def inputs(self):
        return [
            meas(IMU_SENSOR_KEY, self._adev_computer.imu_input_key),
            meas(IMU_SENSOR_KEY, IMU_TIME_KEY),
        ]

    def compute(self, session) -> Optional[np.ndarray]:
        computed_data = self._adev_computer._compute_allan_data(session) # This will use the cache
        if computed_data:
            taus, _adevs = computed_data
            return taus
        return None

# --- Register ADEV plugins and plots ---
# ... (IMU_AXES_CONFIG remains the same) ...
IMU_AXES_CONFIG = {
    "ax": ("Accel X", ACCEL_UNIT_CONVERSION.get("ax", 1.0), "m/s²", "#FF6347"),
    "ay": ("Accel Y", ACCEL_UNIT_CONVERSION.get("ay", 1.0), "m/s²", "#32CD32"),
    "az": ("Accel Z", ACCEL_UNIT_CONVERSION.get("az", 1.0), "m/s²", "#1E90FF"),
    "wx": ("Gyro X", GYRO_UNIT_CONVERSION.get("wx", 1.0), "rad/s", "#FFD700"),
    "wy": ("Gyro Y", GYRO_UNIT_CONVERSION.get("wy", 1.0), "rad/s", "#DA70D6"),
    "wz": ("Gyro Z", GYRO_UNIT_CONVERSION.get("wz", 1.0), "rad/s", "#00CED1"),
}

if allantools:
    for imu_key, (plot_name_suffix, conv_factor, adev_units, plot_color) in IMU_AXES_CONFIG.items():
        output_key_base = f"adev_{imu_key}"

        adev_plugin = AllanDeviationAxis(
            imu_input_key=imu_key,
            output_measurement_key_base=output_key_base,
            unit_conversion_factor=conv_factor,
            output_adev_units_str=adev_units
        )
        register_measurement(adev_plugin)

        tau_plugin = AllanTauAxis(
            imu_input_key_for_cache_logic=imu_key,
            output_tau_key_base=output_key_base
        )
        register_measurement(tau_plugin)
        
        category_group = "Accelerometer" if "a" in imu_key else "Gyroscope"
        register_plot(SimplePlot(
            category=f"Allan Deviation ({category_group})",
            name=f"ADEV {plot_name_suffix}",
            units=adev_units,
            color=plot_color,
            sensor=ALLAN_SENSOR_KEY,
            measurement=f"{output_key_base}_y"
            # The C++ side needs to handle using "{output_key_base}_x" from ALLAN_SENSOR_KEY for the X-axis values
            # This implies your plotting mechanism can source X and Y from different measurement keys.
        ))
    print(f"[allan_variance_plugin.py] Registered Allan Deviation plugins (using allantools) and plots for {ALLAN_SENSOR_KEY}.")
    if DEBUG_ALLAN:
        print(f"[allan_variance_plugin.py] To see debug output from this script, ensure FLYSIGHT_DEBUG_ALLAN environment variable is set to 'true'.")

else:
    print("[allan_variance_plugin.py] allantools not available. Skipping ADEV plugin registration.")
