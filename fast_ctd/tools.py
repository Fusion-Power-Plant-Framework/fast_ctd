import subprocess

from fast_ctd_ext import occ_faceter, occ_merger, occ_step_to_brep

try:
    import openmc
except ImportError:
    openmc = None


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


def step_to_brep(
    input_step_file: str, output_brep_file: str | None = None
) -> tuple[str, list[str]]:
    """Convert a STEP file to a BREP file and return the BREP file path and component names"""
    occ_step_to_brep(
        input_step_file,
        output_brep_file,
        minimum_volume=1,
        check_geometry=True,
        fix_geometry=False,
    )
    return output_brep_file, [""]


def merge_brep_geometries(
    input_brep_file, output_brep_file=None, tolerance: float = 0.001
) -> str:
    """Merge vertices in a BREP file and save the result to a new BREP file"""
    occ_merger(input_brep_file, output_brep_file, tolerance)


def facet_brep_to_dagmc(
    input_brep_file: str,
    materials_def: str | list | dict = "",
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


def make_watertight(h5m_file: str, output_h5m_file: str = None) -> None:
    """Make a geometry watertight using the make_watertight tool"""
    try:
        make_watertight = subprocess.run(
            ["make_watertight", h5m_file, "-o", output_h5m_file],
            check=True,
            capture_output=True,
        )
    except subprocess.CalledProcessError as e:
        print("Error running make_watertight")
        print(e)
    print(make_watertight.stdout.decode())


def validate_dagmc_model_using_openmc(
    dagmc_file: str,
    materials_def: str | list | dict = "",
) -> bool:
    """Validate a DAGMC model using the validate_dagmc tool"""
