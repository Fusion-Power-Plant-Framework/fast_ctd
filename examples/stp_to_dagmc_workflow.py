# print("merging")
# merge("EUDEMO.brep", "EUDEMO-merged.brep")

# print("faceting")
# facet("EUDEMO-merged.brep", "EUDEMO-materials.csv")

from fast_ctd import facet_brep_to_dagmc, merge_brep_geometries

input_stp_file = "data/EUDEMO.stp"
brep_file = "data/EUDEMO.brep"
merged_brep_file = "data/EUDEMO-merged.brep"
output_dagmc_file = "data/EUDEMO.h5m"

# Convert STEP to BREP
# brep_file, list_of_component_names = stp_to_brep(input_stp_file, brep_file)

# Merge BREP geometries (i.e. imprinted geometries)
merge_brep_geometries(brep_file, merged_brep_file)

# # Facet BREP to DAGMC
# facet_brep_to_dagmc(
#     merged_brep_file,
#     materials_def="data/EUDEMO-materials.csv",
#     output_h5m_file=output_dagmc_file,
#     tolerance=0.001,
#     scale_factor=0.1,
#     tol_is_absolute=False,
# )

# then run (in terminal):
# make_watertight data/EUDEMO.h5m -o data/EUDEMO-watertight.h5m
