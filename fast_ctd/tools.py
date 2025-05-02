"""This module wraps the underlying C++ API.

It provides tooling around the API, such as default values, input validation and logging.
"""

import logging
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
# - move all defaulting etc. to the python side
# - add ests
# - chagne tolerance in merger to dist_tolerance
# - docstrings for every function, explaing what they do and what the inputs are and returns
# - add a README.md with usage examples
# - add openmc validation work
# - for stp_to_brep, impl. the outputting of component names (i.e. the ordering and the names)
# - for facet_brep_to_dagmc, implement the mateirals_def, can pass in a file (path to list of materials) or a dict (mapping comp name to material name), or a list of materials
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
    check_geometry: bool = True,
    fix_geometry: bool = False,
    extension_logging: bool = False,
) -> list[str]:
    """Convert a STEP file to a BREP file and return the BREP file path and component names.

    Args:
        input_step_file: The path to the input STEP file.
        output_brep_file: The path to the output BREP file.
        minimum_volume: The minimum volume for components to be included in the BREP file.
        check_geometry: Whether to check the geometry of the STEP file.
        fix_geometry: Whether to fix the geometry of the STEP file.
        extension_logging: Whether to enable logging in the C++ extension

    Returns:
        An ordered list of component names in the BREP file.
    """
    input_step_file = Path(input_step_file)
    output_brep_file = Path(output_brep_file)

    _validate_file_extension(input_step_file, (".stp", ".step"))
    _validate_file_exists(input_step_file)
    _validate_file_extension(output_brep_file, ".brep")

    occ_step_to_brep(
        input_step_file.as_posix(),
        output_brep_file.as_posix(),
        minimum_volume=minimum_volume,
        check_geometry=check_geometry,
        fix_geometry=fix_geometry,
        logging=extension_logging,
    )
    return [""]


def merge_brep_geometries(
    input_brep_file,
    output_brep_file=None,
    *,
    dist_tolerance: float = 0.001,
    extension_logging: bool = False,
) -> None:
    """Merge vertices in a BREP file and save the result to a new BREP file"""
    occ_merger(input_brep_file, output_brep_file, dist_tolerance)


def facet_brep_to_dagmc(
    input_brep_file: str,
    materials_def: str | Path | list[str],
    output_h5m_file: str = "dagmc.h5m",
    tolerance: float = 0.001,
    scale_factor: float = 0.1,
    *,
    tol_is_absolute: bool = False,
) -> int:
    """Facet a geometry and save it to a MOAB h5m file"""
    occ_faceter(
        input_brep_file,
        materials_def,
        output_h5m_file,
        tolerance,
        scale_factor,
        tol_is_absolute,
    )


def make_watertight(
    h5m_file: str | Path,
    output_h5m_file: str | Path | None = None,
) -> Path:
    """Make a geometry watertight using the make_watertight tool.

    This function utilizes the `make_watertight` tool to process a given `.h5m`
    geometry file and ensure it is watertight. The function validates the input
    paths, runs the tool, and checks the output for any leaky volumes. If the
    geometry is not watertight, log files are generated for further inspection.

    Args:
        h5m_file: The path to the input `.h5m` file to be processed.
        output_h5m_file: The path to the output
            `.h5m` file. If not provided, the output file will be named based
            on the input file with a `-wt` suffix.

    Returns:
        The path to the output `.h5m` file.

    Raises:
        FileNotFoundError: If the `make_watertight` binary or the input file
            does not exist.
        ValueError: If the input or output file does not have a `.h5m` extension.
        subprocess.CalledProcessError: If the `make_watertight` tool fails to
            execute successfully.
    """
    make_watertight_bin_path = Path(sys.executable).parent / "make_watertight"
    h5m_file = Path(h5m_file)
    output_h5m_file = (
        Path(output_h5m_file)
        if output_h5m_file
        else h5m_file.with_stem(h5m_file.stem + "-wt")
    )

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
            "Check log files for details.",
            more_info={
                "output_file": output_h5m_file,
                "stout_log": stout_log_dump_path,
                "sterr_log": sterr_log_dump_path,
            },
        )

    return output_h5m_file


def validate_dagmc_model_using_openmc(
    dagmc_file: str,
    materials_def: str | list | dict = "",
) -> bool:
    """Validate a DAGMC model using the validate_dagmc tool"""
