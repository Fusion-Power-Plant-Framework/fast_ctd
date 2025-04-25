# print("merging")
# merge("EUDEMO.brep", "EUDEMO-merged.brep")

# print("faceting")
# facet("EUDEMO-merged.brep", "EUDEMO-materials.csv")

from fast_ctd import stp_to_dagmc

stp_to_dagmc(
    input_brep="data/EUDEMO.brep",
    output_brep="data/EUDEMO-merged.brep",
    materials_csv="data/EUDEMO-materials.csv",
)
