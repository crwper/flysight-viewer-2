import numpy as np
import math
import os
from typing import Optional, Dict, Tuple, List
from flysight_plugin_sdk import DefaultTime

# --- Debug Flag ---
FLYSIGHT_DEBUG_ALLAN_STR = os.environ.get("FLYSIGHT_DEBUG_ALLAN", "True")
DEBUG_ALLAN = FLYSIGHT_DEBUG_ALLAN_STR.lower() in ("true", "1", "t", "yes")

if DEBUG_ALLAN:
    print("[allan_variance_plugin.py] DEBUG_ALLAN is ON")

# Attempt to import allantools
try:
    import allantools
    if DEBUG_ALLAN:
        print(f"[allan_variance_plugin.py] allantools library version: {allantools.__version__}")
except ImportError:
    print("[allan_variance_plugin.py] WARNING: allantools library not found...")
    allantools = None

from flysight_plugin_sdk import (
    MeasurementPlugin,
    register_measurement,
    SimplePlot,
    register_plot,
    meas
)

# --- Configuration ---
ALLAN_SENSOR_KEY = "ALLAN_ADEV"
IMU_SENSOR_KEY = "IMU"
IMU_TIME_KEY = "_time"
COMMON_TAUS_MEASUREMENT_NAME = "common_taus" # The single name for our tau array

ACCEL_UNIT_CONVERSION: Dict[str, float] = {"ax": 9.80665, "ay": 9.80665, "az": 9.80665}
GYRO_UNIT_CONVERSION: Dict[str, float] = {"wx": math.pi / 180.0, "wy": math.pi / 180.0, "wz": math.pi / 180.0}
MIN_DATA_SAMPLES_FOR_AVAR = 2000

# --- Python-level Caches ---
# Cache for common taus: Key: session_id_str -> Value: Optional[np.ndarray]
_COMMON_TAUS_CACHE: Dict[str, Optional[np.ndarray]] = {}

# Cache for ADEV values: Key: (session_id_str, original_imu_key_str) -> Value: Optional[np.ndarray]
_ADEV_VALUES_CACHE: Dict[Tuple[str, str], Optional[np.ndarray]] = {}

def _clear_internal_adev_caches():
    global _COMMON_TAUS_CACHE, _ADEV_VALUES_CACHE
    if DEBUG_ALLAN: print("[allan_variance_plugin.py] Clearing internal Allan caches (taus and adevs).")
    _COMMON_TAUS_CACHE.clear()
    _ADEV_VALUES_CACHE.clear()

def _get_imu_sampling_rate(session) -> Optional[float]:
    """Helper to get the sampling rate from IMU/_time."""
    imu_time_data_list = session.getMeasurement(IMU_SENSOR_KEY, IMU_TIME_KEY)
    if imu_time_data_list is None:
        if DEBUG_ALLAN: print("[_get_imu_sampling_rate] IMU time data not found.")
        return None
    
    time_axis = np.array(imu_time_data_list, dtype=float)
    if time_axis.size < MIN_DATA_SAMPLES_FOR_AVAR : # Or just < 2 for rate calculation
        if DEBUG_ALLAN: print(f"[_get_imu_sampling_rate] Not enough time samples ({time_axis.size}).")
        return None

    time_span = time_axis[-1] - time_axis[0]
    if abs(time_span) < 1e-9:
        if DEBUG_ALLAN: print("[_get_imu_sampling_rate] Time span is zero.")
        return None
            
    rate = float(len(time_axis) - 1) / time_span
    if rate <= 0:
        if DEBUG_ALLAN: print(f"[_get_imu_sampling_rate] Calculated rate <= 0 ({rate}).")
        return None
    return rate

class AllanCommonTausPlugin(MeasurementPlugin):
    """
    Computes and provides the common Tau values (x-axis for ADEV plots)
    based on the IMU sensor's time data.
    """
    sensor = ALLAN_SENSOR_KEY
    name = COMMON_TAUS_MEASUREMENT_NAME # e.g., "ALLAN_ADEV/common_taus"
    units = "s"

    def __init__(self):
        if DEBUG_ALLAN:
            print(f"[allan_variance_plugin.py] AllanCommonTausPlugin initialized for: {self.sensor}/{self.name}")

    def inputs(self) -> List[Dict]:
        # Depends only on the IMU time channel to determine sampling rate and tau range
        return [meas(IMU_SENSOR_KEY, IMU_TIME_KEY)]

    def compute(self, session) -> Optional[np.ndarray]:
        if not allantools:
            if DEBUG_ALLAN: print(f"[{self.name}.compute] Allantools not available.")
            return None

        session_id_str = str(session.getAttribute("SESSION_ID")) # Assuming SESSION_ID
        if session_id_str in _COMMON_TAUS_CACHE:
            if DEBUG_ALLAN: print(f"[{self.name}.compute] Returning cached common_taus for session {session_id_str}")
            return _COMMON_TAUS_CACHE[session_id_str]

        if DEBUG_ALLAN: print(f"[{self.name}.compute] Computing common_taus for session {session_id_str}")

        rate = _get_imu_sampling_rate(session)
        if rate is None:
            _COMMON_TAUS_CACHE[session_id_str] = None
            return None

        # We need some data to pass to allantools.adev to get taus.
        # It doesn't matter what the data is, as long as it's the correct length
        # and rate is provided. We use a dummy array.
        # Alternatively, if one of the IMU axes is guaranteed to exist, use that.
        # For simplicity, create a dummy array reflecting the number of samples.
        imu_time_data_list = session.getMeasurement(IMU_SENSOR_KEY, IMU_TIME_KEY)
        num_samples = len(imu_time_data_list) if imu_time_data_list else 0
        
        if num_samples < MIN_DATA_SAMPLES_FOR_AVAR: # Or MIN_DATA_SAMPLES_FOR_AVAR
            if DEBUG_ALLAN: print(f"[{self.name}.compute] Not enough samples ({num_samples}) for dummy data for tau calculation.")
            _COMMON_TAUS_CACHE[session_id_str] = None
            return None

        dummy_data = np.zeros(num_samples)

        try:
            if DEBUG_ALLAN: print(f"[{self.name}.compute] Calling allantools.adev with dummy data to get taus (rate: {rate:.2f} Hz)...")
            (taus_out, _, _, _) = allantools.adev(
                dummy_data, rate=rate, data_type="freq", taus="octave"
            )
            taus_array = np.array(taus_out, dtype=float)
            if DEBUG_ALLAN: print(f"[{self.name}.compute]   taus_out (len {len(taus_array)}): {taus_array[:min(5, len(taus_array))]}...")
            
            _COMMON_TAUS_CACHE[session_id_str] = taus_array
            return taus_array
        except Exception as e:
            print(f"[allan_variance_plugin.py] Error during allantools.adev for common_taus: {e}")
            if DEBUG_ALLAN:
                import traceback
                traceback.print_exc()
            _COMMON_TAUS_CACHE[session_id_str] = None
            return None

class AllanAdevValuePlugin(MeasurementPlugin):
    """
    Computes and provides the ADEV values (y-axis) for a specific IMU data channel.
    It will also ensure that the common_taus for this session are computed and cached
    in SessionData C++ side if this is the first ADEV value being requested for this session.
    """
    sensor = ALLAN_SENSOR_KEY # All ADEV outputs go under this sensor key

    def __init__(self, original_imu_key: str,
                 unit_conversion_factor: float, 
                 adev_display_units: str):
        self.original_imu_key = original_imu_key # e.g., "ax"
        self._unit_conversion_factor = unit_conversion_factor
        self.units = adev_display_units # e.g., "m/s^2"
        self.name = f"adev_{self.original_imu_key}" # e.g., "ALLAN_ADEV/adev_ax"

        if DEBUG_ALLAN:
            print(f"[allan_variance_plugin.py] AllanAdevValuePlugin initialized for: {self.sensor}/{self.name}")

    def inputs(self) -> List[Dict]:
        # Depends on the specific raw IMU data axis and the common IMU time channel
        return [
            meas(IMU_SENSOR_KEY, self.original_imu_key),
            meas(IMU_SENSOR_KEY, IMU_TIME_KEY),
            meas(ALLAN_SENSOR_KEY, COMMON_TAUS_MEASUREMENT_NAME) # Declare dependency on common taus
        ]

    def compute(self, session) -> Optional[np.ndarray]:
        if not allantools:
            if DEBUG_ALLAN: print(f"[{self.name}.compute] Allantools not available.")
            return None

        session_id_str = str(session.getAttribute("SESSION_ID"))
        cache_key = (session_id_str, self.original_imu_key)

        if cache_key in _ADEV_VALUES_CACHE:
            if DEBUG_ALLAN: print(f"[{self.name}.compute] Returning cached ADEV values for {self.original_imu_key}, session {session_id_str}")
            return _ADEV_VALUES_CACHE[cache_key]

        if DEBUG_ALLAN: print(f"[{self.name}.compute] Computing ADEV values for {self.original_imu_key}, session {session_id_str}")

        # Get common taus first (this will trigger its computation if not already done)
        # This also ensures that if this is the first adev plugin called, the common_taus
        # gets into the C++ cache via its own plugin mechanism.
        common_taus_array = session.getMeasurement(ALLAN_SENSOR_KEY, COMMON_TAUS_MEASUREMENT_NAME)
        if common_taus_array is None: # Should be np.array if successful
             if DEBUG_ALLAN: print(f"[{self.name}.compute] Failed to get common_taus for session {session_id_str}. Cannot compute ADEV for {self.original_imu_key}.")
             _ADEV_VALUES_CACHE[cache_key] = None
             return None
        common_taus_array_np = np.array(common_taus_array, dtype=float)


        imu_raw_axis_data_list = session.getMeasurement(IMU_SENSOR_KEY, self.original_imu_key)
        if imu_raw_axis_data_list is None:
            if DEBUG_ALLAN: print(f"[{self.name}.compute] Missing raw IMU data for {self.original_imu_key}.")
            _ADEV_VALUES_CACHE[cache_key] = None
            return None

        data_axis_raw = np.array(imu_raw_axis_data_list, dtype=float)
        
        if data_axis_raw.size < MIN_DATA_SAMPLES_FOR_AVAR:
            if DEBUG_ALLAN: print(f"[{self.name}.compute] Insufficient data samples ({data_axis_raw.size}) for {self.original_imu_key}.")
            _ADEV_VALUES_CACHE[cache_key] = None
            return None

        data_axis_converted = data_axis_raw * self._unit_conversion_factor
        
        rate = _get_imu_sampling_rate(session) # Re-fetch rate, or pass from common_taus plugin if it also stored it
        if rate is None:
            _ADEV_VALUES_CACHE[cache_key] = None
            return None

        if DEBUG_ALLAN:
            print(f"[{self.name}.compute]   For {self.original_imu_key}: Rate={rate:.2f} Hz, Conversion={self._unit_conversion_factor}")
            if data_axis_converted.size > 0: print(f"[{self.name}.compute]   Converted data (first 3): {data_axis_converted[:3]}")

        try:
            if DEBUG_ALLAN: print(f"[{self.name}.compute]   Calling allantools.adev for {self.original_imu_key} using pre-defined taus if possible...")
            
            # Use the common_taus_array_np if the allantools version supports passing taus directly.
            # Many versions expect `taus` to be a string like "octave", "decade", "all", or an actual array.
            # If passing an array directly:
            (taus_out, adevs_out, _, _) = allantools.adev(
                data_axis_converted,
                rate=rate,
                data_type="freq",
                taus=common_taus_array_np # Pass the pre-calculated common taus
            )
            # If the above `taus=common_taus_array_np` doesn't work or gives inconsistent taus_out,
            # revert to `taus="octave"` and rely on the rate being the same.
            # (taus_out, adevs_out, _, _) = allantools.adev(
            #     data_axis_converted, rate=rate, data_type="freq", taus="octave"
            # )

            # We primarily care about adevs_out here. taus_out should match common_taus_array_np.
            if DEBUG_ALLAN:
                print(f"[{self.name}.compute]   allantools.adev successful for {self.original_imu_key}.")
                # print(f"[{self.name}.compute]   taus_out (len {len(taus_out)}): {taus_out[:min(5, len(taus_out))]}...") # Should match common_taus
                print(f"[{self.name}.compute]   adevs_out (len {len(adevs_out)}): {adevs_out[:min(5, len(adevs_out))]}...")

            adevs_array = np.array(adevs_out, dtype=float)
            _ADEV_VALUES_CACHE[cache_key] = adevs_array
            return adevs_array
            
        except Exception as e:
            print(f"[allan_variance_plugin.py] Error during allantools.adev for {self.original_imu_key}: {e}")
            if DEBUG_ALLAN:
                import traceback
                traceback.print_exc()
            _ADEV_VALUES_CACHE[cache_key] = None
            return None

# --- Axes Configuration (IMU input keys and their plot properties) ---
IMU_AXES_CONFIG = {
    "ax": ("Accel X", ACCEL_UNIT_CONVERSION.get("ax", 1.0), "m/s²", "#FF6347"),
    "ay": ("Accel Y", ACCEL_UNIT_CONVERSION.get("ay", 1.0), "m/s²", "#32CD32"),
    "az": ("Accel Z", ACCEL_UNIT_CONVERSION.get("az", 1.0), "m/s²", "#1E90FF"),
    "wx": ("Gyro X", GYRO_UNIT_CONVERSION.get("wx", 1.0), "rad/s", "#FFD700"),
    "wy": ("Gyro Y", GYRO_UNIT_CONVERSION.get("wy", 1.0), "rad/s", "#DA70D6"),
    "wz": ("Gyro Z", GYRO_UNIT_CONVERSION.get("wz", 1.0), "rad/s", "#00CED1"),
}

# --- Register ADEV plugins and plots ---
if allantools:
    # Register the single common taus plugin first
    common_taus_plugin = AllanCommonTausPlugin()
    register_measurement(common_taus_plugin)
    if DEBUG_ALLAN:
        print(f"[allan_variance_plugin.py] Registered common taus plugin: {common_taus_plugin.sensor}/{common_taus_plugin.name}")

    # Register an ADEV value plugin and its plot for each IMU axis
    for original_imu_key, (plot_name_suffix, conv_factor, adev_units_str, plot_color) in IMU_AXES_CONFIG.items():
        
        adev_value_plugin = AllanAdevValuePlugin(
            original_imu_key=original_imu_key,
            unit_conversion_factor=conv_factor,
            adev_display_units=adev_units_str
        )
        register_measurement(adev_value_plugin)
        if DEBUG_ALLAN:
            print(f"[allan_variance_plugin.py] Registered ADEV value plugin: {adev_value_plugin.sensor}/{adev_value_plugin.name}")

        category_group = "Accelerometer" if "a" in original_imu_key else "Gyroscope"
        register_plot(SimplePlot(
            category=f"Allan Deviation ({category_group})",
            name=f"ADEV {plot_name_suffix}",
            units=adev_units_str,
            color=plot_color,
            sensor=ALLAN_SENSOR_KEY, # Y-data comes from this sensor
            measurement=f"adev_{original_imu_key}"  # Y-values (e.g., "adev_ax")
            # PlotWidget will need to be configured to use ALLAN_SENSOR_KEY/COMMON_TAUS_MEASUREMENT_NAME
            # as the X-axis for any plot whose sensor is ALLAN_SENSOR_KEY and measurement starts with "adev_".
        ))
        
    print(f"[allan_variance_plugin.py] Registered all Allan Deviation plugins and plots for sensor '{ALLAN_SENSOR_KEY}'.")
else:
    print("[allan_variance_plugin.py] allantools library not available. Skipping ADEV plugin registration.")
