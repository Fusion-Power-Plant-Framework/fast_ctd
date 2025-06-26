# fast_ctd - Fast CAD to DAGMC

This is a Python package for the fast conversion of STEP files to DAGMC models (.h5m files).

This package wraps certain C++ modules from the following projects that enable this conversion:

- [overlap_checker](https://github.com/ukaea/overlap_checker)
- [occ_faceter](https://github.com/makeclean/occ_faceter)

And exposes a Python interface to them using [nanobind](https://nanobind.readthedocs.io/en/latest/).

## Quick start

This project requires the following packages pre-installed:

- cmake
- OpenCASCADE 7.8.0 or later
- MOAB 5.0.0 or later

The recommended way to install these dependencies is to use `conda` and can be installed with the following (`openmc` bundles `MOAB`):

```bash
conda install -c conda-forge 'openmc>=0.15.2=dagmc_*' 'occt>=7.8.0=all_*' cmake
```

Use `pip` to remotely install the package:

```bash
pip install fast_ctd@git+https://github.com/Fusion-Power-Plant-Framework/fast_ctd@main
```

or clone the repository and install it locally:

```bash
git clone https://github.com/Fusion-Power-Plant-Framework/fast_ctd.git
cd fast_ctd
pip install .
```

## Development installation

Development of this package is done in a conda environment.

To create the environment, run:

```bash
conda env create -f conda/environment.yml
```

This will create a conda environment called `fast_ctd`. It locally installs the package in editable mode (`meson` is used to compile the C++ code with `nanobind` to create the extension under the hood).

Activate the `fast_ctd` environment (or set it as the default interpreter in your IDE):

```bash
conda activate fast_ctd
```

then try running an example:

```bash
cd examples
python stp_to_dagmc_workflow.py
```

### Changing the C++ source

The editable install of the package (included in the conda `environment.yml`) means you can edit the C++ source code and run the Python code without needing to reinstall (rebuild) the package. `nanobind` automatically detects changes and recompiles the extension when you run the Python code.

However, changing some files (for example `binding.cpp`) will require a reinstall, which can be done with:

```bash
pip install -e .
```

`nanobind` will throw an error when you try to run the Python code without reinstalling after changing files that require it.

### Meson compilation (directly)

Sometimes it's useful to test the meson compilation directly, run this once:

```bash
meson setup --wipe builddir
meson compile -C builddir
```

and then

```bash
meson compile -C builddir
```

after that.
