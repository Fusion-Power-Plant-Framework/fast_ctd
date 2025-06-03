"""An example of a workflow to convert a STEP file to a DAGMC file using fast_ctd."""

from pathlib import Path

from fast_ctd import (
    dagmc_to_vtk,
    decode_tightness_checks,
    facet_brep_to_dagmc,
    make_watertight,
    merge_brep_geometries,
    step_to_brep,
)

# These file names can be anything

ex_obj_name = "test_cubes"
data_path = Path(__file__).parent / "data"

materials_csv_file = data_path / f"{ex_obj_name}-mats.csv"
input_stp_file = data_path / f"{ex_obj_name}.stp"
brep_file = data_path / f"{ex_obj_name}.brep"
merged_brep_file = data_path / f"{ex_obj_name}-merged.brep"
output_dagmc_file = data_path / f"{ex_obj_name}-nwt.h5m"
output_wt_dagmc_file = data_path / f"{ex_obj_name}.h5m"
output_dagmc_vtk_file = data_path / f"{ex_obj_name}.vtk"

enable_debug_logging = True

print("Converting to BREP")
comps_info = step_to_brep(
    input_stp_file,
    brep_file,
    enable_logging=enable_debug_logging,
)

print("Merging BREP entities")
merge_brep_geometries(
    brep_file,
    merged_brep_file,
    enable_logging=enable_debug_logging,
)

print("Converting to DAGMC")
facet_brep_to_dagmc(
    merged_brep_file,
    output_h5m_file=output_dagmc_file,
    materials_csv_file=materials_csv_file,
    enable_logging=enable_debug_logging,
)

print("Running make_watertight")
wt = make_watertight(output_dagmc_file, output_wt_dagmc_file)
percentages = decode_tightness_checks(wt.stdout)
if percentages:
    all_zeros = all(p == 0 for p in percentages)
    print(f"model is watertight: {all_zeros}")
else:
    print("model watertightness could not be determined")
    print("make_watertight stdout:")
    print(wt.stdout)
    print("make_watertight stderr:")
    print(wt.stderr)

print("Creating VTK model - view in Paraview")
mbc = dagmc_to_vtk(output_wt_dagmc_file, output_dagmc_vtk_file)

# Validate DAGMC model using OpenMC -- future work

# validate_dagmc_model_using_openmc(
#     dagmc_file=output_wt_dagmc_file
# )
