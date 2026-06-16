from __future__ import annotations

import numpy as np

import iso18571
from iso18571_native import magnitude_ratio


def main() -> int:
    x = np.array([0.0, 1.0, 2.0, 3.0], dtype=np.float64)
    y = np.array([0.0, 1.1, 1.9, 3.1], dtype=np.float64)
    assert np.isfinite(magnitude_ratio(x, y, 0.5))
    assert iso18571.ISO18571
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
