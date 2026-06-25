from __future__ import annotations

from typing import Final, TypedDict

import numpy as np
import pytest

from iso18571 import ISO18571
from tools.annex import AnnexCase, AnnexDataset


class ExpectedScores(TypedDict):
    R: float
    Z: float
    EP: float
    EM: float
    ES: float


ANNEX_TOLERANCE: Final = 1.0e-3

EXPECTED_ANNEX_SCORES: Final[dict[str, ExpectedScores]] = {
    "annex_c_1_1__ac1__cae1": {
        "R": 0.9169,
        "Z": 0.9560,
        "EP": 0.9518,
        "EM": 0.9524,
        "ES": 0.7683,
    },
    "annex_c_1_1__ac1__cae2": {
        "R": 0.8848,
        "Z": 0.8977,
        "EP": 0.9196,
        "EM": 0.9640,
        "ES": 0.7449,
    },
    "annex_c_1_1__ac1__cae3": {
        "R": 0.9122,
        "Z": 0.9225,
        "EP": 0.9839,
        "EM": 0.9716,
        "ES": 0.7606,
    },
    "annex_c_1_2__ac2__cae1": {
        "R": 0.4379,
        "Z": 0.4059,
        "EP": 0.0065,
        "EM": 0.6471,
        "ES": 0.7241,
    },
    "annex_c_1_2__ac2__cae2": {
        "R": 0.6993,
        "Z": 0.6423,
        "EP": 0.6254,
        "EM": 0.8219,
        "ES": 0.7648,
    },
    "annex_c_1_2__ac2__cae3": {
        "R": 0.8542,
        "Z": 0.8452,
        "EP": 0.7720,
        "EM": 0.9659,
        "ES": 0.8427,
    },
    "annex_c_1_3__ac3__cae1": {
        "R": 0.8045,
        "Z": 0.7263,
        "EP": 0.9401,
        "EM": 0.8977,
        "ES": 0.7321,
    },
    "annex_c_1_3__ac3__cae2": {
        "R": 0.8701,
        "Z": 0.8164,
        "EP": 0.9401,
        "EM": 0.9310,
        "ES": 0.8467,
    },
    "annex_c_1_3__ac3__cae3": {
        "R": 0.8314,
        "Z": 0.7499,
        "EP": 0.9401,
        "EM": 0.9088,
        "ES": 0.8084,
    },
    "annex_c_1_4__ac4__cae1": {
        "R": 0.7877,
        "Z": 0.7927,
        "EP": 0.9771,
        "EM": 0.8709,
        "ES": 0.5052,
    },
    "annex_c_1_4__ac4__cae2": {
        "R": 0.6540,
        "Z": 0.6476,
        "EP": 0.9200,
        "EM": 0.7909,
        "ES": 0.2641,
    },
    "annex_c_1_4__ac4__cae3": {
        "R": 0.7910,
        "Z": 0.7837,
        "EP": 0.9943,
        "EM": 0.8486,
        "ES": 0.5446,
    },
    "annex_c_2_1__an1__cae1": {
        "R": 0.6994,
        "Z": 0.5297,
        "EP": 0.7175,
        "EM": 0.8924,
        "ES": 0.8276,
    },
    "annex_c_2_1__an1__cae2": {
        "R": 0.6242,
        "Z": 0.5406,
        "EP": 0.9177,
        "EM": 0.4633,
        "ES": 0.6588,
    },
    "annex_c_2_1__an1__cae3": {
        "R": 0.7060,
        "Z": 0.5379,
        "EP": 0.7496,
        "EM": 0.8476,
        "ES": 0.8568,
    },
    "annex_c_2_2__an2__cae1": {
        "R": 0.5912,
        "Z": 0.4033,
        "EP": 0.7203,
        "EM": 0.7599,
        "ES": 0.6694,
    },
    "annex_c_2_2__an2__cae2": {
        "R": 0.3643,
        "Z": 0.2868,
        "EP": 0.9056,
        "EM": 0.0980,
        "ES": 0.2441,
    },
    "annex_c_2_2__an2__cae3": {
        "R": 0.5835,
        "Z": 0.3740,
        "EP": 0.7448,
        "EM": 0.6426,
        "ES": 0.7819,
    },
    "annex_c_3_1__ds1__cae1": {
        "R": 0.9214,
        "Z": 0.8878,
        "EP": 0.9369,
        "EM": 0.9765,
        "ES": 0.9182,
    },
    "annex_c_3_1__ds1__cae2": {
        "R": 0.9825,
        "Z": 1.0000,
        "EP": 0.9747,
        "EM": 0.9814,
        "ES": 0.9563,
    },
    "annex_c_3_1__ds1__cae3": {
        "R": 0.9834,
        "Z": 0.9994,
        "EP": 0.9495,
        "EM": 0.9948,
        "ES": 0.9741,
    },
    "annex_c_3_2__ds2__cae1": {
        "R": 0.6518,
        "Z": 0.4702,
        "EP": 0.8085,
        "EM": 0.6796,
        "ES": 0.8303,
    },
    "annex_c_3_2__ds2__cae2": {
        "R": 0.8632,
        "Z": 0.8069,
        "EP": 0.9362,
        "EM": 0.9422,
        "ES": 0.8236,
    },
    "annex_c_3_2__ds2__cae3": {
        "R": 0.8368,
        "Z": 0.7916,
        "EP": 0.8582,
        "EM": 0.9553,
        "ES": 0.7875,
    },
    "annex_c_4_1__fo1__cae1": {
        "R": 0.5364,
        "Z": 0.4501,
        "EP": 0.6899,
        "EM": 0.6177,
        "ES": 0.4739,
    },
    "annex_c_4_1__fo1__cae2": {
        "R": 0.6556,
        "Z": 0.5267,
        "EP": 0.5930,
        "EM": 0.9111,
        "ES": 0.7206,
    },
    "annex_c_4_1__fo1__cae3": {
        "R": 0.9297,
        "Z": 0.9112,
        "EP": 0.9225,
        "EM": 0.9572,
        "ES": 0.9465,
    },
    "annex_c_4_2__fo2__cae1": {
        "R": 0.2761,
        "Z": 0.3753,
        "EP": 0.5238,
        "EM": 0.0000,
        "ES": 0.1060,
    },
    "annex_c_4_2__fo2__cae2": {
        "R": 0.5766,
        "Z": 0.4277,
        "EP": 0.3810,
        "EM": 0.8390,
        "ES": 0.8079,
    },
    "annex_c_4_2__fo2__cae3": {
        "R": 0.6463,
        "Z": 0.5214,
        "EP": 0.8810,
        "EM": 0.6582,
        "ES": 0.6498,
    },
    "annex_c_4_3__fo3__cae1": {
        "R": 0.7133,
        "Z": 0.6548,
        "EP": 0.9769,
        "EM": 0.7384,
        "ES": 0.5414,
    },
    "annex_c_4_3__fo3__cae2": {
        "R": 0.4658,
        "Z": 0.4910,
        "EP": 0.8731,
        "EM": 0.3628,
        "ES": 0.1109,
    },
    "annex_c_4_3__fo3__cae3": {
        "R": 0.8642,
        "Z": 0.9032,
        "EP": 0.9596,
        "EM": 0.9282,
        "ES": 0.6269,
    },
    "annex_c_5_1__mo1__cae1": {
        "R": 0.8203,
        "Z": 0.7402,
        "EP": 0.9784,
        "EM": 0.9099,
        "ES": 0.7329,
    },
    "annex_c_5_1__mo1__cae2": {
        "R": 0.7549,
        "Z": 0.7233,
        "EP": 0.5905,
        "EM": 0.9668,
        "ES": 0.7703,
    },
    "annex_c_5_1__mo1__cae3": {
        "R": 0.8084,
        "Z": 0.7901,
        "EP": 0.8060,
        "EM": 0.9618,
        "ES": 0.6939,
    },
    "annex_c_5_2__mo2__cae1": {
        "R": 0.2946,
        "Z": 0.2049,
        "EP": 0.4979,
        "EM": 0.0000,
        "ES": 0.5652,
    },
    "annex_c_5_2__mo2__cae2": {
        "R": 0.3770,
        "Z": 0.2797,
        "EP": 0.5711,
        "EM": 0.1995,
        "ES": 0.5551,
    },
    "annex_c_5_2__mo2__cae3": {
        "R": 0.5592,
        "Z": 0.4281,
        "EP": 0.9268,
        "EM": 0.4683,
        "ES": 0.5450,
    },
    "annex_c_5_3__mo3__cae1": {
        "R": 0.6607,
        "Z": 0.5390,
        "EP": 0.6958,
        "EM": 0.8390,
        "ES": 0.6907,
    },
    "annex_c_5_3__mo3__cae2": {
        "R": 0.6637,
        "Z": 0.5380,
        "EP": 0.7338,
        "EM": 0.7983,
        "ES": 0.7102,
    },
    "annex_c_5_3__mo3__cae3": {
        "R": 0.6713,
        "Z": 0.5563,
        "EP": 1.0000,
        "EM": 0.7304,
        "ES": 0.5132,
    },
}


@pytest.mark.annex
def test_expected_annex_manifest_covers_all_official_cases(
    annex_dataset: AnnexDataset,
) -> None:
    assert {path.stem for path in annex_dataset.paths} == set(EXPECTED_ANNEX_SCORES)


@pytest.mark.annex
@pytest.mark.parametrize("case_name", sorted(EXPECTED_ANNEX_SCORES))
def test_annex_scores_match_expected(
    case_name: str, annex_dataset: AnnexDataset
) -> None:
    scorer = _scorer_for_case(case_name, annex_dataset)
    expected = EXPECTED_ANNEX_SCORES[case_name]
    assert abs(scorer.scores["R"] - expected["R"]) < ANNEX_TOLERANCE
    assert abs(scorer.scores["Z"] - expected["Z"]) < ANNEX_TOLERANCE
    assert abs(scorer.scores["EP"] - expected["EP"]) < ANNEX_TOLERANCE
    assert abs(scorer.scores["EM"] - expected["EM"]) < ANNEX_TOLERANCE
    assert abs(scorer.scores["ES"] - expected["ES"]) < ANNEX_TOLERANCE


@pytest.mark.annex
@pytest.mark.parametrize("case_name", sorted(EXPECTED_ANNEX_SCORES))
def test_annex_original_input_signals_match_loader_inputs(
    case_name: str,
    annex_dataset: AnnexDataset,
) -> None:
    case = _case(case_name, annex_dataset)

    np.testing.assert_array_equal(
        case.reference_curve()[:, 0], case.input_values("Time")
    )
    np.testing.assert_array_equal(
        case.reference_curve()[:, 1], case.input_values("Test")
    )
    np.testing.assert_array_equal(
        case.comparison_curve()[:, 1], case.input_values("CAE")
    )


@pytest.mark.annex
@pytest.mark.parametrize("case_name", sorted(EXPECTED_ANNEX_SCORES))
def test_annex_corridor_curves_match(
    case_name: str, annex_dataset: AnnexDataset
) -> None:
    case = _case(case_name, annex_dataset)
    scorer = _scorer_for_case(case_name, annex_dataset)

    _assert_max_error(
        scorer.corridor_outer_upper_values, case.input_values("Outer_Corridor_Upper")
    )
    _assert_max_error(
        scorer.corridor_inner_upper_values, case.input_values("Inner_Corridor_Upper")
    )
    _assert_max_error(
        scorer.corridor_inner_lower_values, case.input_values("Inner_Corridor_Lower")
    )
    _assert_max_error(
        scorer.corridor_outer_lower_values, case.input_values("Outer_Corridor_Lower")
    )


@pytest.mark.annex
@pytest.mark.parametrize("case_name", sorted(EXPECTED_ANNEX_SCORES))
def test_annex_phase_shifted_curves_match(
    case_name: str, annex_dataset: AnnexDataset
) -> None:
    case = _case(case_name, annex_dataset)
    scorer = _scorer_for_case(case_name, annex_dataset)

    test_shifted, cae_shifted = case.finite_pair(
        "Test_Phase_Shifted", "CAE_Phase_Shifted"
    )
    _assert_max_error(scorer.shifted_reference_values, test_shifted)
    _assert_max_error(scorer.shifted_comparison_values, cae_shifted)


@pytest.mark.annex
@pytest.mark.parametrize("case_name", sorted(EXPECTED_ANNEX_SCORES))
def test_annex_warped_curves_drive_magnitude_fields(
    case_name: str,
    annex_dataset: AnnexDataset,
) -> None:
    case = _case(case_name, annex_dataset)
    scorer = _scorer_for_case(case_name, annex_dataset)
    scores = scorer.scores
    magnitude_fields = case.magnitude_fields_from_warped()
    assert (
        abs(scores["magnitude_numerator"] - magnitude_fields["magnitude_numerator"])
        < ANNEX_TOLERANCE
    )
    assert (
        abs(scores["magnitude_denominator"] - magnitude_fields["magnitude_denominator"])
        < ANNEX_TOLERANCE
    )
    assert (
        abs(scores["magnitude_error"] - magnitude_fields["magnitude_error"])
        < ANNEX_TOLERANCE
    )


def _case(case_name: str, annex_dataset: AnnexDataset) -> AnnexCase:
    return AnnexCase.from_csv(annex_dataset.root / f"{case_name}.csv")


def _scorer_for_case(case_name: str, annex_dataset: AnnexDataset) -> ISO18571:
    case = _case(case_name, annex_dataset)
    return ISO18571(case.reference_curve(), case.comparison_curve())


def _assert_max_error(actual: np.ndarray, expected: np.ndarray) -> None:
    assert actual.shape == expected.shape
    max_error = float(np.max(np.abs(np.asarray(actual, dtype=np.float64) - expected)))
    assert max_error < ANNEX_TOLERANCE
