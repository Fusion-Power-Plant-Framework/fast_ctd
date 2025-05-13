"""An example of a workflow to convert a STEP file to a DAGMC file using fast_ctd."""

from fast_ctd import (
    facet_brep_to_dagmc,
    make_watertight,
    merge_brep_geometries,
    step_to_brep,
)
from fast_ctd.logging import config_basic_logging

config_basic_logging()

input_stp_file = "data/EUDEMO.stp"
brep_file = "data/EUDEMO.brep"
merged_brep_file = "data/EUDEMO-merged.brep"
output_dagmc_file = "data/EUDEMO-nwt.h5m"
output_wt_dagmc_file = "data/EUDEMO.h5m"

comps_info = step_to_brep(
    input_stp_file,
    brep_file,
    enable_logging=True,
    minimum_volume=None,
)
merge_brep_geometries(
    brep_file,
    merged_brep_file,
    enable_logging=True,
)
facet_brep_to_dagmc(
    merged_brep_file,
    output_h5m_file=output_dagmc_file,
    materials_csv_file="data/EUDEMO-materials.csv",
    enable_logging=True,
)
make_watertight(output_dagmc_file, output_wt_dagmc_file)

# Validate DAGMC model using OpenMC

# validate_dagmc_model_using_openmc(
#     dagmc_file=output_wt_dagmc_file,
#     materials_def="data/EUDEMO-materials.csv",
# )
