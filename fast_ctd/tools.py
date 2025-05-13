"""This module wraps the underlying C++ API.

It provides tooling around the API, such as default values, input validation and logging.
"""

import re
import subprocess as sp
import sys
from pathlib import Path

from fast_ctd_ext import occ_faceter, occ_merger, occ_step_to_brep

from fast_ctd.logging import log_info, log_warn

try:
    import openmc
except ImportError:
    openmc = None

StrPath = str | Path


# todo:
# - add ests
# - add a README.md with usage examples
# - add openmc validation work
# - stub file generation and installing from nanobind: python -m nanobind.stubgen -m fast_ctd_ext -M py.typed
# - versioning the project with pyproject.toml and meson project version https://github.com/mesonbuild/meson/issues/688S


def _validate_file_extension(
    file_path: Path,
    expected_extension: str | tuple[str],
) -> None:
    """Validate the file extension of a given file path."""
    expected_extension = (
        (expected_extension,)
        if isinstance(expected_extension, str)
        else expected_extension
    )
    if file_path.suffix not in expected_extension:
        raise ValueError(
            f"File must be one of {expected_extension}, but got {file_path.suffix}",
        )


def _validate_file_exists(
    file_path: Path,
    suffix_stmt: str = "Are you in the right directory?",
) -> None:
    """Validate if a file exists."""
    if not file_path.exists():
        raise FileNotFoundError(f"'{file_path}' does not exist. {suffix_stmt}")


def step_to_brep(
    input_step_file: StrPath,
    output_brep_file: StrPath,
    *,
    minimum_volume: float = 1.0,
    fix_geometry: bool = False,
    check_geometry: bool = True,
    enable_logging: bool = False,
) -> list[tuple[str, str]]:
    """Convert a STEP file to a BREP file and return the BREP file path and component names.

    Args:
        input_step_file: The path to the input STEP file (.stp, .step).
        output_brep_file: The path to the output BREP file (.brep).
        minimum_volume:
            The minimum solid volume to be included in the BREP file.
            The unit is the unit of the geometry^3 (i.e. if your model is in mm,
            the unit is mm^3).
        fix_geometry:
            Attempts to fix small edges and gaps (by merging them) using the
            OCC ShapeFix_Wireframe API in the edges of solids.
            Also to "fix shapes" using the ShapeFix_Solid API.
            If you are not getting watertight models, try this option and enable
            logging.
        check_geometry:
            Checks if the geometry is valid using the OCC
            ShapeAnalysis_Shape::Check() API.
        enable_logging: Whether to enable logging in the C++ extension code.

    Returns:
        An ordered list of component groups and names in the BREP file.
    """
    input_step_file = Path(input_step_file)
    output_brep_file = Path(output_brep_file)

    _validate_file_extension(input_step_file, (".stp", ".step"))
    _validate_file_exists(input_step_file)
    _validate_file_extension(output_brep_file, ".brep")

    comps_info_list = occ_step_to_brep(
        input_step_file.as_posix(),
        output_brep_file.as_posix(),
        minimum_volume=minimum_volume,
        fix_geometry=fix_geometry,
        check_geometry=check_geometry,
        logging=enable_logging,
    )
    if not isinstance(comps_info_list, list):
        raise TypeError(
            f"Expected a list of component info, but got {type(comps_info_list)}",
        )

    ret_comps_info_list = []
    for ci in comps_info_list:
        if not isinstance(ci, str):
            raise TypeError(
                f"Expected a string component info, but got {type(ci)}",
            )
        group_no, comp_name = ci.split(",")
        ret_comps_info_list.append((group_no, comp_name))

    return ret_comps_info_list


def merge_brep_geometries(
    input_brep_file: StrPath,
    output_brep_file: StrPath,
    *,
    dist_tolerance: float = 0.001,
    enable_logging: bool = False,
) -> None:
    """Merge vertices in a BREP file and save the result to a new BREP file.

    Args:
        input_brep_file: The path to the input BREP file.
        output_brep_file: The path to the output BREP file.
        dist_tolerance:
            The distance tolerance for merging entities
            (vertices, edges, faces, etc.).
        enable_logging: Whether to enable logging in the C++ extension code.
    """
    input_brep_file = Path(input_brep_file)
    output_brep_file = Path(output_brep_file)

    _validate_file_extension(input_brep_file, ".brep")
    _validate_file_exists(input_brep_file)
    _validate_file_extension(output_brep_file, ".brep")

    occ_merger(
        input_brep_file.as_posix(),
        output_brep_file.as_posix(),
        dist_tolerance,
        enable_logging,
    )


def facet_brep_to_dagmc(
    input_brep_file: StrPath,
    output_h5m_file: StrPath,
    materials_csv_file: StrPath,
    *,
    lin_deflection_tol: float = 0.001,
    tol_is_absolute: bool = False,
    ang_deflection_tol: float = 0.5,
    scale_factor: float = 0.1,
    enable_logging: bool = False,
) -> None:
    """Facet .brep geometry and save it to a DAGMC MOAB .h5m file.

    Args:
        input_brep_file: The path to the input BREP file.
        output_h5m_file: The path to the output MOAB file (.h5m).
        materials_csv_file: The path to the materials definition file (.csv).
        lin_deflection_tol: Linear deflection tolerance for faceting.
        tol_is_absolute:
            Whether the lin_deflection_tol is absolute
            or relative to edge length.
        ang_deflection_tol: Angular deflection tolerance for faceting.
        scale_factor: Scale factor for the geometry. [may get removed]
        enable_logging: Whether to enable logging in the C++ extension code.

    Notes:
        `lin_deflection_tol`, `tol_is_absolute` and `ang_deflection_tol` all relate to
        mesh quality and are passed to the OCC BRepMesh_IncrementalMesh API.
        More information about BRepMesh_IncrementalMesh can be found in the OCC documentation.
    """
    input_brep_file = Path(input_brep_file)
    output_h5m_file = Path(output_h5m_file)
    materials_csv_file = Path(materials_csv_file)

    _validate_file_extension(input_brep_file, ".brep")
    _validate_file_exists(input_brep_file)
    _validate_file_extension(output_h5m_file, ".h5m")
    _validate_file_extension(materials_csv_file, ".csv")
    _validate_file_exists(materials_csv_file)

    occ_faceter(
        input_brep_file.as_posix(),
        output_h5m_file.as_posix(),
        materials_csv_file.as_posix(),
        lin_deflection_tol,
        tol_is_absolute,
        ang_deflection_tol,
        scale_factor,
        enable_logging,
    )


def make_watertight(
    h5m_file: StrPath,
    output_h5m_file: StrPath,
) -> None:
    """Make a geometry watertight using the make_watertight tool.

    This function utilizes the `make_watertight` from tool from DAGMC
    to process a given `.h5m` and attempt to fix any leaky volumes in the
    geometry file, ensuring it is watertight. If the
    geometry is cannot be made watertight, log files are generated for further inspection.

    Args:
        h5m_file: The path to the input `.h5m` file to be processed.
        output_h5m_file: The path to the output `.h5m` file.

    Raises:
        FileNotFoundError: If the `make_watertight` binary or the input file
            does not exist.
        ValueError: If the input or output file does not have a `.h5m` extension.
        subprocess.CalledProcessError: If the `make_watertight` tool fails to
            execute successfully.
    """
    make_watertight_bin_path = Path(sys.executable).parent / "make_watertight"
    h5m_file = Path(h5m_file)
    output_h5m_file = Path(output_h5m_file)

    _validate_file_exists(
        make_watertight_bin_path,
        "Is DAGMC/OpenMC (with DAGMC) installed in your Python env?",
    )
    _validate_file_extension(h5m_file, ".h5m")
    _validate_file_exists(h5m_file)
    _validate_file_extension(output_h5m_file, ".h5m")

    make_watertight = sp.run(  # noqa: S603
        [
            make_watertight_bin_path.as_posix(),
            h5m_file.as_posix(),
            "-o",
            output_h5m_file.as_posix(),
        ],
        check=True,
        capture_output=True,
    )

    # Extract percentage values from the stdout
    # they come from check_watertight, run at the end of make_watertight
    mw_std_o = make_watertight.stdout.decode()
    percentages = re.findall(r"(\d+\.\d+|\d+)%", mw_std_o)
    percentages = [float(p) for p in percentages]

    if all(int(p) == 0 if p.is_integer() else False for p in percentages):
        log_info(
            "make_watertight finished successfully, with check_watertight showing no leaky volumes.",
            more_info={"output_file": output_h5m_file},
        )
    else:
        stout_log_dump_path = h5m_file.with_name(
            f"{h5m_file.stem}-make_watertight.stdout.txt",
        )
        sterr_log_dump_path = h5m_file.with_name(
            f"{h5m_file.stem}-make_watertight.stderr.txt",
        )
        with stout_log_dump_path.open("w") as log_dump:
            log_dump.write(mw_std_o)
        with sterr_log_dump_path.open("w") as log_dump:
            log_dump.write(make_watertight.stderr.decode())

        log_warn(
            "make_watertight finished successfully, but the model is not watertight. "
            "Check log files for further details.",
            more_info={
                "output_file": output_h5m_file,
                "stout_log": stout_log_dump_path,
                "sterr_log": sterr_log_dump_path,
            },
        )


def validate_dagmc_model_using_openmc(
    dagmc_file: str,
    materials_def: str | list | dict = "",
) -> bool:
    """Validate a DAGMC model using OpenMC."""
    return True
