#ifndef OCC_FACETER_HH
#define OCC_FACETER_HH

#include <string>

// Function to facet a geometry and save it to a MOAB h5m file
void occ_faceter(std::string input_brep_file,
                 std::string output_h5m_file,
                 std::string materials_file,
                 double lin_deflection_tol,
                 bool tol_is_absolute,
                 double ang_deflection_tol,
                 double scale_factor,
                 bool logging);

#endif // OCC_FACETER_HH
