#include <nanobind/nanobind.h>
#include <nanobind/stl/string.h>

#include "step_to_brep.hpp"
#include "occ_merger.hpp"
#include "occ_faceter.hpp"

namespace nb = nanobind;

NB_MODULE(fast_ctd_ext, m)
{
      m.doc() = "Python bindings for OpenCASCADE shape merging and faceting, "
                "for the creation of moab .h5m DAGMC models";

      m.def("occ_step_to_brep", &occ_step_to_brep,
            "Convert a STEP file to a BREP file",
            nb::arg("input_step_file"),
            nb::arg("output_brep_file"),
            nb::arg("minimum_volume") = 1,
            nb::arg("check_geometry") = true,
            nb::arg("fix_geometry") = false);

      m.def("occ_merger", &occ_merger,
            "Merge shapes from an input BREP file and write the result to an output BREP file",
            nb::arg("input_brep_file"), nb::arg("output_brep_file"), nb::arg("tolerance") = 0.001);

      m.def("occ_faceter", &occ_faceter,
            "Facet a geometry and save it to a MOAB h5m file",
            nb::arg("input_brep_file"),
            nb::arg("materials_file") = "",
            nb::arg("output_h5m_file") = "dagmc.h5m",
            nb::arg("tolerance") = 0.001,
            nb::arg("scale_factor") = 0.1,
            nb::arg("tol_is_absolute") = false);
}
