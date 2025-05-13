# fast_ctd - Fast CAD to DAGMC

This is a Python package for the fast conversion of STEP files to DAGMC models (.h5m files).

## Installation

todo

## Local development and installation

Development of this package is done in a conda environment. To create the environment, run (replace `conda` with `mamba` if you use that):

```bash
conda env create -f conda/environment.yml
```

This will create a conda environment called `fast_ctd` with all the dependencies C++ depencencies needed for development. It also locally installs the package in editable mode (compiles and installs the C++ extensions using `meson` under the hood).

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

If you change the C++ code, you usually don't need to do anything as `nanobind` can detect changes and will automatically recompile the extension. However, changing some files (like `binding.cpp`) will require a reinstall:

```bash
pip install -e .
```

### Meson compilation (directly)

Sometimes it's usefull to test the meson compilation directly, run this once:

```bash
meson setup --wipe builddir
meson compile -C builddir
```

and then

```bash
meson compile -C builddir
```

after that.
