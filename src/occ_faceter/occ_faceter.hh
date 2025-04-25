#ifndef OCC_FACETER_HH
#define OCC_FACETER_HH

#include <string>

// Function to facet a geometry and save it to a MOAB h5m file
int occ_faceter(std::string input_brep_file,
                std::string materials_file = "",
                std::string output_h5m_file = "dagmc.h5m",
                double tolerance = 0.001,
                bool tol_is_absolute = false);

#endif // OCC_FACETER_HH