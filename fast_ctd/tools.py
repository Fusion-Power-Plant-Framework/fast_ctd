"""This module wraps the underlying C++ API.

It provides tooling around the API, such as default values, input validation and logging.
"""

import re
import subprocess as sp
import sys
from pathlib import Path

from fast_ctd_ext import occ_faceter, occ_merger, occ_step_to_brep

from fast_ctd.utils import none_guard, validate_file_exists, validate_file_extension

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

    validate_file_extension(input_step_file, (".stp", ".step"))
    validate_file_exists(input_step_file)
    validate_file_extension(output_brep_file, ".brep")

    minimum_volume = none_guard(minimum_volume, 1.0)
    fix_geometry = none_guard(fix_geometry, False)  # noqa: FBT003
    check_geometry = none_guard(check_geometry, True)  # noqa: FBT003

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

    validate_file_extension(input_brep_file, ".brep")
    validate_file_exists(input_brep_file)
    validate_file_extension(output_brep_file, ".brep")

    dist_tolerance = none_guard(dist_tolerance, 0.001)

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

    validate_file_extension(input_brep_file, ".brep")
    validate_file_exists(input_brep_file)
    validate_file_extension(output_h5m_file, ".h5m")
    validate_file_extension(materials_csv_file, ".csv")
    validate_file_exists(materials_csv_file)

    lin_deflection_tol = none_guard(lin_deflection_tol, 0.001)
    tol_is_absolute = none_guard(tol_is_absolute, False)  # noqa: FBT003
    ang_deflection_tol = none_guard(ang_deflection_tol, 0.5)
    scale_factor = none_guard(scale_factor, 0.1)

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
    h5m_file: StrPath, output_h5m_file: StrPath
) -> sp.CompletedProcess[str]:
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
    """
    make_watertight_bin_path = Path(sys.executable).parent / "make_watertight"
    h5m_file = Path(h5m_file)
    output_h5m_file = Path(output_h5m_file)

    validate_file_exists(
        make_watertight_bin_path,
        "Is DAGMC/OpenMC (with DAGMC) installed in your Python env?",
    )
    validate_file_extension(h5m_file, ".h5m")
    validate_file_exists(h5m_file)
    validate_file_extension(output_h5m_file, ".h5m")

    return sp.run(  # noqa: S603
        [
            make_watertight_bin_path.as_posix(),
            h5m_file.as_posix(),
            "-o",
            output_h5m_file.as_posix(),
        ],
        check=False,
        text=True,
        capture_output=True,
    )


def check_watertight(
    h5m_file: StrPath, tolerance: float | None = None
) -> sp.CompletedProcess[str]:
    """Check if a geometry is watertight using the check_watertight tool.

    This function utilizes the `check_watertight` from tool from DAGMC
    to process a given `.h5m` and check if the geometry is watertight.

    Args:
        h5m_file: The path to the input `.h5m` file to be processed.
        tolerance: The tolerance for the check. If None, the default value is used.

    Returns:
        A subprocess.CompletedProcess object containing the result of the command.

    Raises:
        FileNotFoundError: If the `check_watertight` binary or the input file
            does not exist.
        ValueError: If the input file does not have a `.h5m` extension.
    """
    check_watertight_bin_path = Path(sys.executable).parent / "check_watertight"

    h5m_file = Path(h5m_file)

    validate_file_exists(
        check_watertight_bin_path,
        "Is DAGMC/OpenMC (with DAGMC) installed in your Python env?",
    )
    validate_file_extension(h5m_file, ".h5m")
    validate_file_exists(h5m_file)

    cmds = [check_watertight_bin_path.as_posix(), "-o", h5m_file.as_posix()]
    if tolerance is not None:
        cmds.extend(["-t", str(tolerance)])

    return sp.run(  # noqa: S603
        cmds,
        check=False,
        text=True,
        capture_output=True,
    )


def decode_tightness_checks(stdout: str) -> list[float | int] | None:
    """Decode the stdout of the check_watertight/make_watertight subprocess.

    Args:
        stdout: The stdout of the subprocess.

    Returns:
        A list of floats or ints representing the percentage values.
        None if the output is empty or does not contain any decodable values.
    """
    percentages = re.findall(r"(\d+\.\d+|\d+)%", stdout)
    try:
        percentages = [float(p) for p in percentages]
    except ValueError:
        return None
    return [int(p) if p.is_integer() else p for p in percentages]


def mbconvert_vtk(
    h5m_file: StrPath,
    output_vtk_file: StrPath,
) -> sp.CompletedProcess[str]:
    """Convert a MOAB file to a VTK file using the mbconvert tool.

    Args:
        h5m_file: The path to the input `.h5m` file to be processed.
        output_vtk_file: The path to the output `.vtk` file.

    Raises:
        FileNotFoundError: If the `mbconvert` binary or the input file
            does not exist.
        ValueError: If the input or output file does not have a `.h5m` or `.vtk` extension.

    Notes:
        The `mbconvert` tool can convert between various file formats,
        run `mbconvert -l` to list the supported formats manually.
    """
    mbconvert_bin_path = Path(sys.executable).parent / "mbconvert"
    h5m_file = Path(h5m_file)
    output_vtk_file = Path(output_vtk_file)

    validate_file_exists(
        mbconvert_bin_path,
        "Is DAGMC/OpenMC (with DAGMC) installed in your Python env?",
    )
    validate_file_extension(h5m_file, ".h5m")
    validate_file_exists(h5m_file)
    validate_file_extension(output_vtk_file, ".vtk")

    return sp.run(  # noqa: S603
        [
            mbconvert_bin_path.as_posix(),
            h5m_file.as_posix(),
            "-f",
            "vtk",
            output_vtk_file.as_posix(),
        ],
        check=False,
        text=True,
        capture_output=True,
    )


def validate_dagmc_model_using_openmc(
    dagmc_file: str,
    materials_def: str | list | dict = "",
) -> bool:
    """Validate a DAGMC model using OpenMC."""
    return True
