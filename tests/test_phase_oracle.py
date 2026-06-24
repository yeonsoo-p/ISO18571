from __future__ import annotations

from typing import Final

import numpy as np
import pytest

from iso18571 import ISO18571
from tests.oracles import PhaseFields, phase_alignment_fields


FloatArray = np.ndarray[tuple[int], np.dtype[np.float64]]
PHASE_TOLERANCE: Final = 1.0e-12


def test_phase_identical_nonzero_signals_do_not_shift() -> None:
    values = np.array([3.0, -2.0, 5.0, 1.0, -4.0, 2.0, 0.0, 6.0, -1.0, 4.0])

    scorer = ISO18571(_curve(values), _curve(values))

    assert scorer.phase_n_eps == 0
    assert scorer.scores["phase_reference_start"] == 0
    assert scorer.scores["phase_comparison_start"] == 0
    assert scorer.scores["phase_shift_length"] == values.shape[0]
    assert scorer.scores["EP"] == 1.0


def test_phase_score_is_zero_when_best_shift_reaches_threshold() -> None:
    reference = np.array(
        [
            3.0,
            -2.0,
            5.0,
            1.0,
            -4.0,
            2.0,
            0.0,
            6.0,
            -1.0,
            4.0,
            7.0,
            -3.0,
            2.0,
            5.0,
            -2.0,
            1.0,
            0.0,
            3.0,
            -5.0,
            2.0,
        ]
    )
    comparison = np.empty_like(reference)
    comparison[:4] = np.array([9.0, -8.0, 7.0, -6.0])
    comparison[4:] = reference[:-4]

    scorer = ISO18571(_curve(reference), _curve(comparison))

    assert scorer.phase_n_eps == 4
    assert scorer.scores["phase_comparison_start"] == 4
    assert scorer.scores["phase_shift_length"] == 16
    assert scorer.scores["EP"] == 0.0


def test_phase_clamps_when_preferred_shift_leaves_short_curve() -> None:
    reference = np.array([3.0, -2.0, 5.0, 1.0, -4.0, 2.0, 0.0, 6.0, -1.0, 4.0])
    comparison = np.empty_like(reference)
    comparison[:2] = np.array([9.0, -8.0])
    comparison[2:] = reference[:-2]

    with pytest.warns(
        RuntimeWarning, match="phase alignment left fewer than 9 samples"
    ):
        scorer = ISO18571(_curve(reference), _curve(comparison), init_min=0.0)

    assert scorer.phase_n_eps == 0
    assert scorer.scores["phase_reference_start"] == 0
    assert scorer.scores["phase_comparison_start"] == 0
    assert scorer.scores["phase_shift_length"] == reference.shape[0]


def test_phase_tie_priority_prefers_fewer_shift_steps() -> None:
    pattern = np.array([2.0, -1.0, 4.0, 0.0, -3.0])
    reference = np.tile(pattern, 5)
    comparison = np.empty_like(reference)
    comparison[0] = 99.0
    comparison[1:] = reference[:-1]

    expected = phase_alignment_fields(reference, comparison, init_min=0.5)
    scorer = ISO18571(_curve(reference), _curve(comparison), init_min=0.5)

    assert expected["phase_n_eps"] == 1
    _assert_phase_fields_match_scorer(expected, scorer)


def test_phase_tie_priority_prefers_left_shift_for_same_shift_count() -> None:
    reference = np.array([2.0, -3.0] * 10)
    comparison = np.array([-3.0, 2.0] * 10)

    expected = phase_alignment_fields(reference, comparison, init_min=0.5)
    scorer = ISO18571(_curve(reference), _curve(comparison), init_min=0.5)

    assert expected["phase_n_eps"] == 1
    assert expected["phase_reference_start"] == 0
    assert expected["phase_comparison_start"] == 1
    _assert_phase_fields_match_scorer(expected, scorer)


def _assert_phase_fields_match_scorer(expected: PhaseFields, scorer: ISO18571) -> None:
    scores = scorer.scores
    assert scores["phase_reference_start"] == expected["phase_reference_start"]
    assert scores["phase_comparison_start"] == expected["phase_comparison_start"]
    assert scores["phase_shift_length"] == expected["phase_shift_length"]
    assert scores["phase_n_eps"] == expected["phase_n_eps"]
    assert abs(scores["phase_rho_e"] - expected["phase_rho_e"]) < PHASE_TOLERANCE


def _curve(values: FloatArray) -> np.ndarray[tuple[int, int], np.dtype[np.float64]]:
    time = np.arange(values.shape[0], dtype=np.float64)
    return np.column_stack((time, values)).astype(np.float64, copy=False)
