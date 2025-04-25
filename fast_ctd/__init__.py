from fast_ctd_ext import facet, merge


def stp_to_dag(input_brep, output_brep, materials_csv):
    """
    Facet and merge a BREP file.

    Parameters:
    - input_brep: str, path to the input BREP file.
    - output_brep: str, path to the output BREP file.
    - materials_csv: str, path to the materials CSV file.
    """
    print("merging")
    merge(input_brep, output_brep)

    print("faceting")
    facet(output_brep, materials_csv)
