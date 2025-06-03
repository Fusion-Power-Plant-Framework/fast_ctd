import pytest

from fast_ctd import check_watertight, decode_tightness_checks, make_watertight


@pytest.fixture
def test_data_path():
    from pathlib import Path

    return Path(__file__).parent / "test_data"


@pytest.mark.parametrize(
    ("test_shape_name", "is_wt"),
    [
        ("box_cyl_not_wt.h5m", False),
        ("test_cubes.h5m", True),
    ],
)
def test_make_and_check_watertight(
    test_shape_name: str,
    is_wt: bool,
    tmp_path,
    test_data_path,
):
    input_file = test_data_path / test_shape_name
    output_file = tmp_path / f"wt_{test_shape_name}"

    sp_make_wt = make_watertight(input_file, output_file)
    # run it on the output, they should be the same
    sp_check_wt = check_watertight(output_file)

    assert sp_make_wt.returncode == 0, "make_watertight failed"
    assert output_file.exists(), "Output file was not created"
    assert sp_check_wt.returncode == 0, "check_watertight failed"

    out_make_wt = decode_tightness_checks(sp_make_wt.stdout)
    out_check_wt = decode_tightness_checks(sp_check_wt.stdout)

    assert out_make_wt is not None, (
        "Failed to decode tightness checks on make_watertight"
    )
    assert all(p == 0 if is_wt else p != 0 for p in out_make_wt), (
        "make_watertight did not produce expected results"
    )
    assert out_check_wt is not None, (
        "Failed to decode tightness checks on check_watertight"
    )
    assert all(p == 0 if is_wt else p != 0 for p in out_check_wt), (
        "check_watertight did not produce expected results"
    )
