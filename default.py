"""Default plugins for EuroNCAP VTA validation and plotting logic.

This plugin contains the original validation and plotting logic and is loaded
by default when no user plugin is available.
"""

from __future__ import annotations
import numpy as np
import logging
import traceback
from .interface import PluginMetadata, UserPlugin
from .context import ScenarioContext
from .utils import *
from ..postprocessing.rating import ISO18571
from erg_python import ERG
from can_python import BLF, ASC

logger = logging.getLogger(__name__)

# Placeholders
__MESSAGE__ = None
__SIGNAL__ = None
__UAQ__ = None

Array = np.ndarray | None
TimeSeries = tuple[Array, Array]
Value = float | str | bool | None
Index = int | None
Data = ERG | BLF | ASC
Log = LogChain | None

INT_MAX = 9_223_372_036_854_775_807
NOT_OCCURRED = INT_MAX
FREQ = 1000


"""
1. EuroNCAP section provides definitions and criteria directly from official documentation.

2. HMC section describes implementation of some workarounds and fallbacks. 
It must describe what is not defined or documented in the EuroNCAP section, but is implemented in actual code.
It must also describe what is defined but not implemented in actual code

3. Output section is only for functions that do not return a 'Value' or 'Index'.
If a function returns a tuple, or if the return value has an implicit meaning, it should be documented in this section.
"""


class DefaultPlugin(UserPlugin):
    @staticmethod
    def _series_fingerprint(y):
        """Debug helper: returns (min, max, size, unique_count) for a numeric array."""
        if y is None:
            return None
        y = np.asarray(y, dtype=float)
        y = y[np.isfinite(y)]
        if y.size == 0:
            return ("empty", 0)
        yq = np.round(y, 6)
        return (float(np.min(yq)), float(np.max(yq)), int(yq.size), int(np.unique(yq).size))

    def __init__(self):
        self.metadata = PluginMetadata(
            name="Default EuroNCAP Plugin",
            version="1.0.0",
            author="EuroNCAP VTA",
            description="Default validation and plotting logic using the original application algorithms",
        )
        self._signals_to_plot = {
            "FCA": [
                "vut_long_vel",
                "vut_lat_dev",
                "rel_long_dist",
                "vut_yaw_vel",
                "vut_steer_whl_vel",
            ],
            "ELK": [
                "vut_long_vel",
                "rel_long_vel",
                "rel_long_dist",
                "vut_lat_dev",
                "vut_lat_vel",
                "vut_yaw_vel",
                "vut_steer_whl_vel",
            ],
        }

        self.signal_map = None

    def _load_signal_map_from_db(self, cursor) -> dict:
        cursor.execute(
            """
            SELECT signal_key, erg_signal, erg_factor, erg_filter, phys_message, phys_signal, phys_factor, phys_filter
            FROM SignalMappings
            """
        )
        rows = cursor.fetchall()
        signal_map = {}
        for row in rows:
            signal_key, erg_signal, erg_factor, erg_filter, phys_message, phys_signal, phys_factor, phys_filter = row
            if erg_signal == "__UAQ__":
                erg_signal = __UAQ__
            if phys_message == "__MESSAGE__":
                phys_message = __MESSAGE__
            if phys_signal == "__SIGNAL__":
                phys_signal = __SIGNAL__

            erg_factor = erg_factor if erg_factor is not None else 1.0
            phys_factor = phys_factor if phys_factor is not None else 1.0

            signal_map[signal_key] = {
                "erg": erg_signal,
                "erg_factor": erg_factor,
                "erg_filter": bool(erg_filter),
                "phys": (phys_message, phys_signal),
                "phys_factor": phys_factor,
                "phys_filter": bool(phys_filter),
            }

        return signal_map

    def reload_signal_map(self, cursor) -> None:
        """Reload signal map from database. Called when signal mappings are updated."""
        self.signal_map = self._load_signal_map_from_db(cursor)

    def get_signal_map(self) -> dict:
        return self.signal_map

    def get_frequency(self) -> Value:
        return FREQ

    @ensure_log
    def get_signal(self, data: Data, signal_key: str, log: Log = None) -> TimeSeries:
        if self.signal_map is None:
            log.push("signal_map is None (not loaded)")
            return None, None

        if signal_key not in self.signal_map:
            log.push(f"signal_key '{signal_key}' not found in signal_map")
            return None, None

        try:
            signal_config = self.signal_map[signal_key]
            if isinstance(data, ERG):
                uaq_name = signal_config["erg"]
                factor = signal_config.get("erg_factor", 1.0)
                should_filter = signal_config.get("erg_filter", False)
                time = data["Time"]
                signal = data[uaq_name] * factor
                time, signal = interpolate(time, signal, FREQ, "step")
                if should_filter:
                    time, signal = filter(time, signal)
                return time, signal
            elif isinstance(data, (BLF, ASC)):
                msg, sig = signal_config.get("phys", (None, None))
                if not msg or not sig:
                    log.push(f"Signal '{signal_key}' not mapped for physical data (phys_message/phys_signal empty)")
                    return None, None
                message_name, signal_name = msg, sig
                factor = signal_config.get("phys_factor", 1.0)
                should_filter = signal_config.get("phys_filter", False)
                scope = data._scope
                time = scope.message(message_name).signal("Time")
                signal = scope.message(message_name).signal(signal_name) * factor
                time, signal = interpolate(time, signal, FREQ, "step")
                if should_filter:
                    time, signal = filter(time, signal)
                return time, signal
            else:
                log.push("Data passed to 'get_signal' is not of type ERG or BLF or ASC")
                return None, None
        except Exception as e:
            if isinstance(data, ERG):
                data_type = "ERG"
            elif isinstance(data, BLF):
                data_type = "BLF"
            elif isinstance(data, ASC):
                data_type = "ASC"
            else:
                data_type = "Unknown"

            log.push(f"Error occurred while reading signal '{signal_key}' from {data_type}: {e}")
            return None, None

    @ensure_log
    def get_signal_raw(self, data: Data, signal_key: str, log: Log = None) -> TimeSeries:
        """
        -- HMC --
        Reads physical (BLF/ASC) or ERG signals at their native frequency without
        resampling to FREQ. Used by fca_iso_rating to provide raw 100Hz PHYS data
        as required by EuroNCAP spec (no additional filtering on output data).
        ERG data is returned with optional filtering but no frequency interpolation.
        """
        if self.signal_map is None:
            log.push("signal_map is None (not loaded)")
            return None, None

        if signal_key not in self.signal_map:
            log.push(f"signal_key '{signal_key}' not found in signal_map")
            return None, None

        try:
            signal_config = self.signal_map[signal_key]

            if isinstance(data, ERG):
                uaq_name = signal_config["erg"]
                factor = signal_config.get("erg_factor", 1.0)
                should_filter = signal_config.get("erg_filter", False)
                time = data["Time"]
                signal = data[uaq_name] * factor
                if should_filter:
                    time, signal = filter(time, signal)
                return time, signal

            elif isinstance(data, (BLF, ASC)):
                message_name, signal_name = signal_config["phys"]
                if not message_name or not signal_name:
                    log.push(f"Signal '{signal_key}' not mapped for physical data (phys_message/phys_signal empty)")
                    return None, None
                factor = signal_config.get("phys_factor", 1.0)
                should_filter = signal_config.get("phys_filter", False)
                scope = data._scope
                time = scope.message(message_name).signal("Time")
                signal = scope.message(message_name).signal(signal_name) * factor
                if should_filter:
                    time, signal = filter(time, signal)
                return time, signal

            else:
                log.push(f"get_signal_raw: unsupported data type '{type(data).__name__}'")
                return None, None

        except Exception as e:
            log.push(f"get_signal_raw error for '{signal_key}': {e}")
            return None, None

    @ensure_log
    def get_ttc(self, data: Data, eps: float = 1e-9, context: ScenarioContext = None, log: Log = None) -> TimeSeries:
        """
        -- EuroNCAP --
        CA 004 Data Acquisition and Assessment Criteria Calculation v1.1
        3.1.3 Time-to-Collision
        Time-to-collision (TTC) is defined, at an instant t, as the time remaining before a
        collision would occur if the relative speed between the VUT and the Target remains constant:
        TTC(t) = D_VUT_TARGET(t) / (V_VUT(t) - V_target(t))
        With:
        D_VUT_TARGET(t): Closest distance between the Target bounding box and the VUT profiled line
        (i.e., closest polygon-to-polygon longitudinal distance) at the instant t
        V_VUT(t): Speed of VUT at the instant t
        V_target(t): Speed of target at the instant t

        -- HMC --
        When TTC is already calculated and stored as a UAQ or a CAN signal, use that instead
        If TTC needs to be calculated, add a small value to the denominator to prevent division-by-zero
        Instead of subtracting target velocity from VUT velocity, directly use relative velocity
        """
        ttc_time, ttc_signal = self.get_signal(data, "ttc", log=None)
        if ttc_time is not None and ttc_signal is not None:
            return ttc_time, ttc_signal

        dist_time, dist_signal = self.get_signal(data, "rel_long_dist", log)
        vel_time, vel_signal = self.get_signal(data, "rel_long_vel", log)

        if dist_time is None or dist_signal is None:
            log.push("'rel_long_dist' could not be retrieved")
            return None, None
        if vel_time is None or vel_signal is None:
            log.push("'rel_long_vel' could not be retrieved")
            return None, None

        dist_time, dist_signal, vel_time, vel_signal = truncate_signals(dist_time, dist_signal, vel_time, vel_signal)

        return dist_time, -dist_signal / (vel_signal + eps)

    @ensure_log
    def get_time_headway(self, data: Data, eps: float = 1e-9, context: ScenarioContext = None, log: Log = None) -> TimeSeries:
        """
        -- EuroNCAP --
        CA 004 Data Acquisition and Assessment Criteria Calculation v1.1
        3.1.4 Time Headway
        Time Headway (THW) is defined, at an instant t, as the time it takes the VUT to travel the
        closest distance between the front of the VUT and the rear of the preceding Target.
        THW(t) = D_VUT_TARGET(t) / V_VUT(t)
        With:
        D_VUT_TARGET(t): Closest distance between the Target bounding box and the VUT profiled line
        (i.e., closest polygon-to-polygon longitudinal distance) at the instant t
        V_VUT(t): Speed of VUT at the instant t

        -- HMC --
        Add a small value to the denominator to prevent division-by-zero
        """
        dist_time, dist_signal = self.get_signal(data, "rel_long_dist", log)
        vel_time, vel_signal = self.get_signal(data, "vut_long_vel", log)

        if dist_time is None or dist_signal is None:
            log.push("'rel_long_dist' could not be retrieved")
            return None, None
        if vel_time is None or vel_signal is None:
            log.push("'rel_long_vel' could not be retrieved")
            return None, None

        dist_time, dist_signal, vel_time, vel_signal = truncate_signals(dist_time, dist_signal, vel_time, vel_signal)

        return dist_time, -dist_signal / (vel_signal + eps)

    @ensure_log
    def fca_t_steer_start_idx(self, data: Data, context: ScenarioContext, log: Log = None) -> Index:
        """
        -- EuroNCAP --
        Crash Avoidance Frontal Collisions v1.1
        1.5.1 Variables
        T_steer: Time where the VUT enters in curve segment

        -- HMC --
        The start of the curve segment is defined by global coordinates varying on VUT speed
        """
        scenario = context.scenario
        vut_speed = context.vut_speed
        if "fo" in scenario or "fs" in scenario:
            if vut_speed == 10:
                start_x = -10.629
                start_y = -1.75
            elif vut_speed == 15:
                start_x = -14.4665
                start_y = -1.75
            elif vut_speed == 20:
                start_x = -13.101
                start_y = -1.75
            else:
                log.push("Unsupported scenario")
                return None
        else:
            start_x = -13.101
            start_y = -1.75
        _, tx = self.get_signal(data, "road_tx", log)
        _, ty = self.get_signal(data, "road_ty", log)

        if tx is None:
            log.push("Road tx signal could not be retrieved")
            return None
        if ty is None:
            log.push("Road ty signal could not be retrieved")
            return None

        if len(tx) != len(ty):
            log.push("Road tx and ty have different signal lengths")
            return None

        distances = np.sqrt((tx - start_x) ** 2 + (ty - start_y) ** 2)
        return int(np.argmin(distances))

    @ensure_log
    def fca_t_steer_end_idx(self, data: Data, context: ScenarioContext, log: Log = None) -> Index:
        """
        -- EuroNCAP --
        Crash Avoidance Frontal Collisions v1.1
        1.5.1 Variables
        T_steer: Time where the VUT enters in curve segment

        -- HMC --
        T_steer_end is time where VUT leaves curve segment
        The end of the curve segment is defined by global coordinates varying on VUT speed
        """
        scenario = context.scenario
        vut_speed = context.vut_speed

        if "fo" in scenario or "fs" in scenario:
            if vut_speed == 10:
                end_x = 1.75
                end_y = 10.6297
            elif vut_speed == 15:
                end_x = 1.75
                end_y = 14.4665
            elif vut_speed == 20:
                end_x = 1.7509
                end_y = 18.8269
            else:
                log.push("Unsupported scenario")
                return None
        else:
            end_x = -1.75
            end_y = -13.1006

        _, tx = self.get_signal(data, "road_tx", log)
        _, ty = self.get_signal(data, "road_ty", log)

        if tx is None:
            log.push("Road tx signal could not be retrieved")
            return None
        if ty is None:
            log.push("Road ty signal could not be retrieved")
            return None

        if len(tx) != len(ty):
            log.push("Road tx and ty have different signal lengths")
            return None

        distances = np.sqrt((tx - end_x) ** 2 + (ty - end_y) ** 2)
        return int(np.argmin(distances))

    @ensure_log
    def fca_t_target_accel_end_idx(self, data: Data, context: ScenarioContext = None, log: Log = None) -> Index:
        """
        -- EuroNCAP --
        Crash Avoidance Frontal Collisions v1.1
        1.5.1 Variables
        Crossing scenarios: T_0 = 0.5s after target acceleration phase
        3.1.3.1 Car-to-Car Crossing
        To achieve the correct GVT speed, the GVT must be accelerated at a rate >1m/s2 during the acceleration phase.
        3.1.3.2 Car-to-Motorcylist Crossing
        To achieve the correct EMT speed, the EMT must be accelerated at a rate >1m/s2 during the acceleration phase.

        -- HMC --
        t_target_accel_end is only used for crossing scenarios
        """
        _, acc = self.get_signal(data, "target_long_acc", log)
        if acc is None:
            log.push("Could not retrieve signal 'target_long_acc'")
            return None

        threshold = 1.0
        temp_idx = get_first_index_larger_or_equal_to(acc, threshold)
        if temp_idx is None:
            log.push(f"Target acceleration is always smaller than {threshold}")
            return None

        idx = get_first_index_smaller_or_equal_to(acc[temp_idx:], threshold)
        if idx is None:
            log.push(f"Target acceleration remains larger than {threshold} after acceleration starts")
            return None

        return idx + temp_idx

    @ensure_log
    def fca_t_target_decel_start_idx(self, data: Data, context: ScenarioContext = None, log: Log = None) -> Index:
        """
        -- EuroNCAP --
        Crash Avoidance Frontal Collisions v1.1
        1.5.1 Variables
        Braking scenarios: T_0 = T_Target_deceleration_start - 1s
        3.1.1.1 Car-to-Car Rear
        For CCRb, the Time Headway = 1.0 s, and the target acceleration = -4m/s2 for both Standard Range and Extended Range.
        The desired deceleration of the GVT shall be reached within 1.0 second (T_0 + 2.0s) which after the GVT shall
        remain within ± 0.5 km/h of the reference speed profile, derived from the desired deceleration, until the vehicle speed equals 2km/h.
        3.1.1.3 Car-to-Motorcyclist Rear
        For CMRb, the Time Headway = 1.0 s, and the target acceleration = -4m/s2 for both Standard Range and Extended range.
        The desired deceleration of the EMT shall be reached within 1.0 second (T_0 + 2.0s) which after the EMT shall
        remain within ± 0.5 km/h of the reference speed profile, derived from the desired deceleration, until the vehicle speed equals 2km/h.

        -- HMC --
        The allowance for target acceleration is not defined in acceleration but by target velocity profile, arbitrary allowance of 0.1 m/s2 is applied
        """
        _, acc = self.get_signal(data, "target_long_acc", log)
        if acc is None:
            log.push("Could not retrieve signal 'target_long_acc'")
            return None

        threshold = -3.9
        idx = get_first_index_smaller_or_equal_to(acc, threshold)
        if idx is None:
            log.push(f"Target acceleration is always larger than {threshold}")
            return None

        return idx

    @ensure_log
    def fca_turning_t_0_idx(self, data: Data, context: ScenarioContext = None, log: Log = None) -> Index:
        """
        -- EuroNCAP --
        Crash Avoidance Frontal Collisions v1.1
        1.5.1 Variables
        Turning scenarios: T_0 = T_steer -1s

        -- HMC --
        Minimum value is capped at 0.
        """
        t_steer_idx = self.fca_t_steer_start_idx(data, context, log)
        if t_steer_idx is None:
            log.push("t_steer could not be calculated")
            return None
        freq = self.get_frequency()

        return max(0, t_steer_idx - freq)

    @ensure_log
    def fca_crossing_t_0_idx(self, data: Data, context: ScenarioContext = None, log: Log = None) -> Index:
        """
        -- EuroNCAP --
        Crash Avoidance Frontal Collisions v1.1
        1.5.1 Variables
        Crossing scenarios: T_0 = 0.5s after target acceleration phase

        -- HMC --
        Crossing scenarios: CCCscp, CMCscp
        """
        t_target_accel_end_idx = self.fca_t_target_accel_end_idx(data, context, log)
        if t_target_accel_end_idx is None:
            log.push("t_target_accel_end could not be calculated")
            return None

        freq = self.get_frequency()

        return t_target_accel_end_idx + int(0.5 * freq)

    @ensure_log
    def fca_braking_t_0_idx(self, data: Data, context: ScenarioContext = None, log: Log = None) -> Index:
        """
        -- EuroNCAP --
        Crash Avoidance Frontal Collisions v1.1
        1.5.1 Variables
        Braking scenarios: T_0 = T_Target_deceleration_start - 1s

        -- HMC --
        Braking scenarios are a subset of longitudinal scenarios
        Braking scenarios: CMRb, CCRb
        Minimum value is capped at 0.
        """
        t_target_decel_start_idx = self.fca_t_target_decel_start_idx(data, context, log)
        if t_target_decel_start_idx is None:
            log.push("t_target_decel_start could not be calculated")
            return None
        freq = self.get_frequency()

        return max(0, t_target_decel_start_idx - freq)

    @ensure_log
    def fca_default_t_0_idx(self, data: Data, context: ScenarioContext = None, log: Log = None) -> Index:
        """
        -- EuroNCAP --
        Crash Avoidance Frontal Collisions v1.1
        1.5.1 Variables
        Time of test start T_0 = TTC 4s, unless stated otherwise

        -- HMC --
        In order to correctly extract t_0 from the vicinity of a controller intervention, search from t_aeb - 12s to t_aeb.
        First search for ttc value crossing 4s, fallback to closest to 4s.
        When Taeb is unavailable or TTC=4s not found in the 12s window, fall back to full-scan of entire TTC array.
        """
        _, ttc = self.get_ttc(data, context=context, log=log)
        if ttc is None:
            log.push("TTC could not be calculated")
            return None

        freq = int(self.get_frequency())
        threshold = 4.0

        def _find_ttc_4s(ttc_window, base_idx=0):
            valid_mask = ~np.isnan(ttc_window) & (ttc_window > 0) & (ttc_window <= 200)
            valid_indices = np.flatnonzero(valid_mask)
            if len(valid_indices) < 2:
                return None

            breaks = np.where(np.diff(valid_indices) > 1)[0] + 1
            idx_segments = np.split(valid_indices, breaks)

            best_min_idx = None
            best_min_dist = float("inf")

            for idx_seg in idx_segments:
                if len(idx_seg) < 2:
                    continue
                ttc_seg = ttc_window[idx_seg[0] : idx_seg[-1] + 1]

                cross_idx = get_first_index_cross_down(ttc_seg, threshold)
                if cross_idx is not None:
                    return base_idx + idx_seg[0] + cross_idx

                distances = np.abs(ttc_seg - threshold)
                local_min_idx = int(np.argmin(distances))
                if float(distances[local_min_idx]) < best_min_dist:
                    best_min_dist = float(distances[local_min_idx])
                    best_min_idx = base_idx + idx_seg[0] + local_min_idx

            return best_min_idx

        # 1) Taeb-window (12s)
        t_aeb_idx = self.fca_t_aeb_idx(data, context, log)
        if t_aeb_idx is not None:
            start_idx = max(0, int(t_aeb_idx) - 12 * freq)
            end_idx = int(t_aeb_idx)
            idx = _find_ttc_4s(ttc[start_idx:end_idx], base_idx=start_idx)
            if idx is not None:
                log.push(f"Test start (T_0): TTC=4s found in Taeb-window at index {idx}")
                return int(idx)
            log.push("No TTC=4s in Taeb-window(12s), falling back to full-scan")
        else:
            log.push("Taeb not available, falling back to full-scan TTC=4s")

        # 2) Full-scan fallback
        idx = _find_ttc_4s(ttc, base_idx=0)
        if idx is not None:
            log.push(f"TTC=4s found in full-scan. t0_idx={idx}")
            return int(idx)

        log.push("TTC=4s not found (Taeb-window and full-scan both failed)")
        return None

    @ensure_log
    def fca_t_end_idx(self, data: Data, context: ScenarioContext, log: Log = None) -> Index:
        """
        -- EuroNCAP --
        Crash Avoidance Frontal Collisions v1.1
        1.5.1 Variables
        T_end: Time of test end (see 4.3.2 and 4.3.3)
        4.3.2 AEB tests
        The end of a test, where the AEB function is assessed and for CCRs FCW and CMRs FCW, is
        considered when one of the following occurs:
        - VVUT = 0km/h (crossing) or VVUT = Vtarget (longitudinal)
        - Contact between VUT and target
        - The target has left the VUT path or VUT has left the target path
        +-----------------------------+--------------+---------+----------+--------------+
        |                             | Longitudinal | Turning | Crossing | Longitudinal |
        |                             |    (Rear)    |         |          |    (Front)   |
        +-----------------------------+--------------+---------+----------+--------------+
        | V_VUT = 0 km/h              |     [X]      |  [X*]   |   [X]    |     [X]      |
        +-----------------------------+--------------+---------+----------+--------------+
        | V_VUT = V_Target            |     [X]      |         |          |              |
        +-----------------------------+--------------+---------+----------+--------------+
        | Contact between VUT/Target  |     [X]      |   [X]   |   [X]    |     [X]      |
        +-----------------------------+--------------+---------+----------+--------------+
        | Target has left VUT path    |              |   [X]   |   [X]    |              |
        +-----------------------------+--------------+---------+----------+--------------+
        * The VUT must not enter the path of the target to achieve the pass
        4.3.3 FCW tests
        Where the FCW function is assessed, the end of a test is considered when one of the following occurs:
        - VVUT = Vtarget (longitudinal)
        - TFCW
        - TTC ≤ 1.5s , after which an evasive action can be started

        -- HMC --
        For "VUT has left the target path" and "The VUT must not enter the path of the target to achieve the pass", apply 4.2.3 VUT lateral deviation limits.
        Currently no logic is implemented to check "The target has left the VUT path".

        AEB: Stop search starts from Taeb (not T_0) to ignore stops before AEB activation.
        If no termination condition is met, the last sample is used as fallback.
        If T_0 cannot be calculated, 0 is used as fallback.
        Candidate indices are bounds-checked against signal length.
        """
        def _safe_at(arr, idx, fmt=None):
            if arr is None or idx is None:
                return "N/A" if fmt else None
            idx = int(idx)
            if idx < 0 or idx >= len(arr):
                return "N/A" if fmt else None
            try:
                val = float(arr[idx])
                return f"{val:{fmt}}" if fmt else val
            except Exception:
                return "N/A" if fmt else None

        function_type = context.function_type
        cluster = context.cluster
        scenario = context.scenario

        time, speed = self.get_signal(data, "vut_long_vel", log)
        if speed is None:
            log.push("Missing required signal 'vut_long_vel' — cannot determine T_end")
            return None

        _, acc = self.get_signal(data, "vut_long_acc", log)
        _, dist = self.get_signal(data, "rel_long_dist", log)
        _, rel_vel = self.get_signal(data, "rel_long_vel", log)
        _, ttc = self.get_ttc(data, context=context, log=log)

        t_0_idx = self.fca_t_0_idx(data, context, log)
        if t_0_idx is None:
            log.push("Test start time unavailable — using beginning of recording")
            t_0_idx = 0
        t_0_idx = max(0, int(t_0_idx))

        # --- diagnostic: log Taeb once per group ---
        t_aeb_idx = self.fca_t_aeb_idx(data, context, log=None)
        if t_aeb_idx is not None:
            t_aeb_idx = int(t_aeb_idx)
            log.push(
                f"[TAEB] idx={t_aeb_idx} | "
                f"t={_safe_at(time, t_aeb_idx, '.4f')}s | "
                f"v={_safe_at(speed, t_aeb_idx, '.2f')}m/s | "
                f"a={_safe_at(acc, t_aeb_idx, '.2f')}m/s² | "
                f"d={_safe_at(dist, t_aeb_idx, '.2f')}m | "
                f"TTC={_safe_at(ttc, t_aeb_idx, '.2f')}s"
            )

        if function_type == "AEB":
            start_idx = max(0, int(t_aeb_idx) if t_aeb_idx is not None else t_0_idx)

            vut_stop_idx = get_first_index_smaller_or_equal_to(speed[start_idx:], 0.1)
            if vut_stop_idx is not None:
                t_end_idx = start_idx + int(vut_stop_idx)
                log.push(
                    f"[TEND] idx={t_end_idx} | "
                    f"t={_safe_at(time, t_end_idx, '.4f')}s | "
                    f"v={_safe_at(speed, t_end_idx, '.2f')}m/s | "
                    f"reason=STOP"
                )
                return t_end_idx

            collision_idx = self.fca_t_collision_idx(data, context, log)

            vut_equal_target_idx = None
            if cluster == "Longitudinal" and rel_vel is not None:
                idx = get_first_index_larger_than(rel_vel[t_0_idx:], -0.5)
                if idx is not None:
                    vut_equal_target_idx = idx + t_0_idx

            leave_path_idx = None
            _, lat_dev = self.get_signal(data, "vut_lat_dev", log)
            if lat_dev is not None:
                threshold = 0.1 if cluster == "Turning" else 0.05
                idx = get_first_index_larger_than(np.abs(lat_dev[t_0_idx:]), threshold)
                if idx is not None:
                    leave_path_idx = idx + t_0_idx

            candidates = [collision_idx, vut_equal_target_idx, leave_path_idx]
            valid = [
                i for i in candidates
                if i is not None and i != NOT_OCCURRED and int(i) < len(time)
            ]
            t_end_idx = min(valid) if valid else len(time) - 1

            if not valid:
                log.push("No AEB termination condition met — using end of recording")

            log.push(
                f"[TEND] idx={t_end_idx} | "
                f"t={_safe_at(time, t_end_idx, '.4f')}s | "
                f"v={_safe_at(speed, t_end_idx, '.2f')}m/s | "
                f"reason={'CONDITION' if valid else 'FALLBACK'}"
            )
            return t_end_idx

        if function_type == "FCW":
            vut_equal_target_idx = None
            if cluster == "Longitudinal":
                if rel_vel is None:
                    log.push("Could not retrieve signal 'rel_long_vel'")
                else:
                    idx = get_first_index_larger_than(rel_vel[t_0_idx:], -0.5)
                    if idx is not None:
                        vut_equal_target_idx = idx + t_0_idx

            fcw_idx = self.fca_t_warning_idx(data, 1, context, log)
            if fcw_idx is None:
                log.push("Could not determine FCW")

            ttc_below_threshold_idx = None
            if ttc is None:
                log.push("TTC could not be calculated")
            else:
                idx = get_first_index_smaller_or_equal_to(ttc[t_0_idx:], 1.5)
                if idx is None:
                    log.push("TTC is always larger than 1.5 after t_0")
                else:
                    ttc_below_threshold_idx = t_0_idx + idx

            indices = [fcw_idx, vut_equal_target_idx, ttc_below_threshold_idx]
            valid_indices = [
                i for i in indices
                if i is not None and i != NOT_OCCURRED and int(i) < len(time)
            ]

            if valid_indices:
                t_end_idx = min(valid_indices)
                log.push(
                    f"[TEND] idx={t_end_idx} | "
                    f"t={_safe_at(time, t_end_idx, '.4f')}s | "
                    f"v={_safe_at(speed, t_end_idx, '.2f')}m/s | "
                    f"reason=FCW"
                )
                return t_end_idx

            log.push("No valid FCW termination condition found")
            return None

        log.push(f"Scenario function type is '{function_type}' (expected 'AEB' or 'FCW') — check scenario configuration")
        return None

    @ensure_log
    def fca_t_0_idx(self, data: Data, context: ScenarioContext, log: Log = None) -> Index:
        """
        -- EuroNCAP --
        Crash Avoidance Frontal Collisions v1.1
        1.5.1 Variables
        Time of test start T_0 = TTC 4s, unless stated otherwise
        Turning scenarios: T_0 = T_steer -1s
        Braking scenarios: T_0 = T_Target_deceleration_start -1s
        Crossing scenarios: T_0 = 0.5s after target acceleration phase

        -- HMC --
        Crossing scenarios: CCCscp, CMCscp
        Braking scenarios: CCRb, CMRb
        When scenario-specific T_0 fails, fall back to TTC=4s (fca_default_t_0_idx).
        If that also fails, return 0 (start of recording) to prevent downstream None propagation.
        """
        cluster = context.cluster
        scenario = context.scenario

        if cluster == "Crossing" and "scp" in scenario:
            t0 = self.fca_crossing_t_0_idx(data, context, log)
        elif cluster == "Crossing":
            t0 = self.fca_default_t_0_idx(data, context, log)
            if t0 is not None:
                return int(t0)
            log.push("[t0] default TTC=4s failed, fallback t0=0")
            return 0
        elif cluster == "Turning":
            t0 = self.fca_turning_t_0_idx(data, context, log)
        elif cluster == "Longitudinal" and scenario in ("CMRb", "CCRb"):
            t0 = self.fca_braking_t_0_idx(data, context, log)
        else:
            t0 = self.fca_default_t_0_idx(data, context, log)
            if t0 is not None:
                return int(t0)
            log.push("[t0] default TTC=4s failed, fallback t0=0")
            return 0

        if t0 is not None:
            return int(t0)

        # Fallback: try default TTC=4s
        log.push(f"[t0] {cluster}/{scenario} method failed, fallback to TTC=4s")
        t0 = self.fca_default_t_0_idx(data, context, log)
        if t0 is not None:
            return int(t0)

        log.push("[t0] TTC=4s fallback also failed, final fallback t0=0")
        return 0

    @ensure_log
    def fca_t_decel_idx(self, data: Data, context: ScenarioContext = None, log: Log = None) -> Index:
        """
        -- EuroNCAP --
        Crash Avoidance Frontal Collisions v1.1
        Definitions
        T_AEB - means the time where the AEB system activates. Activation time is determined by
        identifying the last data point where the filtered acceleration signal is below -3 m/s2,
        and then going back to the point in time where the acceleration first crossed -1 m/s2

        -- HMC --
        This function is used for fallback calculation of remaining distance. Apply the thresholds for t_aeb.
        """
        _, acc = self.get_signal(data, "vut_long_acc", log)
        if acc is None:
            log.push("Could not retrieve signal 'vut_long_acc'")
            return None

        threshold = -3.0
        idx = get_first_index_smaller_or_equal_to(acc, threshold)
        if idx is None:
            log.push(f"VUT acceleration is always larger than {threshold}")
            return None

        return idx

    @ensure_log
    def fca_t_aeb_idx(self, data: Data, context: ScenarioContext = None, log: Log = None) -> Index:
        """
        -- EuroNCAP --
        Crash Avoidance Frontal Collisions v1.1
        Definitions
        Autonomous Emergency Braking (AEB) - braking that is applied automatically by the vehicle in response to
        the detection of a likely collision to reduce the vehicle speed and potentially avoid the collision.
        T_AEB - means the time where the AEB system activates. Activation time is determined by
        identifying the last data point where the filtered acceleration signal is below -3 m/s2,
        and then going back to the point in time where the acceleration first crossed -1 m/s2

        -- HMC --
        When multiple -3.0 m/s² segments exist, the segment closest to FCW activation
        is selected instead of the first one. This avoids picking an unrelated braking
        event when the vehicle brakes multiple times before the actual AEB activation.
        If FCW is unavailable or has a length mismatch, the first segment is used.
        """
        _, acc = self.get_signal(data, "vut_long_acc", log)
        if acc is None:
            log.push("Could not retrieve signal 'vut_long_acc'")
            return None

        threshold_below = -3.0
        threshold_cross = -1.0

        # --- FCW activation index ---
        _, fcw = self.get_signal(data, "fcw", log)
        fcw_active_idx = None
        if fcw is not None and len(fcw) == len(acc):
            fcw_active_mask = fcw != 0
            if np.any(fcw_active_mask):
                fcw_active_idx = int(np.argmax(fcw_active_mask))
                log.push(f"[Taeb] FCW active at idx={fcw_active_idx}")
            else:
                log.push("[Taeb] FCW signal always 0 -> use first -3.0 occurrence")
        else:
            log.push("[Taeb] FCW signal unavailable or length mismatch -> use first -3.0 occurrence")

        # --- all -3.0 segments ---
        all_below_indices = np.flatnonzero(acc <= threshold_below)
        if len(all_below_indices) == 0:
            min_acc = float(np.min(acc)) if len(acc) > 0 else None
            log.push(f"[Taeb] acc never reaches {threshold_below} (min={min_acc} m/s²)")
            return None

        breaks = np.where(np.diff(all_below_indices) > 1)[0] + 1
        segments = np.split(all_below_indices, breaks)
        segment_starts = np.array([seg[0] for seg in segments])
        log.push(f"[Taeb] found {len(segments)} segment(s) starting at {segment_starts.tolist()}")

        # --- pick segment closest to FCW ---
        if fcw_active_idx is not None:
            distances = np.abs(segment_starts - fcw_active_idx)
            chosen_seg = segments[int(np.argmin(distances))]
        else:
            chosen_seg = segments[0]
        chosen_start = int(chosen_seg[0])

        # --- trace back to -1.0 crossing before chosen segment ---
        idx = get_last_index_larger_than(acc[:chosen_start], threshold_cross)
        result_idx = idx if idx is not None else chosen_start

        log.push(f"[Taeb] t_aeb_idx={result_idx} acc={float(acc[result_idx]):.4f} m/s²")
        return result_idx

    def _estimate_phys_t_aeb_idx_from_tend(
        self, phys_time_raw, phys_acc_raw, phys_tend_t, log=None
    ):
        """
        -- HMC --
        Fallback Taeb estimation for PHYS data when fca_t_aeb_idx returns None.
        Uses the same -3.0 / -1.0 threshold logic as fca_t_aeb_idx, but searches
        only up to Tend and picks the segment closest to Tend (the final braking).
        If no -3.0 segment is found, falls back to Tend - 2s.
        """
        threshold_below = -3.0
        threshold_cross = -1.0

        end_idx = int(np.searchsorted(phys_time_raw, phys_tend_t, side="right"))
        end_idx = min(end_idx, len(phys_acc_raw))
        search_acc = phys_acc_raw[:end_idx]

        all_below_indices = np.flatnonzero(search_acc <= threshold_below)

        if len(all_below_indices) == 0:
            fallback_t = phys_tend_t - 2.0
            fallback_idx = int(np.searchsorted(phys_time_raw, fallback_t, side="right"))
            fallback_idx = max(0, min(fallback_idx, len(phys_time_raw) - 1))
            if log:
                log.push(f"[ISO][Taeb-est] Fallback: -3.0 not found -> Tend-2s idx={fallback_idx}")
            return int(fallback_idx)

        breaks = np.where(np.diff(all_below_indices) > 1)[0] + 1
        segments = np.split(all_below_indices, breaks)
        segment_starts = np.array([seg[0] for seg in segments])

        distances = np.abs(segment_starts - end_idx)
        chosen_seg = segments[int(np.argmin(distances))]
        chosen_start = int(chosen_seg[0])

        cross_idx = get_last_index_larger_than(search_acc[:chosen_start], threshold_cross)
        estimated_idx = cross_idx if cross_idx is not None else chosen_start

        if log:
            log.push(
                f"[ISO][Taeb-est] estimated_taeb=idx{estimated_idx} "
                f"t={float(phys_time_raw[estimated_idx]):.4f}s "
                f"acc={float(search_acc[estimated_idx]):.4f} m/s²"
            )
        return int(estimated_idx)

    @ensure_log
    def fca_t_warning_idx(self, data: Data, level: int, context: ScenarioContext = None, log: Log = None) -> Index:
        """
        -- EuroNCAP --
        Crash Avoidance Frontal Collisions v1.1
        Definitions
        Forward Collision Warning (FCW) - an audio-visual warning that is provided automatically by the vehicle in
        response to the detection of a likely collision to alert the driver.
        T_FCW - means the time where the audible warning of the FCW starts.
        The starting point is determined by audible recognition.

        -- HMC --
        FCW warning levels are 1 to 3.

        -- Output --
        Returns the first instance where FCW warning level equals the specified level.
        If no warning occurs, returns NOT_OCCURRED.
        If warning cannot be determined, returns None.
        """
        _, fcw = self.get_signal(data, "fcw", log)
        if fcw is None:
            log.push("Could not retrieve signal 'fcw'")
            return None

        idx = get_first_index_equal_to(fcw, level)
        if idx is None:
            return NOT_OCCURRED

        return idx

    @ensure_log
    def fca_t_collision_idx(self, data: Data, context: ScenarioContext = None, log: Log = None) -> Index:
        """
        -- EuroNCAP --
        Crash Avoidance Frontal Collisions v1.1
        Definitions
        V_impact - means the speed at which the profiled line around the front, rear end, or side of the VUT
        coincides with the virtual box around the test targets (platform not included in the virtual box)
        EPTa, EPTc, EBTa and EMT as shown in the right part of the figures below, as illustrated in
        Figure 0-1 and Figure 0-2.

        -- HMC --
        Primary method: Uses the 'collision_count' signal directly.
        Returns the first index where collision_count > 0.

        Fallback method (when collision_count unavailable):
        Uses relative longitudinal distance to infer collision.
        1. Determines a starting index from the earliest of:
           - FCW warning levels (1, 2, or 3)
           - t_aeb (AEB activation time)
           - t_0 (test start time)
        2. Searches for the first point where distance <= 0.1m
        3. Validates that VUT speed >= 2.0 m/s at that point
           (speeds below 2.0 m/s are not considered collisions)

        -- Output --
        Returns the first index where a collision occurs.
        If no collision occurs, returns NOT_OCCURRED.
        If collision cannot be determined, returns None.
        """
        _, coll = self.get_signal(data, "collision_count", log)
        if coll is not None:
            idx = get_first_index_larger_than(coll, 0)
            if idx is None:
                return NOT_OCCURRED
            return idx

        log.push("Collision could not be directly retrieved. Using distance metric instead.")

        _, dist_signal = self.get_signal(data, "rel_long_dist", log)
        if dist_signal is None:
            log.push("Relative longitudinal distance could not be retrieved")
            return None

        lvl_1 = self.fca_t_warning_idx(data, 1, context, log)
        lvl_2 = self.fca_t_warning_idx(data, 2, context, log)
        lvl_3 = self.fca_t_warning_idx(data, 3, context, log)

        valid_indices = [idx for idx in [lvl_1, lvl_2, lvl_3] if idx is not None and idx != NOT_OCCURRED]
        temp_idx = min(valid_indices) if valid_indices else None
        if temp_idx is None:
            logger.warning("FCW warning could not be determined. Using t_aeb instead.")
            temp_idx = self.fca_t_aeb_idx(data, context, log)
            if temp_idx is None:
                logger.warning("t_aeb could not be determined. Using t_0 instead.")
                temp_idx = self.fca_t_0_idx(data, context, log)
                if temp_idx is None:
                    logger.warning("t_0 could not be determined")
                    temp_idx = 0

        signal = dist_signal[temp_idx:]
        idx = get_first_index_smaller_or_equal_to(signal, 0.1)
        if idx is None:
            # Signals required for collision determination are valid but no collision occurred
            return NOT_OCCURRED
        _, vut_speed = self.get_signal(data, "vut_long_vel", log)
        if vut_speed is None:
            logger.warning(f"Could not retrieve signal 'vut_long_vel' for {context}. Skipping VUT speed check for collision detection.")
            return idx + temp_idx
        if vut_speed[temp_idx + idx] < 2.0:
            # VUT speed too low to consider as collision
            return NOT_OCCURRED
        return idx + temp_idx

    @ensure_log
    def fca_warning_levels(self, data: Data, context: ScenarioContext = None, log: Log = None) -> Value:
        """
        -- EuroNCAP --
        Crash Avoidance Frontal Collisions v1.1
        Definitions
        T_FCW - means the time where the audible warning of the FCW starts.
        The starting point is determined by audible recognition.

        -- HMC --
        Checks FCW warning levels 1, 2, and 3 for activation.

        -- Output --
        Returns a hyphen-joined string of warning levels that occurred (e.g., "1-2-3", "1-2", "1").
        Returns None if no warning levels were triggered.
        """
        lvls = []
        lvl_1 = self.fca_t_warning_idx(data, 1, context, log)
        lvl_2 = self.fca_t_warning_idx(data, 2, context, log)
        lvl_3 = self.fca_t_warning_idx(data, 3, context, log)
        if lvl_1 is not None and lvl_1 != NOT_OCCURRED:
            lvls.append("1")
        if lvl_2 is not None and lvl_2 != NOT_OCCURRED:
            lvls.append("2")
        if lvl_3 is not None and lvl_3 != NOT_OCCURRED:
            lvls.append("3")

        return "-".join(lvls) if lvls else None

    @ensure_log
    def fca_collision(self, data: Data, context: ScenarioContext = None, log: Log = None) -> Value:
        """
        -- EuroNCAP --
        Crash Avoidance Frontal Collisions v1.1
        Definitions
        V_impact - means the speed at which the profiled line around the front, rear end, or side of the VUT
        coincides with the virtual box around the test targets (platform not included in the virtual box)
        EPTa, EPTc, EBTa and EMT as shown in the right part of the figures below, as illustrated in
        Figure 0-1 and Figure 0-2.

        -- HMC --

        -- Output --
        Returns True if a collision occurred,
        False if no collision occurred,
        None if collision could not be determined.
        """
        idx = self.fca_t_collision_idx(data, context, log)
        if idx == NOT_OCCURRED:
            return False
        if idx is None:
            log.push("Could not determine collision")
            return None
        return True

    @ensure_log
    def fca_impact_speed(self, data: Data, context: ScenarioContext = None, log: Log = None) -> Value:
        """
        -- EuroNCAP --
        Crash Avoidance Frontal Collisions v1.1
        Definitions
        V_impact - means the speed at which the profiled line around the front, rear end, or side of the VUT
        coincides with the virtual box around the test targets (platform not included in the virtual box)
        EPTa, EPTc, EBTa and EMT as shown in the right part of the figures below, as illustrated in
        Figure 0-1 and Figure 0-2.

        -- HMC --
        Uses vut_long_vel signal directly at collision index.

        -- Output --
        Returns the absolute VUT longitudinal speed at the time of collision.
        If no collision has occurred, returns 0.0.
        If collision cannot be determined, returns None.
        """
        idx = self.fca_t_collision_idx(data, context, log)
        if idx is None:
            log.push("Could not determine collision")
            return None

        if idx == NOT_OCCURRED:
            # Return 0.0 if no collision has occurred
            return 0.0

        _, signal = self.get_signal(data, "vut_long_vel", log)
        if signal is None:
            log.push("Could not retrieve signal 'vut_long_vel'")
            return None

        return abs(float(signal[idx]))

    @ensure_log
    def fca_rel_impact_speed(self, data: Data, context: ScenarioContext = None, log: Log = None) -> Value:
        """
        -- EuroNCAP --
        Crash Avoidance Frontal Collisions v1.1
        Definitions
        V_impact - means the speed at which the profiled line around the front, rear end, or side of the VUT
        coincides with the virtual box around the test targets (platform not included in the virtual box)
        EPTa, EPTc, EBTa and EMT as shown in the right part of the figures below, as illustrated in
        Figure 0-1 and Figure 0-2.
        V_rel_impact  means the relative speed at which the VUT hits the test target (GVT, EPT, EBT or
        EMT) by subtracting the longitudinal velocity of the test target from V_impact at the time of collision.

        -- HMC --
        Instead of retrieving target speed and impact speed and subtracting, directly use signal

        -- Output --
        Returns the absolute relative longitudinal speed at the time of collision.
        If no collision has occurred, returns 0.0.
        If collision cannot be determined, returns None.
        """
        idx = self.fca_t_collision_idx(data, context, log)

        if idx is None:
            log.push("Could not determine collision")
            return None

        if idx == NOT_OCCURRED:
            # Return 0.0 if no collision has occurred
            return 0.0

        _, signal = self.get_signal(data, "rel_long_vel", log)
        if signal is None:
            log.push("Could not retrieve signal 'rel_long_vel'")
            return None

        return abs(float(signal[idx]))

    @ensure_log
    def fca_remaining_distance(self, data: Data, context: ScenarioContext = None, log: Log = None) -> Value:
        """
        -- EuroNCAP --
        No explicit definition for remaining distance calculation method.
        Remaining distance is implicitly used as a KPI in 5.3 Acceptance Criteria with ±1.0m tolerance.

        -- HMC --
        Finds the earliest of two possible test end conditions after deceleration starts:
        1. VUT stop: VUT speed <= 0.1 m/s
        2. Target outspeed: rel_long_vel crosses 0 (target moving away from VUT)

        Returns rel_long_dist at whichever condition occurs first.

        Note: All indices are capped by `last_idx` where rel_long_dist > 0 to avoid
        returning negative distances when signals continue past the test end.

        -- Output --
        Returns the remaining distance to the target at the earliest end condition.
        Returns None if collision occurred, or if deceleration cannot be determined.
        """
        coll_idx = self.fca_t_collision_idx(data, context, log)
        if coll_idx is None:
            log.push("Collision could not be determined")

        if coll_idx != NOT_OCCURRED:
            return None

        _, dist = self.get_signal(data, "rel_long_dist", log)
        _, rel_vel = self.get_signal(data, "rel_long_vel", log)
        _, speed = self.get_signal(data, "vut_long_vel", log)

        if dist is None:
            log.push("Could not retrieve signal 'rel_long_dist'")
            return None

        if rel_vel is None:
            log.push("Could not retrieve signal rel_long_vel")
            return None

        if speed is None:
            log.push("Could not retrieve signal 'vut_long_vel'")
            return None

        last_idx = get_last_index_larger_than(dist, 0.0)
        if last_idx is None:
            log.push("Remaining distance is always negative")
            return None

        decel_idx = self.fca_t_decel_idx(data, context, log)
        if decel_idx is None:
            log.push("VUT deceleration could not be determined")
            return None

        speed_after_decel = speed[decel_idx:]
        rel_vel_after_decel = rel_vel[decel_idx:]
        threshold = 0.1

        # Look for VUT stop
        low_speed_idx = get_first_index_smaller_or_equal_to(speed_after_decel, threshold)
        if low_speed_idx is not None:
            low_speed_idx = min(last_idx, decel_idx + low_speed_idx)

        # Look for rel_vel cross 0
        cross_idx = get_first_index_cross(rel_vel_after_decel, 0.0)
        if cross_idx is not None:
            cross_idx = min(last_idx, decel_idx + cross_idx)

        valid_indices = [idx for idx in [low_speed_idx, cross_idx] if idx is not None]

        return float(dist[min(valid_indices)]) if valid_indices else None

    @ensure_log
    def fca_ttc_iso(self, data: Data, context: ScenarioContext = None, log: Log = None) -> Value:
        """
        -- EuroNCAP --
        Safe Driving & Crash Avoidance Virtual Testing v1.0
        5.2.1 ISO Score
        The ISO score calculation will only take into account the time series data from T_aeb-0.2s until T_end as defined in the physical test protocols

        -- HMC --
        Returns TTC value at t_aeb index directly.
        """
        _, ttc_signal = self.get_ttc(data, context=context, log=log)
        if ttc_signal is None:
            log.push("TTC could not be calculated")
            return None

        t_aeb_idx = self.fca_t_aeb_idx(data, context, log)
        if t_aeb_idx is None:
            log.push("t_aeb could not be determined")
            return None

        return float(ttc_signal[t_aeb_idx])

    @ensure_log
    def fca_ttc_at_warning(self, data: Data, level: int, context: ScenarioContext = None, log: Log = None) -> Value:
        """
        -- EuroNCAP --
        CA 004 Data Acquisition and Assessment Criteria Calculation v1.1
        3.1.5 FCW Time-to-Collision
        The Time-to-Collision of FCW is calculated with the following formula:
        TTC_FCW = TTC(T_FCW)
        With:
        TTC Time-to-Collision
        T_FCW Time of FCW initiation

        -- HMC --
        FCW warning levels are 1 to 3.
        """
        idx = self.fca_t_warning_idx(data, level, context, log)
        if idx is None:
            log.push("t_warning could not be determined")
            return None

        if idx == NOT_OCCURRED:
            return None

        _, signal = self.get_ttc(data, context=context, log=log)
        if signal is None:
            log.push("TTC could not be calculated")
            return None

        return float(signal[idx])

    @ensure_log
    def fca_get_indices(self, data: Data, context: ScenarioContext, log: Log = None) -> tuple:
        """
        -- EuroNCAP --
        Crash Avoidance Frontal Collisions v1.1
        1.5.1 Variables
        4.3.2 AEB tests

        -- HMC --

        -- Output --
        Returns a tuple of (indices, err)
        """
        t_0_idx = self.fca_t_0_idx(data, context, log)
        if t_0_idx is None:
            log.push("Could not determine test start time (T_0)")
            return None, "no valid T_0"

        t_end_idx = self.fca_t_end_idx(data, context, log)
        if t_end_idx is None:
            log.push("Could not determine test end time (T_end)")
            return None, "no valid T_end"

        t_aeb_idx = self.fca_t_aeb_idx(data, context, log)
        if t_aeb_idx is None:
            log.push("Could not determine AEB activation time")
            return None, "no valid AEB activation time"

        t_fcw_idx = self.fca_t_warning_idx(data, 1, context, log)
        if t_fcw_idx is None:
            log.push("Could not determine FCW warning time")
            return None, "no valid FCW warning time"

        t_steer_start_idx = None
        t_steer_end_idx = None
        if context.cluster == "Turning":
            t_steer_start_idx = self.fca_t_steer_start_idx(data, context, log)
            if t_steer_start_idx is None:
                log.push("Could not determine steering start time")
                return None, "no valid steering start time"

            t_steer_end_idx = self.fca_t_steer_end_idx(data, context, log)
            if t_steer_end_idx is None:
                log.push("Could not determine steering end time")
                return None, "no valid steering end time"

        return (t_0_idx, t_end_idx, t_aeb_idx, t_fcw_idx, t_steer_start_idx, t_steer_end_idx), None

    @ensure_log
    def fca_validate_vut_long_vel(self, data: Data, indices: tuple, context: ScenarioContext, log: Log = None) -> tuple:
        """
        -- EuroNCAP --
        Crash Avoidance Frontal Collisions v1.1
        4.3.2 AEB tests
        Accelerate the VUT and target to the respective test speeds where needed. The test shall start at T_0 and is
        valid when all boundary conditions are met between T_0 and T_AEB, T_FCW, or any other system intervention - whichever comes/occurs first

        -- HMC --
        Use the minimum of t_end, t_aeb, t_fcw as x_max
        -- Output --
        Returns a tuple of (boundaries, err)
        """
        _, vut_long_vel = self.get_signal(data, "vut_long_vel", log)
        if vut_long_vel is None:
            log.push("Could not retrieve signal 'vut_long_vel'")
            return None, "No VUT speed"
        x_min_idx = indices[0]
        x_max_idx = min(indices[1:4])
        y_min = (context.vut_speed) / 3.6
        y_max = (context.vut_speed + 1) / 3.6

        return (vut_long_vel, x_min_idx, x_max_idx, y_min, y_max), None

    @ensure_log
    def fca_validate_vut_lat_dev(self, data: Data, indices: tuple, context: ScenarioContext, log: Log = None) -> tuple:
        """
        -- EuroNCAP --
        Crash Avoidance Frontal Collisions v1.1
        4.3.2 AEB tests
        Accelerate the VUT and target to the respective test speeds where needed. The test shall start at T_0 and is
        valid when all boundary conditions are met between T_0 and T_AEB, T_FCW, or any other system intervention - whichever comes/occurs first

        -- HMC --
        Use t_steer_start and t_steer_end as x_min and x_max, respectively, for Turning scenarios
        Use the minimum of t_end, t_aeb, t_fcw as x_max for all others

        -- Output --
        Returns a tuple of (boundaries, err)
        """
        _, vut_lat_dev = self.get_signal(data, "vut_lat_dev", log)
        if vut_lat_dev is None:
            log.push("Could not retrieve signal 'vut_lat_dev'")
            return None, "No VUT lateral deviation"

        if context.cluster == "Turning":
            threshold = 0.1
            x_min_idx = indices[4]
            x_max_idx = indices[5]
        else:
            threshold = 0.05
            x_min_idx = indices[0]
            x_max_idx = min(indices[1:4])

        y_min = vut_lat_dev[x_min_idx] - threshold
        y_max = vut_lat_dev[x_min_idx] + threshold

        return (vut_lat_dev, x_min_idx, x_max_idx, y_min, y_max), None

    @ensure_log
    def fca_validate_vut_yaw_vel(self, data: Data, indices: tuple, context: ScenarioContext, log: Log = None) -> tuple:
        """
        -- EuroNCAP --
        Crash Avoidance Frontal Collisions v1.1
        4.3.2 AEB tests
        Accelerate the VUT and target to the respective test speeds where needed. The test shall start at T_0 and is
        valid when all boundary conditions are met between T_0 and T_AEB, T_FCW, or any other system intervention - whichever comes/occurs first

        -- HMC --
        Use t_steer_start for x_max for Turning scenarios
        Use the minimum of t_end, t_aeb, t_fcw as x_max for all others

        -- Output --
        Returns a tuple of (boundaries, err)
        """
        _, vut_yaw_vel = self.get_signal(data, "vut_yaw_vel", log)
        if vut_yaw_vel is None:
            log.push("Could not retrieve signal 'vut_yaw_vel'")
            return None, "No VUT yaw velocity"

        x_min_idx = indices[0]
        if context.cluster == "Turning":
            x_max_idx = indices[4]
        else:
            x_max_idx = min(indices[1:4])
        y_min = -1 * np.pi / 180
        y_max = +1 * np.pi / 180

        return (vut_yaw_vel, x_min_idx, x_max_idx, y_min, y_max), None

    @ensure_log
    def fca_validate_vut_steer_whl_vel(self, data: Data, indices: tuple, context: ScenarioContext, log: Log = None) -> tuple:
        """
        -- EuroNCAP --
        Crash Avoidance Frontal Collisions v1.1
        4.3.2 AEB tests
        Accelerate the VUT and target to the respective test speeds where needed. The test shall start at T_0 and is
        valid when all boundary conditions are met between T_0 and T_AEB, T_FCW, or any other system intervention - whichever comes/occurs first

        -- HMC --
        Use t_steer_start for x_max for Turning scenarios
        Use the minimum of t_end, t_aeb, t_fcw as x_max for all others

        -- Output --
        Returns a tuple of (boundaries, err)
        """
        _, vut_steer_whl_vel = self.get_signal(data, "vut_steer_whl_vel", log)
        if vut_steer_whl_vel is None:
            log.push("Could not retrieve signal 'vut_steer_whl_vel'")
            return None, "No VUT steering wheel velocity"

        x_min_idx = indices[0]
        if context.cluster == "Turning":
            x_max_idx = indices[4]
        else:
            x_max_idx = min(indices[1:4])
        y_min = -15 * np.pi / 180
        y_max = +15 * np.pi / 180

        return (vut_steer_whl_vel, x_min_idx, x_max_idx, y_min, y_max), None

    @ensure_log
    def fca_validate_target_long_vel(self, data: Data, indices: tuple, context: ScenarioContext, log: Log = None) -> tuple:
        """
        -- EuroNCAP --
        Crash Avoidance Frontal Collisions v1.1
        4.3.2 AEB tests
        Accelerate the VUT and target to the respective test speeds where needed. The test shall start at T_0 and is
        valid when all boundary conditions are met between T_0 and T_AEB, T_FCW, or any other system intervention - whichever comes/occurs first

        -- HMC --
        Use the minimum of t_end, t_aeb, t_fcw as x_max

        -- Output --
        Returns a tuple of (boundaries, err)
        """
        _, target_long_vel = self.get_signal(data, "target_long_vel", log)
        if target_long_vel is None:
            log.push("Could not retrieve signal 'target_long_vel'")
            return None, "No target speed"

        if context.scenario_type == "Car-to-Bicyclist":
            speed_range = 0.5
        elif context.scenario_type == "Car-to-Pedestrian":
            speed_range = 0.2
        else:
            speed_range = 1.0

        x_min_idx = indices[0]
        x_max_idx = min(indices[1:4])
        y_min = (context.target_speed - speed_range) / 3.6
        y_max = (context.target_speed + speed_range) / 3.6

        return (target_long_vel, x_min_idx, x_max_idx, y_min, y_max), None

    @ensure_log
    def fca_validate_target_lat_dev(self, data: Data, indices: tuple, context: ScenarioContext, log: Log = None) -> tuple:
        """
        -- EuroNCAP --
        Crash Avoidance Frontal Collisions v1.1
        4.3.2 AEB tests
        Accelerate the VUT and target to the respective test speeds where needed. The test shall start at T_0 and is
        valid when all boundary conditions are met between T_0 and T_AEB, T_FCW, or any other system intervention - whichever comes/occurs first

        -- HMC --
        Use the minimum of t_end, t_aeb, t_fcw as x_max

        -- Output --
        Returns a tuple of (boundaries, err)
        """
        _, target_lat_dev = self.get_signal(data, "target_lat_dev", log)
        if target_lat_dev is None:
            log.push("Could not retrieve signal 'target_lat_dev'")
            return None, "No target lateral deviation"

        if context.scenario_type == "Car-to-Car":
            lat_dev_range = 0.1
        elif context.scenario_type == "Car-to-Motorcycle":
            lat_dev_range = 0.15
        else:
            if context.cluster == "Crossing":
                lat_dev_range = 0.05
            else:
                lat_dev_range = 0.15

        x_min_idx = indices[0]
        x_max_idx = min(indices[1:4])
        y_min = target_lat_dev[x_min_idx] - lat_dev_range
        y_max = target_lat_dev[x_min_idx] + lat_dev_range

        return (target_lat_dev, x_min_idx, x_max_idx, y_min, y_max), None

    @ensure_log
    def fca_validate_target_lat_vel(self, data: Data, indices: tuple, context: ScenarioContext = None, log: Log = None) -> tuple:
        """
        -- EuroNCAP --
        Crash Avoidance Frontal Collisions v1.1
        4.3.2 AEB tests
        Accelerate the VUT and target to the respective test speeds where needed. The test shall start at T_0 and is
        valid when all boundary conditions are met between T_0 and T_AEB, T_FCW, or any other system intervention - whichever comes/occurs first

        -- HMC --
        Use the minimum of t_end, t_aeb, t_fcw as x_max

        -- Output --
        Returns a tuple of (boundaries, err)
        """
        _, target_lat_vel = self.get_signal(data, "target_lat_vel", log)
        if target_lat_vel is None:
            log.push("Could not retrieve signal 'target_lat_vel'")
            return None, "No target lateral velocity"

        x_min_idx = indices[0]
        x_max_idx = min(indices[1:4])
        y_min = -0.15
        y_max = +0.15

        return (target_lat_vel, x_min_idx, x_max_idx, y_min, y_max), None

    @ensure_log
    def fca_validate_scenario(self, data: Data, context: ScenarioContext, log: Log = None) -> tuple[bool, str]:
        """
        -- EuroNCAP --
        Crash Avoidance Frontal Collisions v1.1
        4.3.2 AEB tests
        +---------------------+---------------------+---------------------+---------------------+---------------------+---------------------+
        |                     | VUT                 | GVT                 | EPT                 | EBT                 | EMT                 |
        +---------------------+---------------------+---------------------+---------------------+---------------------+---------------------+
        | Speed               | + 1.0 km/h          | ± 1.0 km/h          | ± 0.2 km/h          | ± 0.5 km/h          | ± 1.0 km/h          |
        +---------------------+---------------------+---------------------+---------------------+---------------------+---------------------+
        | Lateral deviation   | 0 ± 0.05 m          | 0 ± 0.10 m          | 0 ± 0.05 m for      |                     | 0 ± [0.15] m        |
        |                     | (0 ± 0.10 m during  |                     | crossing scenarios  |                     |                     |
        |                     | the turn in turning |                     | (incl. junction)    |                     |                     |
        |                     | scenarios)          |                     |                     |                     |                     |
        |                     |                     |                     | 0 ± 0.15 m for      |                     |                     |
        |                     |                     |                     | longitudinal        |                     |                     |
        |                     |                     |                     | scenarios           |                     |                     |
        +---------------------+---------------------+---------------------+---------------------+---------------------+---------------------+
        | Lateral velocity    |                     |                     | 0 ± 0.15 m/s        | 0 ± 0.15 m/s        |                     |
        +---------------------+---------------------+---------------------+---------------------+---------------------+---------------------+
        | Relative distance   |                     | 1.0 sec [+0.1sec]   |                     |                     | 1.0 sec [+0.1sec]   |
        |                     |                     | time gap            |                     |                     | time gap            |
        +---------------------+---------------------+---------------------+---------------------+---------------------+---------------------+
        | Yaw velocity        | 0 ± 1.0 °/s         |                     |                     |                     |                     |
        | (up to T_STEER)     |                     |                     |                     |                     |                     |
        +---------------------+---------------------+---------------------+---------------------+---------------------+---------------------+
        | Steering wheel      | 0 ± 15.0 °/s        |                     |                     |                     |                     |
        | velocity            |                     |                     |                     |                     |                     |
        | (up to T_STEER)     |                     |                     |                     |                     |                     |
        +---------------------+---------------------+---------------------+---------------------+---------------------+---------------------+
        | SCP Time Error      | 0 ± 0.1 s           |                     |                     |                     |                     |
        | (only for CCCscp    |                     |                     |                     |                     |                     |
        | & CMCscp)           |                     |                     |                     |                     |                     |
        +---------------------+---------------------+---------------------+---------------------+---------------------+---------------------+
        4.3.3 FCW tests


        -- HMC --
        Relative distance with unit as time(seconds) is interpreted as time headway at target deceleration for braking scenarios
        SCP Time Error is currently not implemented due to missing vehicle geometry in UAQ or CAN signals
        Validation logic for FCW braking has not been implemented

        -- Output --
        Returns a tuple of (validity, reasons_if_not)
        """
        scenario = context.scenario
        scenario_type = context.scenario_type

        # Initialize return values
        reasons = []

        indices, err = self.fca_get_indices(data, context, log)
        if err is not None:
            log.push("Failed to calculate time indices")
            return False, err

        """ VUT """
        # Speed
        rv, err = self.fca_validate_vut_long_vel(data, indices, context, log)
        if err is not None:
            reasons.append(err)
        else:
            if not is_signal_in_range(*rv):
                reasons.append("VUT speed outside range")

        # Lateral deviation
        rv, err = self.fca_validate_vut_lat_dev(data, indices, context, log)
        if err is not None:
            reasons.append(err)
        else:
            if not is_signal_in_range(*rv):
                reasons.append("VUT lateral deviation outside range")

        # Yaw velocity
        rv, err = self.fca_validate_vut_yaw_vel(data, indices, context, log)
        if err is not None:
            reasons.append(err)
        else:
            if not is_signal_in_range(*rv):
                reasons.append("VUT yaw velocity outside range")

        # Steering wheel velocity
        rv, err = self.fca_validate_vut_steer_whl_vel(data, indices, context, log)
        if err is not None:
            reasons.append(err)
        else:
            if not is_signal_in_range(*rv):
                reasons.append("VUT steering wheel velocity outside range")

        """ Target """
        # Speed
        rv, err = self.fca_validate_target_long_vel(data, indices, context, log)
        if err is not None:
            reasons.append(err)
        else:
            if not is_signal_in_range(*rv):
                reasons.append("Target speed outside range")

        # Lateral Deviation
        rv, err = self.fca_validate_target_lat_dev(data, indices, context, log)
        if err is not None:
            reasons.append(err)
        else:
            if not is_signal_in_range(*rv):
                reasons.append("Target lateral deviation outside range")

        # Target Lateral Velocity
        if scenario_type in ["Car-to-Bicyclist", "Car-to-Pedestrian"]:
            rv, err = self.fca_validate_target_lat_vel(data, indices, context, log)
            if err is not None:
                reasons.append(err)
            else:
                if not is_signal_in_range(*rv):
                    reasons.append("Target lateral velocity outside range")

        # Relative Distance
        if scenario in ["CCRb", "CMRb"]:
            _, time_headway = self.get_time_headway(data, context=context, log=log)
            target_decel_idx = self.fca_t_target_decel_start_idx(data, context, log)

            if target_decel_idx is None:
                reasons.append("No target deceleration")
                log.push("Could not retrieve target decel start")
            else:
                if time_headway[target_decel_idx] > 1.1 or time_headway[target_decel_idx] < 1.0:
                    reasons.append("Relative distance outside range")

        # SCP Time Error
        # TODO

        if reasons:
            reason = "\n".join(reasons)
        else:
            reason = None
        return not reasons, reason

    @ensure_log
    def fca_iso_rating(self, erg: ERG, phys: BLF | ASC, context: ScenarioContext = None, log: Log = None) -> Value:
        """
        -- EuroNCAP --
        Safe Driving & Crash Avoidance Virtual Testing v1.0
        3.2 Data Filtering
        Data Filtering
        All time series data must be provided with an output frequency of 100 Hz and no additional filtering shall be applied on the output data.
        5.2.1 ISO score
        The ISO score, according to ISO TS 18571, of the longitudinal vehicle acceleration channel
        (10VEHC000000ACXS) is calculated.
        Before calculating the ISO score, the time channel for the virtual test is time-shifted so that Taeb
        is aligned between the physical test and the simulation.
        The ISO score calculation will only take into account the time series data from Taeb-0.2s until
        Tend as defined in the physical test protocols. The Tend from physical or virtual test is to be used,
        whichever is the earliest.
        ISO/TS 18571
        Road vehicles - Objective rating metric for non-ambiguous signals
        Second edition 2024-05


        -- HMC --
        Time-based alignment: ERG time channel is shifted by (phys_Taeb - erg_Taeb) so AEB activation
        aligns between physical and virtual tests. The analysis window uses real time bounds rather than
        index offsets, which is more correct when ERG and physical data have different time bases.

        PHYS acceleration is read at native frequency (get_signal_raw) to preserve the original 100Hz
        sampling rate per EuroNCAP spec. ERG uses get_signal (interpolated to FREQ).
        When PHYS Taeb cannot be determined via fca_t_aeb_idx, a fallback estimation from Tend is used.
        """
        def _safe_at(arr, idx, fmt=None):
            if arr is None or idx is None:
                return "N/A" if fmt else None
            idx = int(idx)
            if idx < 0 or idx >= len(arr):
                return "N/A" if fmt else None
            try:
                val = float(arr[idx])
                return f"{val:{fmt}}" if fmt else val
            except Exception:
                return "N/A" if fmt else None

        # --- ERG: interpolated signals ---
        erg_time, erg_acc = self.get_signal(erg, "vut_long_acc", log)
        _, erg_vel = self.get_signal(erg, "vut_long_vel", log)

        if erg_time is None or erg_acc is None:
            log.push("ERG acceleration could not be retrieved for ISO rating")
            return None

        erg_time = np.asarray(erg_time, dtype=float)
        erg_acc = np.asarray(erg_acc, dtype=float)

        # --- PHYS: raw signals (native frequency, no resampling) ---
        phys_time_raw, phys_acc_raw = self.get_signal_raw(phys, "vut_long_acc", log)
        _, phys_vel_raw = self.get_signal_raw(phys, "vut_long_vel", log)

        if phys_time_raw is None or phys_acc_raw is None:
            log.push("[ISO] PHYS raw unavailable -> fallback to interpolated")
            phys_time_raw, phys_acc_raw = self.get_signal(phys, "vut_long_acc", log)
            _, phys_vel_raw = self.get_signal(phys, "vut_long_vel", log)

        if phys_time_raw is None or phys_acc_raw is None:
            log.push("PHYS acceleration could not be retrieved for ISO rating")
            return None

        phys_time_raw = np.asarray(phys_time_raw, dtype=float)
        phys_acc_raw = np.asarray(phys_acc_raw, dtype=float)

        log.push(
            f"[ISO] Signal shapes: "
            f"ERG ({len(erg_time)},) ~{1.0 / np.mean(np.diff(erg_time)):.0f}Hz | "
            f"PHYS ({len(phys_time_raw)},) ~{1.0 / np.mean(np.diff(phys_time_raw)):.0f}Hz"
        )

        # --- ERG Taeb/Tend (interpolated indices -> time) ---
        erg_t_aeb_idx = self.fca_t_aeb_idx(erg, context, log)
        erg_t_end_idx = self.fca_t_end_idx(erg, context, log)

        if erg_t_aeb_idx is None or erg_t_end_idx is None:
            log.push(f"[ISO] ERG indices missing: taeb={erg_t_aeb_idx}, tend={erg_t_end_idx}")
            return None

        erg_t_aeb_idx = int(erg_t_aeb_idx)
        erg_t_end_idx = int(erg_t_end_idx)
        erg_taeb_t = float(erg_time[erg_t_aeb_idx])
        erg_tend_t = float(erg_time[erg_t_end_idx])

        # --- PHYS Tend (interpolated index -> time) ---
        phys_t_end_idx_interp = self.fca_t_end_idx(phys, context, log)
        phys_tend_t = None
        if phys_t_end_idx_interp is not None:
            phys_interp_time, _ = self.get_signal(phys, "vut_long_acc", log)
            if phys_interp_time is not None:
                phys_interp_time = np.asarray(phys_interp_time, dtype=float)
                idx = int(phys_t_end_idx_interp)
                if 0 <= idx < len(phys_interp_time):
                    phys_tend_t = float(phys_interp_time[idx])

        # --- PHYS Taeb (interpolated index -> time -> raw index) ---
        phys_t_aeb_idx_interp = self.fca_t_aeb_idx(phys, context, log)
        phys_taeb_t = None
        phys_taeb_raw_idx = None

        if phys_t_aeb_idx_interp is not None:
            phys_interp_time, _ = self.get_signal(phys, "vut_long_acc", log)
            if phys_interp_time is not None:
                phys_interp_time = np.asarray(phys_interp_time, dtype=float)
                idx = int(phys_t_aeb_idx_interp)
                if 0 <= idx < len(phys_interp_time):
                    phys_taeb_t = float(phys_interp_time[idx])
            if phys_taeb_t is not None:
                phys_taeb_raw_idx = int(np.searchsorted(phys_time_raw, phys_taeb_t, side="right") - 1)
                phys_taeb_raw_idx = max(0, min(phys_taeb_raw_idx, len(phys_time_raw) - 1))
        else:
            # PHYS Taeb unavailable -> estimate from raw acc using Tend
            if phys_tend_t is not None:
                log.push(f"[ISO] PHYS Taeb=None, Tend={phys_tend_t:.4f}s -> estimating from raw acc")
                phys_taeb_raw_idx = self._estimate_phys_t_aeb_idx_from_tend(
                    phys_time_raw, phys_acc_raw, phys_tend_t, log
                )
                if phys_taeb_raw_idx is not None:
                    phys_taeb_t = float(phys_time_raw[int(phys_taeb_raw_idx)])

        # --- PHYS Tend raw index ---
        phys_tend_raw_idx = None
        if phys_tend_t is not None:
            phys_tend_raw_idx = int(np.searchsorted(phys_time_raw, phys_tend_t, side="right") - 1)
            phys_tend_raw_idx = max(0, min(phys_tend_raw_idx, len(phys_time_raw) - 1))

        if phys_taeb_t is None or phys_tend_t is None:
            log.push(f"[ISO] Missing PHYS times: taeb_t={phys_taeb_t}, tend_t={phys_tend_t}")
            return None

        # --- guard: t_end must be after t_aeb ---
        if erg_tend_t <= erg_taeb_t:
            log.push(f"[ISO] ERG tend <= taeb ({erg_tend_t:.4f} <= {erg_taeb_t:.4f})")
            return 0.0
        if phys_tend_t <= phys_taeb_t:
            log.push(f"[ISO] PHYS tend <= taeb ({phys_tend_t:.4f} <= {phys_taeb_t:.4f})")
            return 0.0

        # --- diagnostic logging ---
        log.push(
            f"[ISO IDX] ERG: Taeb=idx{erg_t_aeb_idx} t={erg_taeb_t:.4f}s | "
            f"Tend=idx{erg_t_end_idx} t={erg_tend_t:.4f}s"
        )
        log.push(
            f"[ISO IDX] PHYS: Taeb=raw_idx{phys_taeb_raw_idx} t={phys_taeb_t:.4f}s | "
            f"Tend=raw_idx{phys_tend_raw_idx} t={phys_tend_t:.4f}s"
        )
        log.push(
            f"[ISO ERG] Taeb: t={erg_taeb_t:.4f}s "
            f"v={_safe_at(erg_vel, erg_t_aeb_idx, '.2f')}m/s "
            f"a={_safe_at(erg_acc, erg_t_aeb_idx, '.4f')}m/s² | "
            f"Tend: t={erg_tend_t:.4f}s "
            f"v={_safe_at(erg_vel, erg_t_end_idx, '.2f')}m/s "
            f"a={_safe_at(erg_acc, erg_t_end_idx, '.4f')}m/s²"
        )
        log.push(
            f"[ISO PHY] Taeb: t={phys_taeb_t:.4f}s "
            f"v={_safe_at(phys_vel_raw, phys_taeb_raw_idx, '.2f')}m/s "
            f"a={_safe_at(phys_acc_raw, phys_taeb_raw_idx, '.4f')}m/s² | "
            f"Tend: t={phys_tend_t:.4f}s "
            f"v={_safe_at(phys_vel_raw, phys_tend_raw_idx, '.2f')}m/s "
            f"a={_safe_at(phys_acc_raw, phys_tend_raw_idx, '.4f')}m/s²"
        )

        # --- time-based Taeb alignment ---
        shift_s = phys_taeb_t - erg_taeb_t
        erg_time_aligned = erg_time + shift_s
        t_start_s = phys_taeb_t - 0.2
        erg_tend_aligned = erg_tend_t + shift_s
        t_end_s = min(phys_tend_t, erg_tend_aligned)

        log.push(
            f"[ISO] Taeb align: shift={shift_s:.4f}s | "
            f"window=[{t_start_s:.4f}, {t_end_s:.4f}]"
        )

        if t_end_s <= t_start_s:
            log.push(f"[ISO] Invalid window: end({t_end_s:.4f}) <= start({t_start_s:.4f})")
            return 0.0

        # --- time-based window cut ---
        erg_mask = (erg_time_aligned >= t_start_s) & (erg_time_aligned <= t_end_s)
        phys_mask = (phys_time_raw >= t_start_s) & (phys_time_raw <= t_end_s)

        erg_time_w = erg_time_aligned[erg_mask]
        erg_acc_w = erg_acc[erg_mask]
        phys_time_w = phys_time_raw[phys_mask]
        phys_acc_w = phys_acc_raw[phys_mask]

        if len(erg_time_w) < 2 or len(phys_time_w) < 2:
            log.push(f"[ISO] Not enough samples: ERG={len(erg_time_w)}, PHYS={len(phys_time_w)}")
            return 0.0

        log.push(f"[ISO] After window cut: ERG={len(erg_time_w)} samples | PHYS={len(phys_time_w)} samples")

        # --- resample to 100 Hz ---
        erg_time_w, erg_acc_w = interpolate(erg_time_w, erg_acc_w, 100)
        phys_time_w, phys_acc_w = interpolate(phys_time_w, phys_acc_w, 100)

        erg_time_w, erg_acc_w, phys_time_w, phys_acc_w = truncate_signals(
            erg_time_w, erg_acc_w, phys_time_w, phys_acc_w
        )

        if len(erg_time_w) > 0 and len(phys_time_w) > 0:
            log.push(
                f"[ISO] After 100Hz resample & truncate: "
                f"ERG [{erg_time_w[0]:.4f}..{erg_time_w[-1]:.4f}] | "
                f"PHYS [{phys_time_w[0]:.4f}..{phys_time_w[-1]:.4f}] | "
                f"len={len(erg_time_w)}"
            )

        erg_data = np.column_stack((erg_time_w, erg_acc_w))
        phys_data = np.column_stack((phys_time_w, phys_acc_w))

        if erg_data.shape != phys_data.shape:
            log.push(f"[ISO] Shape mismatch: ERG={erg_data.shape} PHYS={phys_data.shape}")
            return 0.0

        if 0 in erg_data.shape:
            log.push("Empty array - one or more dimensions are zero")
            return 0.0

        # --- ISO18571 ---
        try:
            iso_obj = ISO18571(reference_curve=phys_data, comparison_curve=erg_data)

            try:
                overall = iso_obj.overall_rating(ndigits=-1)
            except Exception as e:
                overall = None
                log.push(f"ISO18571 overall_rating() failed: {e}")
                log.push(traceback.format_exc())

            try:
                corridor = iso_obj.corridor_rating(ndigits=-1)
            except Exception as e:
                corridor = None
                log.push(f"ISO18571 corridor_rating() failed: {e}")
                log.push(traceback.format_exc())

            try:
                phase = iso_obj.phase_rating(ndigits=-1)
            except Exception as e:
                phase = None
                log.push(f"ISO18571 phase_rating() failed: {e}")
                log.push(traceback.format_exc())

            try:
                magnitude = iso_obj.magnitude_rating(ndigits=-1)
            except Exception as e:
                magnitude = None
                log.push(f"ISO18571 magnitude_rating() failed: {e}")
                log.push(traceback.format_exc())

            try:
                slope = iso_obj.slope_rating(ndigits=-1)
            except Exception as e:
                slope = None
                log.push(f"ISO18571 slope_rating() failed: {e}")
                log.push(traceback.format_exc())

            log.push(f"ISO18571 breakdown\noverall={overall}\ncorridor={corridor}\nphase={phase}\nmagnitude={magnitude}\nslope={slope}")

            return 0.0 if overall is None else overall

        except Exception as e:
            log.push(f"[ISO] Calculation error: {e}")
            log.push(traceback.format_exc())
            return 0.0

    @ensure_log
    def fca_comparison_kpi(self, erg: ERG, phys: BLF | ASC, context: ScenarioContext, log: Log = None) -> Value:
        """
        -- EuroNCAP --
        Safe Driving & Crash Avoidance Virtual Testing v1.0
        5.2.2 Key Performance Indicator (KPI)
        For each applicable KPI, the error between physical test and simulation are calculated:
        KPI_error = KPI_physical - KPI_simulation
        5.3 Acceptance Criteria
        Different acceptance criteria exist for the scenarios within the scenario clusters:
        - Frontal - Longitudinal: CCR, CMR, CPLA, CBLA, CCF
        - Frontal - Turning: CCFtap, CMFtap, CPTA, CBTA
        - Frontal - Crossing: CCC, CMC, CPC, CBC
        +------------------------+--------------------------+--------------------+
        | Scenario cluster       | KPI_error                | Accepted KPI_error |
        +------------------------+--------------------------+--------------------+
        | Frontal - Longitudinal | TTC_AEB [s]              | [+-0.2]            |
        |                        | TTC_FCW [s]              | [+-0.2]            |
        |                        | Remaining distance [m]   | [+-1.0]            |
        |                        | Impact speed [m/s]       | [+-1.0]            |
        +------------------------+--------------------------+--------------------+
        | Frontal - Turning      | TTC_AEB [s]              | [+-0.2]            |
        |                        | TTC_FCW [s]              | [+-0.2]            |
        |                        | Impact speed [m/s]       | [+-1.0]            |
        +------------------------+--------------------------+--------------------+
        | Frontal - Crossing     | TTC_AEB [s]              | [+-0.25]           |
        |                        | TTC_FCW [s]              | [+-0.5]            |
        |                        | Impact speed [m/s]       | [+-1.0]            |
        +------------------------+--------------------------+--------------------+

        -- HMC --
        KPI comparison logic varies by cluster and function_type (AEB/FCW).
        For Longitudinal without collision: compares remaining distance instead of impact speed.
        """
        cluster = context.cluster
        function_type = context.function_type
        _, erg_dist = self.get_signal(erg, "rel_long_dist", log)
        _, erg_vel = self.get_signal(erg, "rel_long_vel", log)
        _, phys_dist = self.get_signal(phys, "rel_long_dist", log)
        _, phys_vel = self.get_signal(phys, "rel_long_vel", log)
        if erg_dist is None:
            log.push("ERG distance signal could not be retrieved")
            return False
        if erg_vel is None:
            log.push("ERG velocity signal could not be retrieved")
            return False
        if phys_dist is None:
            log.push("BLF/ASC distance signal could not be retrieved")
            return False
        if phys_vel is None:
            log.push("BLF/ASC velocity signal could not be retrieved")
            return False

        # Check if collision can be determined
        erg_collision = self.fca_collision(erg, context, log)
        phys_collision = self.fca_collision(phys, context, log)
        if erg_collision is None:
            log.push("ERG collision cannot be determined")
            return False
        if phys_collision is None:
            log.push("BLF/ASC collision cannot be determined")
            return False

        # Get impact speed (common for both AEB and FCW)
        erg_impact_speed = self.fca_impact_speed(erg, context, log)
        phys_impact_speed = self.fca_impact_speed(phys, context, log)

        # Get TTC signal (common for both AEB and FCW)
        _, erg_ttc = self.get_ttc(erg, context=context, log=log)
        _, phys_ttc = self.get_ttc(phys, context=context, log=log)
        if erg_ttc is None:
            log.push("ERG TTC signal could not be retrieved")
            return False
        if phys_ttc is None:
            log.push("BLF/ASC TTC signal could not be retrieved")
            return False
        if function_type == "AEB":
            erg_aeb_idx = self.fca_t_aeb_idx(erg, context, log)
            phys_aeb_idx = self.fca_t_aeb_idx(phys, context, log)
            if erg_aeb_idx is None:
                log.push("ERG AEB index could not be retrieved")
                return False
            if phys_aeb_idx is None:
                log.push("BLF/ASC AEB index could not be retrieved")
                return False
        elif function_type == "FCW":
            erg_fcw_idx = self.fca_t_warning_idx(erg, 1, context, log)
            phys_fcw_idx = self.fca_t_warning_idx(phys, 1, context, log)
            if erg_fcw_idx is None or erg_fcw_idx == NOT_OCCURRED:
                log.push("ERG FCW index could not be retrieved or not triggered")
                return False
            if phys_fcw_idx is None or phys_fcw_idx == NOT_OCCURRED:
                log.push("BLF/ASC FCW index could not be retrieved or not triggered")
                return False

        if cluster == "Longitudinal":
            if function_type == "AEB":
                kpi_ttc = compare(erg_ttc[erg_aeb_idx], phys_ttc[phys_aeb_idx], 0.2)
            elif function_type == "FCW":
                kpi_ttc = compare(erg_ttc[erg_fcw_idx], phys_ttc[phys_fcw_idx], 0.2)
            if erg_impact_speed > 0.0 and phys_impact_speed > 0.0:
                # Collision has occured for both tests
                kpi_collision = compare(erg_impact_speed, phys_impact_speed, 1.0)
            else:
                erg_remaining_distance = self.fca_remaining_distance(erg, context, log)
                phys_remaining_distance = self.fca_remaining_distance(phys, context, log)
                if erg_impact_speed > 0.0 or erg_remaining_distance is None:
                    log.push("Collision has only occurred for ERG")
                    return False
                if phys_impact_speed > 0.0 or phys_remaining_distance is None:
                    log.push("Collision has only occurred for BLF/ASC")
                    return False
                kpi_collision = compare(erg_remaining_distance, phys_remaining_distance, 1.0)
        elif cluster == "Turning":
            if function_type == "AEB":
                kpi_ttc = compare(erg_ttc[erg_aeb_idx], phys_ttc[phys_aeb_idx], 0.2)
            elif function_type == "FCW":
                kpi_ttc = compare(erg_ttc[erg_fcw_idx], phys_ttc[phys_fcw_idx], 0.2)
            if erg_impact_speed > 0.0 or phys_impact_speed > 0.0:
                kpi_collision = compare(erg_impact_speed, phys_impact_speed, 1.0)
            else:
                log.push("Unknown error has occured during comparing impact speed")
                return False
        elif cluster == "Crossing":
            if function_type == "AEB":
                kpi_ttc = compare(erg_ttc[erg_aeb_idx], phys_ttc[phys_aeb_idx], 0.25)
            elif function_type == "FCW":
                kpi_ttc = compare(erg_ttc[erg_fcw_idx], phys_ttc[phys_fcw_idx], 0.5)
            if erg_impact_speed > 0.0 or phys_impact_speed > 0.0:
                # Collision has occured for at least one of the tests
                kpi_collision = compare(erg_impact_speed, phys_impact_speed, 1.0)
            else:
                log.push("Unknown error has occured during comparing impact speed")
                return False
        else:
            log.push("Unreachable")
            return False
        return kpi_ttc and kpi_collision

    @ensure_log
    def fca_color_kpi(self, data: Data, context: ScenarioContext, log: Log = None) -> Value:
        """
        -- EuroNCAP --
        Crash Avoidance Frontal Collisions v1.1
        5.2 Criteria
        +---------------+------------+-----------------------+------------------------+
        |               |            |                       Scenarios                |
        |   Criteria    |    KPI     +-----------------------+------------------------+
        |               |            |       Car & PTW       |  Pedestrian & Cyclist  |
        +---------------+------------+-----------------------+------------------------+
        | Mitigation OR | V_rel_imp  | CCRs, CCRm, CCRb      | CPLA(AEB), CBLA(AEB)   |
        | avoidance     |            | CMRs, CMRb            | CPNA, CPFA, CPNCO      |
        |               |            |                       | CBNA, CBFA, CBNAO      |
        +---------------+------------+-----------------------+------------------------+
        | Mitigation    | V_reduction| CCFhos, CCFhol        |           -            |
        +---------------+------------+-----------------------+------------------------+
        | Avoidance     | V_impact   | CCFtap, CMFtap        |       CPTA, CBTA       |
        |               |            | CCCscp, CMCscp        |                        |
        +---------------+---- -------+-----------------------+------------------------+
        | Warning       | FCW TTC    |           -           | CPLA(FCW), CBLA(FCW)   |
        +---------------+------------+-----------------------+------------------------+

        -- HMC --
        kpi_criteria_id determines scoring logic:
        - 1: Based on relative impact speed thresholds varying by VUT speed
        - 2: Based on speed reduction from t_0 to collision
        - 3: Based on whether impact occurred (GREEN if no collision)
        - 4: Based on TTC at FCW (GREEN if TTC >= 1.7s)

        -- Output --
        Returns color string: "GREEN", "YELLOW", "ORANGE", "BROWN", or "RED"
        Returns "RED" if required signals cannot be determined.
        """
        vut_speed = int(context.vut_speed)
        kpi_criteria_id = context.kpi_criteria_id
        # Get speed signal using signal map

        if kpi_criteria_id == 1:
            rel_impact_speed = self.fca_rel_impact_speed(data, context, log)
            if rel_impact_speed is None:
                log.push("Defaulting to RED due to missing relative impact speed")
                return "RED"

            rel_impact_speed = rel_impact_speed * 3.6
            if rel_impact_speed <= 0.0:
                return "GREEN"

            if vut_speed <= 20:
                return "RED"

            if vut_speed > 40:
                if rel_impact_speed > 30:
                    return "RED"
                if rel_impact_speed > 20:
                    return "BROWN"
                if rel_impact_speed > 10:
                    return "ORANGE"
                if rel_impact_speed > 0:
                    return "YELLOW"
                else:
                    return "GREEN"
            if vut_speed > 30:
                if rel_impact_speed > 20:
                    return "RED"
                if rel_impact_speed > 10:
                    return "BROWN"
                if rel_impact_speed > 0:
                    return "ORANGE"
                else:
                    return "GREEN"
            if vut_speed > 20:
                if rel_impact_speed > 10:
                    return "RED"
                if rel_impact_speed > 0:
                    return "BROWN"
                else:
                    return "GREEN"

        elif kpi_criteria_id == 2:
            t_0_idx = self.fca_t_0_idx(data, context, log)
            collision_idx = self.fca_t_collision_idx(data, context, log)
            if t_0_idx is None:
                log.push("Defaulting to RED due to missing t_0 index")
                return "RED"
            if collision_idx is None:
                log.push("Defaulting to RED due to missing collision index")
                return "RED"
            if collision_idx == NOT_OCCURRED:
                # Return GREEN if no collision occurred
                return "GREEN"
            _, speed = self.get_signal(data, "vut_long_vel", log)
            if speed is None:
                log.push("Defaulting to RED due to missing VUT speed signal")
                return "RED"
            speed_reduction = (speed[t_0_idx] - speed[collision_idx]) * 3.6
            if vut_speed > 30:
                upper_threshold = 35
            else:
                upper_threshold = 30
            if speed_reduction > upper_threshold:
                return "RED"
            if speed_reduction > 20:
                return "GREEN"
            if speed_reduction > 10:
                return "ORANGE"

            return "RED"

        elif kpi_criteria_id == 3:
            impact_speed = self.fca_impact_speed(data, context, log)
            if impact_speed is None:
                log.push("Defaulting to RED due to missing impact speed")
                return "RED"
            if impact_speed == 0.0:
                return "GREEN"
            return "RED"

        elif kpi_criteria_id == 4:
            _, ttc = self.get_ttc(data, context=context, log=log)
            if ttc is None:
                log.push("Defaulting to RED due to missing TTC")
                return "RED"
            fcw_idx = self.fca_t_warning_idx(data, 1, context, log)
            if fcw_idx is None:
                log.push("Defaulting to RED due to missing FCW")
                return "RED"
            if fcw_idx == NOT_OCCURRED:
                return "RED"
            return "GREEN" if ttc[fcw_idx] >= 1.7 else "RED"
        else:
            log.push(f"Unknown KPI Criteria for {context.scenario}")
            return "RED"

    @ensure_log
    def elk_t_0_idx(self, data: Data, context: ScenarioContext = None, log: Log = None) -> Index:
        """
        -- EuroNCAP --
        Crash Avoidance Lane Departure Collisions v1.1
        1.5.1 Variables
        T_0: Time of test start T_0 = time where manoeuvre starts with 2s straight path

        -- HMC --
        Define start of 2s straight path as 2s before steering
        """
        steer_idx = self.elk_t_steer_start_idx(data, context, log)
        if steer_idx is None:
            log.push("Could not determine t_steer")
            return None
        _2000ms = int(2 * self.get_frequency())
        return max(0, steer_idx - _2000ms)

    @ensure_log
    def elk_t_end_idx(self, data: Data, context: ScenarioContext, log: Log = None) -> Index:
        """
        -- EuroNCAP --
        Crash Avoidance Lane Departure Collisions v1.1
        1.5.1 Variables
        T_end: Time of test end (see 4.3.2 Test Scenarios)
        4.3.2 Test Scenarios
        The end of a test is considered as:
        - LDW tests: when the warning commences.
        - BSM tests: when the longitudinal distance between the VUT and test target is 0m
        (i.e. when the front end of the VUT is aligned with the rear end of the test target).
        - LKA/ELK Road Edge tests: 2 seconds after one of the following occurs:
            o The LKA/ELK system fails to maintain the VUT within the permitted lane departure distance.
            o The LKA/ELK system intervenes to maintain the VUT within permitted
              lane departure distance, such that a maximum lateral position is
              achieved that subsequently diminishes causing the VUT to turn back
              towards the lane.
        - ELK oncoming or overtaking tests: when one of the following occurs:
            o The ELK system intervenes to prevent a collision between the VUT
              and target vehicle
            o The ELK system has failed to intervene (sufficiently) to prevent
              a collision between the VUT and target vehicle. This can be
              assumed when one of the following occurs:
                - The lateral separation between the VUT and target vehicle
                  equal < 0.3m in the oncoming and overtaking scenario
                - No intervention is observed at a TTC = 0.8s or a TTC
                  submitted by the OEM
        It is at the labs discretion to select and use one of the options above
        to ensure a safe testing environment. If the test ends because the
        vehicle has failed to intervene (sufficiently) or if the EMT has left
        its designated path by more than 0.2m, it is recommended that the VUT
        and/or EMT are steered away from the impact, either manually or by
        reactivating the steering control of the driving robot/EMT.

        -- HMC --
        Not implemented:
        - LDW tests (T_end = when warning commences)
        - BSM tests (T_end = when longitudinal distance = 0m)

        Implementation choices:
        - DTLE threshold of -0.1m for "fails to maintain within permitted lane departure distance"
        - "Maximum lateral position that subsequently diminishes" interpreted via get_last_index_decreasing()
        - TTC hardcoded to 0.8s (OEM-submitted TTC not supported)
        - Only failure conditions checked for OV/OC tests, not successful intervention
        """
        t_0_idx = self.elk_t_0_idx(data, context, log)
        if t_0_idx is None:
            log.push("Could not determine t_0")
            return None

        # --- diagnostic logging ---
        t0_s = None
        if isinstance(data, ERG):
            try:
                t = data["Time"]
                if 0 <= int(t_0_idx) < len(t):
                    t0_s = float(t[int(t_0_idx)])
            except Exception:
                pass

        vy = None
        if isinstance(data, ERG):
            try:
                vy = data["Car.Fr1.vy"]
            except Exception:
                pass

        log.push(
            f"[CHECK_ELK] scenario={context.scenario} "
            f"cluster={context.cluster} "
            f"t0_idx={t_0_idx} t0_s={t0_s}"
        )
        log.push(f"[CHECK_ELK] Car.Fr1.vy: {self._series_fingerprint(vy)}")

        if context.cluster == "Lane Departure":
            # Road Edge Tests
            # 2 seconds after one of the following conditions is met

            _, dtle = self.get_signal(data, "dtle", log)
            if dtle is None:
                log.push("Could not retrieve signal 'dtle'")
                return None
            dtle_t_0 = dtle[t_0_idx:]

            # 1. DTLE < -0.1
            dtle_idx = get_last_index_larger_than(dtle_t_0, -0.1)
            if dtle_idx is not None:
                return dtle_idx + t_0_idx + int(2 * self.get_frequency())

            # 2. DTLE starts approaching 0 after lka or elk activation
            elk_idx = self.elk_t_elk_idx(data, context, log)
            lka_idx = self.elk_t_lka_idx(data, context, log)
            if elk_idx is None and lka_idx is None:
                log.push("Could not determine both ELK and LKA activation")
                return None
            actv_idx = min(idx for idx in [elk_idx, lka_idx] if idx is not None)
            dtle_actv = dtle[actv_idx:]
            dtle_idx = get_last_index_decreasing(dtle_actv)
            if dtle_idx is not None:
                return dtle_idx + actv_idx + int(2 * self.get_frequency())
        else:
            # OV/OC Tests
            # 1. Lateral distance between VUT and target vehicle is less than 0.3m
            _, lat_dist = self.get_signal(data, "rel_lat_dist", log)
            if lat_dist is None:
                log.push("Could not retrieve signal 'rel_lat_dist'")
                return None
            lat_dist = lat_dist[t_0_idx:]
            lat_dist_idx = get_first_index_smaller_than(lat_dist, 0.3)
            if lat_dist_idx is not None:
                return lat_dist_idx + t_0_idx
            # 2. ELK is not activated when TTC crosses 0.8s
            _, elk = self.get_signal(data, "elk", log)
            if elk is None:
                log.push("Could not retrieve signal 'elk'")
                return None
            elk = elk[t_0_idx:]
            elk_idx = get_first_index_larger_than(elk, 0)
            _, ttc = self.get_ttc(data, context=context, log=log)
            if ttc is None:
                log.push("Could not retrieve TTC")
                return None
            ttc = ttc[t_0_idx:]
            ttc_idx = get_first_index_cross(ttc, 0.8)
            if ttc_idx is None:
                log.push("TTC never crosses 0.8s")
                return None

            if elk_idx is None or elk_idx > ttc_idx:
                return ttc_idx + t_0_idx

            # 3. ELK is activated and then deactivated
            elk = elk[elk_idx:]
            elk_deact_idx = get_first_index_equal_to(elk, 0)
            if elk_deact_idx is None:
                log.push("ELK is never deactivated after activation")
                return None
            return elk_deact_idx + elk_idx + t_0_idx

    @ensure_log
    def elk_t_ldw_idx(self, data: Data, context: ScenarioContext = None, log: Log = None) -> Index:
        """
        -- EuroNCAP --
        Crash Avoidance Lane Departure Collisions v1.1
        Definitions
        Lane Departure Warning (LDW) - a warning that is provided automatically by the vehicle in response to the
        vehicle that is about to drift beyond a delineated edge line of the current travel lane.
        Emergency Lane Keeping (ELK) - default ON heading correction that is applied automatically by the vehicle
        in response to the detection of the vehicle that is about to drift beyond a lane boundary
        (i.e. solid lane marking, road edge) and/or into oncoming or overtaking traffic in the adjacent lane.
        1.5.1 Variables
        T_LDW: Time where LDW activates

        -- HMC --
        LDW is OFF when signal is 0
        LDW is ON when signal is 1
        """
        _, ldw = self.get_signal(data, "ldw", log)
        if ldw is None:
            log.push("Could not retrieve signal 'ldw'")
            return None

        return get_first_index_larger_than(ldw, 0)

    @ensure_log
    def elk_t_elk_idx(self, data: Data, context: ScenarioContext = None, log: Log = None) -> Index:
        """
        -- EuroNCAP --
        Crash Avoidance Lane Departure Collisions v1.1
        Definitions
        Emergency Lane Keeping (ELK) - default ON heading correction that is applied automatically by the vehicle
        in response to the detection of the vehicle that is about to drift beyond a lane boundary
        (i.e. solid lane marking, road edge) and/or into oncoming or overtaking traffic in the adjacent lane.

        -- HMC --
        ELK is OFF when signal is 0
        ELK is ON when signal is 1
        """
        _, elk = self.get_signal(data, "elk", log)
        if elk is None:
            log.push("Could not retrieve signal 'elk'")
            return None

        return get_first_index_larger_than(elk, 0)

    @ensure_log
    def elk_t_lka_idx(self, data: Data, context: ScenarioContext = None, log: Log = None) -> Index:
        """
        -- EuroNCAP --
        Crash Avoidance Lane Departure Collisions v1.1
        Definitions
        Lane Keeping Assist (LKA) - heading correction that is applied automatically by the vehicle in response to the
        detection of the vehicle that is about to drift beyond a delineated edge line of the current travel lane.
        CA 004 Data Acquisition and Assessment Criteria Calculation v1.1
        3.2.3 T_LKA
        T_LKA means the time where the LKA system of the vehicle intervenes. Activation time is
        determined by the following sequence, based on Yaw velocity 𝜓̇
        VUT during the LSS manoeuvre:
        1. Steering robot release is triggered by X position of VUT (green vertical line)
        2. Identify when 𝜓̇
        VUT > 0,4°/s
        3. From point 2., start searching backwards until 𝜓̇
        VUT < 0,1°/s → TLKA (red vertical line)

        -- HMC --
        LKA is OFF when signal is 1
        LKA is ON when signal is 2
        """
        _, lka = self.get_signal(data, "lka", log)
        if lka is None:
            log.push("Could not retrieve signal 'lka'")
            return None

        return get_first_index_larger_than(lka, 1)

    @ensure_log
    def elk_t_steer_start_idx(self, data: Data, context: ScenarioContext = None, log: Log = None) -> Index:
        """
        -- EuroNCAP --
        Crash Avoidance Lane Departure Collisions v1.1
        1.5.1 Variables
        T_steer: Time where the VUT enters in curve segment

        -- HMC --
        The start of the curve segment is defined by global coordinates: x = 710m
        """
        # TODO
        _, tx = self.get_signal(data, "road_tx", log)
        if tx is None:
            log.push("Could not retrieve signal 'road_tx'")
            return None

        threshold = 710
        idx = get_first_index_larger_or_equal_to(tx, threshold)
        if idx is None:
            log.push(f"tx is always smaller than {threshold}")
            return None

        return idx

    @ensure_log
    def elk_dtle(self, data: Data, context: ScenarioContext, log: Log = None) -> Value:
        """
        -- EuroNCAP --
        Crash Avoidance Lane Departure Collisions v1.1
        Definitions
        Distance To Lane Edge (DTLE) - means the remaining lateral distance (perpendicular to the Lane Edge) between
        the Lane Edge and most outer edge of the tyre, before the VUT crosses Lane Edge, assuming the VUT would continue
        to travel with the same lateral velocity towards it.
        5.2.2.1 Road edge
        The limit value for DTLE in Lane Departure - ELK Road Edge tests is set to -0.1m, meaning that the vehicle
        is only allowed to have a part of the front wheel outside of the road edge.
        5.2.2.2 Lane Departure Warning
        Where the test vehicle is unable to meet the assessment and performance criteria for the extended range of
        Lane Departure - Road Edge tests, the vehicle becomes eligible for the assessment of the Lane Departure Warning.
        Any LDW system that issues a haptic warning clearly relating to the
        lateral control of the vehicle noticeable by the driver (e.g. notable
        heading correction, steering wheel vibration, etc.) before a DTLE of
        -0.1m is awarded when active at lateral velocities up to at least
        0.7m/s.
        Safe Driving & Crash Avoidance Virtual Testing v1.0
        5.3 Acceptance Criteria
        Different acceptance criteria exist for the scenarios within the scenario clusters:
        - Frontal - Longitudinal: CCR, CMR, CPLA, CBLA, CCF
        - Frontal - Turning: CCFtap, CMFtap, CPTA, CBTA
        - Frontal - Crossing: CCC, CMC, CPC, CBC
        - Lane - ELK: Road Edge, Oncoming, Overtaking
        +------------------------+--------------------------+--------------------+
        | Scenario cluster       | KPI_error                | Accepted KPI_error |
        +------------------------+--------------------------+--------------------+
        | Frontal - Longitudinal | TTC_AEB [s]              | [+-0.2]            |
        |                        | TTC_FCW [s]              | [+-0.2]            |
        |                        | Remaining distance [m]   | [+-1.0]            |
        |                        | Impact speed [m/s]       | [+-1.0]            |
        +------------------------+--------------------------+--------------------+
        | Frontal - Turning      | TTC_AEB [s]              | [+-0.2]            |
        |                        | TTC_FCW [s]              | [+-0.2]            |
        |                        | Impact speed [m/s]       | [+-1.0]            |
        +------------------------+--------------------------+--------------------+
        | Frontal - Crossing     | TTC_AEB [s]              | [+-0.25]           |
        |                        | TTC_FCW [s]              | [+-0.5]            |
        |                        | Impact speed [m/s]       | [+-1.0]            |
        +------------------------+--------------------------+--------------------+
        | Lane - ELK             | DTLE_ELK [m]             | [+-0.2]            |
        +------------------------+--------------------------+--------------------+

        -- HMC --
        Instead of using the value of DTLE when ELK is flagged on, use the minimum value of DTLE while LKA is flagged on.
        Apply only to ELK Road Edge(Lane Departure) scenarios.
        """
        _, dtle = self.get_signal(data, "dtle", log)
        if dtle is None:
            log.push("Could not retrieve signal 'dtle'")
            return None
        if context.cluster == "Lane Departure":
            # For HMC, DTLE_ELK is defined by the minimum of the dtle signal while LKA is on
            _, lka = self.get_signal(data, "lka", log)
            if lka is None:
                log.push("Could not retrieve signal 'lka'")
                return None
            lka_on_idx = get_first_index_larger_than(lka, 1)
            if lka_on_idx is None:
                log.push("lka is never on")
                return None
            lka = lka[lka_on_idx:]
            dtle = dtle[lka_on_idx:]
            lka_off_idx = get_first_index_smaller_than(lka, 2)
            if lka_off_idx is not None:
                dtle = dtle[:lka_off_idx]
            if len(dtle) == 0:
                log.push(f"DTLE is empty. lka_on_idx:{lka_on_idx} lka_off_idx:{lka_off_idx}")
                return None
            return float(min(dtle))
        else:
            # For HMC, DTLE_ELK is only defined for Lane Departure scenarios
            return None

    @ensure_log
    def elk_get_indices(self, data: Data, context: ScenarioContext = None, log: Log = None) -> tuple:
        """
        -- EuroNCAP --
        Crash Avoidance Lane Departure Collisions v1.1
        1.5.1 Variables
        4.3.2 Test Scenarios

        -- HMC --

        -- Output --
        Returns a tuple of (indices, err)
        """
        t_0_idx = self.elk_t_0_idx(data, context, log)
        if t_0_idx is None:
            log.push("Failed to calculate t_0")
            return None, "No valid t_0"

        t_end_idx = self.elk_t_end_idx(data, context, log)
        if t_end_idx is None:
            log.push("Failed to calculate t_end")
            return None, "No valid t_end"

        t_steer_idx = self.elk_t_steer_start_idx(data, context, log)
        if t_steer_idx is None:
            log.push("Failed to calculate t_steer")
            return None, "No valid t_steer"

        t_elk_idx = self.elk_t_elk_idx(data, context, log)
        if t_elk_idx is None:
            log.push("Failed to calculate t_elk")

        t_lka_idx = self.elk_t_lka_idx(data, context, log)
        if t_lka_idx is None:
            log.push("Failed to calculate t_lka")

        t_ldw_idx = self.elk_t_ldw_idx(data, context, log)
        if t_ldw_idx is None:
            log.push("Failed to calculate t_ldw")

        valid_indices = [idx for idx in [t_elk_idx, t_lka_idx, t_ldw_idx] if idx is not None]
        if not valid_indices:
            return None, "No t_elk/t_lka/t_ldw"

        return (t_0_idx, t_end_idx, t_elk_idx, t_lka_idx, t_ldw_idx, t_steer_idx), None

    @ensure_log
    def elk_validate_dtle(self, data: Data, indices: tuple, context: ScenarioContext = None, log: Log = None) -> tuple:
        """
        -- EuroNCAP --
        Crash Avoidance Lane Departure Collisions v1.1
        Definitions
        Distance To Lane Edge (DTLE) - means the remaining lateral distance (perpendicular to the Lane Edge) between the
        Lane Edge and most outer edge of the tyre, before the VUT crosses Lane Edge, assuming the VUT would continue
        to travel with the same lateral velocity towards it.
        4.3.2 Test Scenarios
        The test shall start at T_0 and is valid when all boundary conditions are met between T_0 and T_ELK/T_LKA/T_LDW
        5.2.2.1 Road edge
        The limit value for DTLE in Lane Departure - ELK Road Edge tests is set to -0.1m, meaning that the vehicle is only
        allowed to have a part of the front wheel outside of the road edge.

        -- HMC --
        Use the minimum of t_end, t_elk, t_ldw, t_lka for x_max

        -- Output --
        Returns a tuple of (boundaries, err)
        """
        _, dtle = self.get_signal(data, "dtle", log)
        if dtle is None:
            log.push("Could not retrieve signal 'dtle'")
            return None, "No DTLE"

        x_min_idx = indices[0]
        valid_indices = [idx for idx in indices[1:5] if idx is not None]
        x_max_idx = min(valid_indices)
        y_min = -0.1
        y_max = INT_MAX
        return (dtle, x_min_idx, x_max_idx, y_min, y_max), None

    @ensure_log
    def elk_validate_vut_long_vel(self, data: Data, indices: tuple, context: ScenarioContext, log: Log = None) -> tuple:
        """
        -- EuroNCAP --
        Crash Avoidance Lane Departure Collisions v1.1
        4.3.2 Test Scenarios
        The test shall start at T_0 and is valid when all boundary conditions are met between T_0 and T_ELK/T_LKA/T_LDW

        -- HMC --
        Use the minimum of t_end, t_elk, t_ldw, t_lka for x_max

        -- Output --
        Returns a tuple of (boundaries, err)
        """
        _, vut_long_vel = self.get_signal(data, "vut_long_vel", log)
        if vut_long_vel is None:
            log.push("Could not retrieve signal 'vut_long_vel'")
            return None, "No VUT speed"

        x_min_idx = indices[0]
        valid_indices = [idx for idx in indices[1:5] if idx is not None]
        x_max_idx = min(valid_indices)
        y_min = (context.vut_speed - 1) / 3.6
        y_max = (context.vut_speed + 1) / 3.6

        return (vut_long_vel, x_min_idx, x_max_idx, y_min, y_max), None

    @ensure_log
    def elk_validate_rel_long_vel(self, data: Data, indices: tuple, context: ScenarioContext = None, log: Log = None) -> tuple:
        """
        -- EuroNCAP --
        Crash Avoidance Lane Departure Collisions v1.1
        4.3.2 Test Scenarios
        The test shall start at T_0 and is valid when all boundary conditions are met between T_0 and T_ELK/T_LKA/T_LDW

        -- HMC --
        Use the minimum of t_end, t_elk, t_ldw, t_lka for x_max

        -- Output --
        Returns a tuple of (boundaries, err)
        """
        _, rel_long_vel = self.get_signal(data, "rel_long_vel", log)
        if rel_long_vel is None:
            log.push("Could not retrieve signal 'rel_long_vel'")
            return None, "No relative longitudinal speed"

        x_min_idx = indices[0]
        valid_indices = [idx for idx in indices[1:5] if idx is not None]
        x_max_idx = min(valid_indices)
        y_min = -1.0
        y_max = +1.0

        return (rel_long_vel, x_min_idx, x_max_idx, y_min, y_max), None

    @ensure_log
    def elk_validate_rel_long_dist(self, data: Data, indices: tuple, context: ScenarioContext = None, log: Log = None) -> tuple:
        """
        -- EuroNCAP --
        Crash Avoidance Lane Departure Collisions v1.1
        4.3.2 Test Scenarios
        The test shall start at T_0 and is valid when all boundary conditions are met between T_0 and T_ELK/T_LKA/T_LDW

        -- HMC --
        Use the minimum of t_end, t_elk, t_ldw, t_lka for x_max

        -- Output --
        Returns a tuple of (boundaries, err)
        """
        _, rel_long_dist = self.get_signal(data, "rel_long_dist", log)
        if rel_long_dist is None:
            log.push("Could not retrieve signal 'rel_long_dist'")
            return None, "No relative longitudinal distance"

        x_min_idx = indices[0]
        valid_indices = [idx for idx in indices[1:5] if idx is not None]
        x_max_idx = min(valid_indices)
        y_min = -0.2
        y_max = +0.2

        return (rel_long_dist, x_min_idx, x_max_idx, y_min, y_max), None

    @ensure_log
    def elk_validate_vut_lat_dev(self, data: Data, indices: tuple, context: ScenarioContext = None, log: Log = None) -> tuple:
        """
        -- EuroNCAP --
        Crash Avoidance Lane Departure Collisions v1.1
        4.3.2 Test Scenarios
        The test shall start at T_0 and is valid when all boundary conditions are met between T_0 and T_ELK/T_LKA/T_LDW

        -- HMC --
        Use the minimum of t_end, t_elk, t_ldw, t_lka for x_max

        -- Output --
        Returns a tuple of (boundaries, err)
        """
        _, vut_lat_dev = self.get_signal(data, "vut_lat_dev", log)
        if vut_lat_dev is None:
            log.push("Could not retrieve signal 'vut_lat_dev'")
            return None, "No VUT lateral deviation"

        x_min_idx = indices[0]
        valid_indices = [idx for idx in indices[1:5] if idx is not None]
        x_max_idx = min(valid_indices)
        y_min = vut_lat_dev[x_min_idx] - 0.05
        y_max = vut_lat_dev[x_min_idx] + 0.05

        return (vut_lat_dev, x_min_idx, x_max_idx, y_min, y_max), None

    @ensure_log
    def elk_validate_vut_lat_vel(self, data: Data, indices: tuple, context: ScenarioContext = None, log: Log = None) -> tuple:
        """
        -- EuroNCAP --
        Crash Avoidance Lane Departure Collisions v1.1
        4.3.2 Test Scenarios
        The test shall start at T_0 and is valid when all boundary conditions are met between T_0 and T_ELK/T_LKA/T_LDW

        -- HMC --
        Use the minimum of t_end, t_elk, t_ldw, t_lka for x_max.
        When vut_lat_vel is unavailable and data is ERG, fall back to Car.Fr1.vy as lateral velocity proxy.

        -- Output --
        Returns a tuple of (boundaries, err)
        """
        _, vut_lat_vel = self.get_signal(data, "vut_lat_vel", log)

        if vut_lat_vel is None and isinstance(data, ERG):
            try:
                t = data["Time"]
                vy = data["Car.Fr1.vy"]
                _, vut_lat_vel = interpolate(t, vy, self.get_frequency(), "step")
                log.push("ELK: fallback 'vut_lat_vel' <- ERG['Car.Fr1.vy']")
            except Exception as e:
                log.push(f"ELK: ERG['Car.Fr1.vy'] fallback failed: {e}")

        if vut_lat_vel is None:
            log.push("Could not retrieve 'vut_lat_vel' nor ERG fallback")
            return None, "No VUT lateral velocity"

        x_min_idx = indices[0]
        valid_indices = [idx for idx in indices[1:5] if idx is not None]
        x_max_idx = min(valid_indices)
        y_min = -0.05
        y_max = +0.05

        return (vut_lat_vel, x_min_idx, x_max_idx, y_min, y_max), None

    @ensure_log
    def elk_validate_vut_yaw_vel(self, data: Data, indices: tuple, context: ScenarioContext = None, log: Log = None) -> tuple:
        """
        -- EuroNCAP --
        Crash Avoidance Lane Departure Collisions v1.1
        4.3.2 Test Scenarios
        The test shall start at T_0 and is valid when all boundary conditions are met between T_0 and T_ELK/T_LKA/T_LDW

        -- HMC --
        Use the t_steer_start for x_max

        -- Output --
        Returns a tuple of (boundaries, err)
        """
        _, vut_yaw_vel = self.get_signal(data, "vut_yaw_vel", log)
        if vut_yaw_vel is None:
            log.push("Could not retrieve signal 'vut_yaw_vel'")
            return None, "No VUT yaw velocity"

        x_min_idx = indices[0]
        x_max_idx = indices[5]
        y_min = -1 * np.pi / 180
        y_max = +1 * np.pi / 180

        return (vut_yaw_vel, x_min_idx, x_max_idx, y_min, y_max), None

    @ensure_log
    def elk_validate_vut_steer_whl_vel(self, data: Data, indices: tuple, context: ScenarioContext = None, log: Log = None) -> tuple:
        """
        -- EuroNCAP --
        Crash Avoidance Lane Departure Collisions v1.1
        4.3.2 Test Scenarios
        The test shall start at T_0 and is valid when all boundary conditions are met between T_0 and T_ELK/T_LKA/T_LDW

        -- HMC --
        Use the t_steer_start for x_max

        -- Output --
        Returns a tuple of (boundaries, err)
        """
        _, vut_steer_whl_vel = self.get_signal(data, "vut_steer_whl_vel", log)
        if vut_steer_whl_vel is None:
            log.push("Could not retrieve signal 'vut_steer_whl_vel'")
            return None, "No VUT steering wheel velocity"

        x_min_idx = indices[0]
        x_max_idx = indices[5]
        y_min = -15 * np.pi / 180
        y_max = +15 * np.pi / 180

        return (vut_steer_whl_vel, x_min_idx, x_max_idx, y_min, y_max), None

    @ensure_log
    def elk_validate_target_long_vel(self, data: Data, indices: tuple, context: ScenarioContext, log: Log = None) -> tuple:
        """
        -- EuroNCAP --
        Crash Avoidance Lane Departure Collisions v1.1
        4.3.2 Test Scenarios
        The test shall start at T_0 and is valid when all boundary conditions are met between T_0 and T_ELK/T_LKA/T_LDW

        -- HMC --
        Use the minimum of t_end, t_elk, t_ldw, t_lka for x_max

        -- Output --
        Returns a tuple of (boundaries, err)
        """
        _, target_long_vel = self.get_signal(data, "target_long_vel", log)
        if target_long_vel is None:
            log.push("Could not retrieve signal 'target_long_vel'")
            return None, "No target longitudinal velocity"

        x_min_idx = indices[0]
        valid_indices = [idx for idx in indices[1:5] if idx is not None]
        x_max_idx = min(valid_indices)
        y_min = (context.target_speed - 1) / 3.6
        y_max = (context.target_speed + 1) / 3.6

        return (target_long_vel, x_min_idx, x_max_idx, y_min, y_max), None

    @ensure_log
    def elk_validate_target_lat_dev(self, data: Data, indices: tuple, context: ScenarioContext, log: Log = None) -> tuple:
        """
        -- EuroNCAP --
        Crash Avoidance Lane Departure Collisions v1.1
        4.3.2 Test Scenarios
        The test shall start at T_0 and is valid when all boundary conditions are met between T_0 and T_ELK/T_LKA/T_LDW

        -- HMC --
        Use the minimum of t_end, t_elk, t_ldw, t_lka for x_max

        -- Output --
        Returns a tuple of (boundaries, err)
        """
        _, target_lat_dev = self.get_signal(data, "target_lat_dev", log)
        if target_lat_dev is None:
            log.push("Could not retrieve signal 'target_lat_dev'")
            return None, "No target lateral deviation"

        if context.scenario_type == "Car-to-Motorcycle":
            lat_dev_range = 0.15
        elif context.cluster == "Oncoming":
            lat_dev_range = 0.3
        else:
            lat_dev_range = 0.2

        x_min_idx = indices[0]
        valid_indices = [idx for idx in indices[1:5] if idx is not None]
        x_max_idx = min(valid_indices)
        y_min = target_lat_dev[x_min_idx] - lat_dev_range
        y_max = target_lat_dev[x_min_idx] + lat_dev_range

        return (target_lat_dev, x_min_idx, x_max_idx, y_min, y_max), None

    @ensure_log
    def elk_validate_target_yaw(self, data: Data, indices: tuple, context: ScenarioContext = None, log: Log = None) -> tuple:
        """
        -- EuroNCAP --
        Crash Avoidance Lane Departure Collisions v1.1
        4.3.2 Test Scenarios
        The test shall start at T_0 and is valid when all boundary conditions are met between T_0 and T_ELK/T_LKA/T_LDW

        -- HMC --
        Use the minimum of t_end, t_elk, t_ldw, t_lka for x_max

        -- Output --
        Returns a tuple of (boundaries, err)
        """
        _, target_yaw = self.get_signal(data, "target_yaw", log)
        if target_yaw is None:
            log.push("Could not retrieve signal 'target_yaw'")
            return None, "No target yaw angle"

        x_min_idx = indices[0]
        valid_indices = [idx for idx in indices[1:5] if idx is not None]
        x_max_idx = min(valid_indices)
        y_min = -1.5 * np.pi / 180
        y_max = +1.5 * np.pi / 180

        return (target_yaw, x_min_idx, x_max_idx, y_min, y_max), None

    @ensure_log
    def elk_validate_scenario(self, data: Data, context: ScenarioContext, log: Log = None) -> tuple[bool, str]:
        """
        -- EuroNCAP --
        Crash Avoidance Lane Departure Collisions v1.1
        4.3.2 Test Scenarios
        +----------------------+------------+--------------+--------------+--------------+
        |      Parameter       | Condition  |     VUT      |     GVT      |     EMT      |
        +----------------------+------------+--------------+--------------+--------------+
        | Speed                |            |            ± 1.0 km/h                      |
        +----------------------+------------+--------------+--------------+--------------+
        | Rel. long. speed     | Overtaking |            ± 1.0 km/h                      |
        +----------------------+------------+--------------+--------------+--------------+
        | Rel. long. distance  | Overtaking |            ± [0.20] m                      |
        +----------------------+------------+--------------+--------------+--------------+
        |                      | Oncoming   |  0 ± 0.05 m  |  0 ± 0.30 m  | 0 ± [0.15] m |
        | Lateral deviation    +------------+--------------+--------------+--------------+
        |                      | Overtaking |  0 ± 0.05 m  |  0 ± 0.20 m  |      -       |
        +----------------------+------------+--------------+--------------+--------------+
        | Steady state lane    |            |              |              |              |
        | departure lateral    |            |  ± 0.05 m/s  |      -       |      -       |
        | velocity             |            |              |              |              |
        +----------------------+------------+--------------+--------------+--------------+
        | Yaw velocity         |            |  0 ± 1.0 °/s |      -       |      -       |
        +----------------------+ Up to      +--------------+--------------+--------------+
        | Yaw angle            | T_STEER    |      -       |      -       |  0 ± 1.5 °   |
        +----------------------+ for VUT    +--------------+--------------+--------------+
        | Steering wheel       |            |  0 ± 15.0 °/s|      -       |      -       |
        | velocity             |            |              |              |              |
        +----------------------+------------+--------------+--------------+--------------+

        -- HMC --
        DTLE validation is added for Lane Departure cluster (not in original table).

        -- Output --
        Returns a tuple of (validity, reasons_if_not)
        """
        reasons = []

        indices, err = self.elk_get_indices(data, context, log)
        if err is not None:
            log.push("Failed to calculate time indices")
            return False, err

        # DTLE
        if context.cluster == "Lane Departure":
            rv, err = self.elk_validate_dtle(data, indices, context, log)
            if err is not None:
                reasons.append(err)
            else:
                if not is_signal_in_range(*rv):
                    reasons.append("DTLE outside range")

        """ VUT """
        # Speed
        rv, err = self.elk_validate_vut_long_vel(data, indices, context, log)
        if err is not None:
            reasons.append(err)
        else:
            if not is_signal_in_range(*rv):
                reasons.append("VUT speed outside range")

        # Relative longitudinal speed
        if context.cluster == "Overtaking":
            rv, err = self.elk_validate_rel_long_vel(data, indices, context, log)
            if err is not None:
                reasons.append(err)
            else:
                if not is_signal_in_range(*rv):
                    reasons.append("Relative longitudinal speed outside range")

        # Relative longitudinal distance
        if context.cluster == "Overtaking":
            rv, err = self.elk_validate_rel_long_dist(data, indices, context, log)
            if err is not None:
                reasons.append(err)
            else:
                if not is_signal_in_range(*rv):
                    reasons.append("Relative longitudinal distance outside range")

        # Lateral deviation
        if context.cluster in ["Oncoming", "Overtaking"]:
            rv, err = self.elk_validate_vut_lat_dev(data, indices, context, log)
            if err is not None:
                reasons.append(err)
            else:
                if not is_signal_in_range(*rv):
                    reasons.append("VUT lateral deviation outside range")

        # Lateral velocity
        rv, err = self.elk_validate_vut_lat_vel(data, indices, context, log)
        if err is not None:
            reasons.append(err)
        else:
            if not is_signal_in_range(*rv):
                reasons.append("VUT lateral velocity outside range")

        # Yaw velocity
        rv, err = self.elk_validate_vut_yaw_vel(data, indices, context, log)
        if err is not None:
            reasons.append(err)
        else:
            if not is_signal_in_range(*rv):
                reasons.append("VUT yaw velocity outside range")

        # Steering wheel velocity
        rv, err = self.elk_validate_vut_steer_whl_vel(data, indices, context, log)
        if err is not None:
            reasons.append(err)
        else:
            if not is_signal_in_range(*rv):
                reasons.append("VUT steering wheel velocity outside range")

        """ Target """
        # Target Speed
        rv, err = self.elk_validate_target_long_vel(data, indices, context, log)
        if err is not None:
            reasons.append(err)
        else:
            if not is_signal_in_range(*rv):
                reasons.append("Target longitudinal velocity outside range")

        if context.cluster == "Oncoming" or context.scenario_type == "Car-to-Car":
            rv, err = self.elk_validate_target_lat_dev(data, indices, context, log)
            if err is not None:
                reasons.append(err)
            else:
                if not is_signal_in_range(*rv):
                    reasons.append("Target lateral deviation outside range")

        # Target yaw angle
        if context.scenario_type == "Car-to-Motorcycle":
            rv, err = self.elk_validate_target_yaw(data, indices, context, log)
            if err is not None:
                reasons.append(err)
            else:
                if not is_signal_in_range(*rv):
                    reasons.append("Target yaw angle outside range")

        if reasons:
            reason = "\n".join(reasons)
        else:
            reason = None
        return not reasons, reason

    @ensure_log
    def elk_t_collision_idx(self, data: Data, context: ScenarioContext = None, log: Log = None) -> Index:
        """
        -- EuroNCAP --
        Crash Avoidance Lane Departure Collisions v1.1
        4.3.2 Test Scenarios
        The ELK system has failed to intervene (sufficiently) to prevent a collision between the VUT and target vehicle.
        This can be assumed when one of the following occurs:
        ▪ The lateral separation between the VUT and target vehicle equal < 0.3m in the oncoming and overtaking scenario
        ▪ No intervention is observed at a TTC = 0.8s or a TTC submitted by the OEM

        -- HMC --
        Primary method: Uses the 'collision_count' signal directly.
        Returns the first index where collision_count > 0.

        Fallback method (when collision_count unavailable):
        Uses relative distance signals to infer collision.
        1. Gets t_0 as the starting index (defaults to 0 if unavailable)
        2. Searches for the first of:
           - rel_long_dist < 0.1m (longitudinal collision)
           - rel_lat_dist < 0.3m (lateral collision)
        3. Returns the minimum of these indices if collision detected

        -- Output --
        Returns the first index where a collision occurs.
        If no collision occurs, returns NOT_OCCURRED.
        If collision cannot be determined, returns None.
        """
        _, collision = self.get_signal(data, "collision_count", log)
        if collision is not None:
            idx = get_first_index_larger_than(collision, 0)
            if idx is None:
                return NOT_OCCURRED
            return idx

        _, rel_long_dist = self.get_signal(data, "rel_long_dist", log)
        if rel_long_dist is None:
            log.push("Could not retrieve signal 'rel_long_dist'")
            return None

        _, rel_lat_dist = self.get_signal(data, "rel_lat_dist", log)
        if rel_lat_dist is None:
            log.push("Could not retrieve signal 'rel_lat_dist'")
            return None

        t_0_idx = self.elk_t_0_idx(data, context, log)
        if t_0_idx is None:
            t_0_idx = 0

        rel_long_dist = rel_long_dist[t_0_idx:]
        rel_lat_dist = rel_lat_dist[t_0_idx:]

        long_col_idx = get_first_index_smaller_than(rel_long_dist, 0.1)
        lat_col_idx = get_first_index_smaller_than(rel_lat_dist, 0.3)

        valid_indices = [idx for idx in [long_col_idx, lat_col_idx] if idx is not None]
        idx = min(valid_indices) if valid_indices else None
        if idx is None:
            return NOT_OCCURRED
        return t_0_idx + idx

    @ensure_log
    def elk_collision(self, data: Data, context: ScenarioContext = None, log: Log = None) -> Value:
        """
        -- EuroNCAP --
        Crash Avoidance Lane Departure Collisions v1.1
        4.3.2 Test Scenarios
        The ELK system has failed to intervene (sufficiently) to prevent a collision between the VUT and target vehicle.
        This can be assumed when one of the following occurs:
        ▪ The lateral separation between the VUT and target vehicle equal < 0.3m in the oncoming and overtaking scenario
        ▪ No intervention is observed at a TTC = 0.8s or a TTC submitted by the OEM

        -- HMC --

        -- Output --
        Returns True if a collision occurred,
        False if no collision occurred,
        None if collision could not be determined.
        """
        idx = self.elk_t_collision_idx(data, context, log)
        if idx == NOT_OCCURRED:
            return False
        if idx is None:
            log.push("Could not determine collision")
            return None
        return True

    @ensure_log
    def elk_impact_speed(self, data: Data, context: ScenarioContext = None, log: Log = None) -> Value:
        """
        -- EuroNCAP --
        N/A - Derived metric.

        -- HMC --

        -- Output --
        Returns the absolute VUT longitudinal speed at the time of collision.
        If no collision has occurred, returns 0.0.
        If collision cannot be determined, returns None.
        """
        idx = self.elk_t_collision_idx(data, context, log)
        if idx == NOT_OCCURRED:
            # Return 0.0 if no collision has occurred
            return 0.0

        if idx is None:
            log.push("Could not determine collision")
            return None

        _, signal = self.get_signal(data, "vut_long_vel", log)
        if signal is None:
            log.push("Could not retrieve signal 'vut_long_vel'")
            return None

        return abs(float(signal[idx]))

    @ensure_log
    def elk_color_kpi(self, data: Data, context: ScenarioContext, log: Log = None) -> Value:
        """
        -- EuroNCAP --
        5.2.2.1 Road edge
        The limit value for DTLE in Lane Departure - ELK Road Edge tests is set to -0.1m, meaning that the vehicle is only
        allowed to have a part of the front wheel outside of the road edge.
        5.2.2.2 Lane Departure Warning
        Where the test vehicle is unable to meet the assessment and performance criteria for the extended range of
        Lane Departure - Road Edge tests, the vehicle becomes eligible for the assessment of the Lane Departure Warning.
        Any LDW system that issues a haptic warning clearly relating to the
        lateral control of the vehicle noticeable by the driver (e.g. notable
        heading correction, steering wheel vibration, etc.) before a DTLE of
        -0.1m is awarded when active at lateral velocities up to at least
        0.7m/s.
        5.2.3.1 ELK Oncoming & Overtaking
        For all ELK Car-to-Car & Car-to-Motorcyclist Oncoming and Overtaking tests with an oncoming or overtaking vehicle,
        the assessment criteria used is "no impact", meaning that the VUT is not allowed to contact* the overtaking
        or oncoming vehicle target at any time during the test.
        * For the motorcyclist test scenarios, the lateral separation between the VUT and the Oncoming or Overtaking EMT must be >0.3m.
        5.2.3.2 Blind Spot Monitoring
        Where the test vehicle is unable to meet the assessment and performance criteria for the extended range of
        ELK Overtaking (Car and Motorcyclist) tests, the test vehicle becomes eligible for the assessment of the blind spot monitoring system.
        For the blind spot monitoring tests, the assessment criteria used is the visual blind spot information that
        shall be supplied in respect to vehicles in the blind spot of the VUT.

        -- HMC --
        DTLE calculation method implementation uses minimum DTLE while LKA is active.
        LDW (Lane Departure Warning) and BSM (Blind Spot Monitoring) fallback assessments are not implemented.

        -- Output --
        Returns color string: "GREEN" or "RED".
        Returns "RED" if required signals cannot be determined.
        """
        if context.cluster == "Lane Departure":
            dtle = self.elk_dtle(data, context, log)
            if dtle is None:
                log.push("Could not calculate minimum 'dtle' during LKA")
                return "RED"
            if dtle < -0.1:
                return "RED"
            else:
                return "GREEN"
        else:
            collision = self.elk_collision(data, context, log)
            if collision is None:
                log.push("Could not determine collision")

            if collision:
                return "RED"
            else:
                return "GREEN"

    @ensure_log
    def elk_comparison_kpi(self, erg: ERG, phys: BLF | ASC, context: ScenarioContext, log: Log = None) -> Value:
        """
        -- EuroNCAP --
        Safe Driving & Crash Avoidance Virtual Testing v1.0
        5.2.2 Key Performance Indicator (KPI)
        For each applicable KPI, the error between physical test and simulation are calculated:
        KPI_error = KPI_physical - KPI_simulation
        5.3 Acceptance Criteria
        Different acceptance criteria exist for the scenarios within the scenario clusters:
        - Lane - ELK: Road Edge, Oncoming, Overtaking
        +------------------------+--------------------------+--------------------+
        | Scenario cluster       | KPI_error                | Accepted KPI_error |
        +------------------------+--------------------------+--------------------+
        | Lane - ELK             | DTLE_ELK [m]             | [+-0.2]            |
        +------------------------+--------------------------+--------------------+

        -- HMC --
        Instead of using the value of DTLE when ELK is flagged on, use the minimum value of DTLE while LKA is flagged on.
        Apply only to ELK Road Edge(Lane Departure) scenarios.
        """
        if context.cluster != "Lane Departure":
            return True

        erg_dtle = self.elk_dtle(erg, context, log)
        phys_dtle = self.elk_dtle(phys, context, log)
        if erg_dtle is None:
            log.push("DTLE could not be calculated for ERG")
            return None
        if phys_dtle is None:
            log.push("DTLE could not be calculated for BLF/ASC")
            return None

        return compare(erg_dtle, phys_dtle, 0.2)

    @ensure_log
    def fca_signal_plot(self, data: Data, signal_name: str, context: ScenarioContext, log: Log = None) -> dict[str, object]:
        """
        -- EuroNCAP --
        N/A - Visualization helper function.

        -- HMC --
        Generates plot data for scenario validation signals.
        Supported signals: vut_long_vel, vut_lat_dev, vut_yaw_vel, vut_steer_whl_vel, rel_long_dist.
        rel_long_dist is only plotted for CCRb and CMRb scenarios.
        Time axis is shifted so t_0 = 0.
        Includes validation bounds (hBand) and time window (vBand) when available.

        -- Output --
        Returns dict with keys: x, y, y_color, title, vBand, hBand, vLines (optional).
        Returns None if signal cannot be retrieved.
        """
        # Get time axis - Time is a standard signal in ERG
        if context.scenario not in ["CCRb", "CMRb"] and signal_name == "rel_long_dist":
            return None

        x, y = self.get_signal(data, signal_name, log)

        if y is None:
            log.push(f"Could not plot signal '{signal_name}'")
            return None

        rv = {"x": x.tolist(), "y": y.tolist(), "y_color": "blue"}

        if signal_name == "vut_long_vel":
            rv["title"] = "VUT Longitudinal Velocity"
        elif signal_name == "vut_lat_dev":
            rv["title"] = "VUT Lateral Deviation"
        elif signal_name == "vut_yaw_vel":
            rv["title"] = "VUT Yaw Velocity"
        elif signal_name == "vut_steer_whl_vel":
            rv["title"] = "VUT Steering Wheel Velocity"
        elif signal_name == "rel_long_dist":
            rv["title"] = "Relative Longitudinal Distance"

        try:
            indices, err = self.fca_get_indices(data, context, log)
            if indices is None:
                log.push(f"Cannot customize plot for '{signal_name}' — {err}")
                return rv

            bound = None
            if signal_name == "vut_long_vel":
                bound, _ = self.fca_validate_vut_long_vel(data, indices, context, log)
            elif signal_name == "vut_lat_dev":
                bound, _ = self.fca_validate_vut_lat_dev(data, indices, context, log)
            elif signal_name == "vut_yaw_vel":
                bound, _ = self.fca_validate_vut_yaw_vel(data, indices, context, log)
            elif signal_name == "vut_steer_whl_vel":
                bound, _ = self.fca_validate_vut_steer_whl_vel(data, indices, context, log)

            if bound is not None:
                rv["y_color"] = "blue" if is_signal_in_range(*bound) else "red"
                rv["vBand"] = [x[bound[1]], x[bound[2]]]
                rv["vBand_color"] = "rgba(255, 0, 0, 0.1)"
                rv["hBand"] = bound[3:5]
                rv["hBand_color"] = "rgba(0, 255, 0, 0.1)"

            rv["vLines"] = [
                {"value": x[indices[0]], "color": "blue", "label": "t_0"},
                {"value": x[indices[1]], "color": "purple", "label": "t_end"},
            ]
        except Exception as e:
            log.push(f"Unexpected error customizing plot for '{signal_name}': {e}")

        return rv

    @ensure_log
    def elk_signal_plot(self, data: Data, signal_name: str, context: ScenarioContext = None, log: Log = None) -> dict[str, object]:
        """
        -- EuroNCAP --
        N/A - Visualization helper function.

        -- HMC --
        Generates plot data for ELK scenario validation signals.
        Supported signals: vut_long_vel, rel_long_vel, rel_long_dist, vut_lat_dev, vut_lat_vel, vut_yaw_vel, vut_steer_whl_vel.
        Time axis is shifted so t_0 = 0.
        Includes validation bounds (hBand) and time window (vBand) when available.

        -- Output --
        Returns dict with keys: x, y, y_color, title, vBand, hBand, vLines (optional).
        Returns None if signal cannot be retrieved.
        """
        x, y = self.get_signal(data, signal_name, log)
        if y is None:
            log.push(f"Could not plot signal '{signal_name}'")
            return None

        rv = {"x": x.tolist(), "y": y.tolist(), "y_color": "blue"}
        if signal_name == "vut_long_vel":
            rv["title"] = "VUT Longitudinal Velocity"
        elif signal_name == "rel_long_vel":
            rv["title"] = "Relative Longitudinal Velocity"
        elif signal_name == "rel_long_dist":
            rv["title"] = "Relative Longitudinal Distance"
        elif signal_name == "vut_lat_dev":
            rv["title"] = "VUT Lateral Deviation"
        elif signal_name == "vut_lat_vel":
            rv["title"] = "VUT Lateral Velocity"
        elif signal_name == "vut_yaw_vel":
            rv["title"] = "VUT Yaw Velocity"
        elif signal_name == "vut_steer_whl_vel":
            rv["title"] = "VUT Steering Wheel Velocity"

        try:
            indices, err = self.elk_get_indices(data, context, log)
            if indices is None:
                log.push(f"Cannot customize plot for '{signal_name}' — {err}")
                return rv

            bound = None
            if signal_name == "vut_long_vel":
                bound, _ = self.elk_validate_vut_long_vel(data, indices, context, log)
            elif signal_name == "rel_long_vel":
                bound, _ = self.elk_validate_rel_long_vel(data, indices, context, log)
            elif signal_name == "rel_long_dist":
                bound, _ = self.elk_validate_rel_long_dist(data, indices, context, log)
            elif signal_name == "vut_lat_dev":
                bound, _ = self.elk_validate_vut_lat_dev(data, indices, context, log)
            elif signal_name == "vut_lat_vel":
                bound, _ = self.elk_validate_vut_lat_vel(data, indices, context, log)
            elif signal_name == "vut_yaw_vel":
                bound, _ = self.elk_validate_vut_yaw_vel(data, indices, context, log)
            elif signal_name == "vut_steer_whl_vel":
                bound, _ = self.elk_validate_vut_steer_whl_vel(data, indices, context, log)

            if bound is not None:
                rv["y_color"] = "blue" if is_signal_in_range(*bound) else "red"
                rv["vBand"] = [x[bound[1]], x[bound[2]]]
                rv["vBand_color"] = "rgba(255, 0, 0, 0.1)"
                rv["hBand"] = bound[3:5]
                rv["hBand_color"] = "rgba(0, 255, 0, 0.1)"

            rv["vLines"] = [
                {"value": x[indices[0]], "color": "blue", "label": "t_0"},
                {"value": x[indices[1]], "color": "purple", "label": "t_end"},
            ]
        except Exception as e:
            log.push(f"Unexpected error customizing plot for '{signal_name}': {e}")

        return rv
