import pytest

from fast_ctd import (
    check_watertight,
    dagmc_to_vtk,
    decode_tightness_checks,
    facet_brep_to_dagmc,
    make_watertight,
    merge_brep_geometries,
    step_to_brep,
)


@pytest.fixture
def test_data_path():
    from pathlib import Path

    return Path(__file__).parent / "test_data"


def test_tools_output_existence_checks(tmp_path, test_data_path):
    """Test that the tools output files are created and exist."""
    tst_obj_name = "test_cubes"

    input_stp_file = test_data_path / f"{tst_obj_name}.stp"
    materials_csv_file = test_data_path / f"{tst_obj_name}-mats.csv"

    brep_file = tmp_path / f"{tst_obj_name}.brep"
    merged_brep_file = tmp_path / f"{tst_obj_name}-merged.brep"
    output_dagmc_file = tmp_path / f"{tst_obj_name}-nwt.h5m"
    output_dagmc_vtk_file = tmp_path / f"{tst_obj_name}.vtk"

    comps_info = step_to_brep(input_stp_file, brep_file)
    assert brep_file.exists(), "BREP file was not created"
    assert len(comps_info) > 0, "No components found in BREP file"

    merge_brep_geometries(brep_file, merged_brep_file)
    assert merged_brep_file.exists(), "Merged BREP file was not created"

    facet_brep_to_dagmc(
        merged_brep_file,
        output_h5m_file=output_dagmc_file,
        materials_csv_file=materials_csv_file,
    )
    assert output_dagmc_file.exists(), "DAGMC file was not created"
    assert output_dagmc_file.stat().st_size > 0, "DAGMC file is empty"

    dagmc_to_vtk(output_dagmc_file, output_dagmc_vtk_file)
    assert output_dagmc_vtk_file.exists(), "DAGMC VTK file was not created"
    assert output_dagmc_vtk_file.stat().st_size > 0, "DAGMC VTK file is empty"


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
