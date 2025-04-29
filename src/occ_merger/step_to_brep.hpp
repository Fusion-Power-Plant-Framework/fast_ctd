#ifndef STEP_TO_BREP_HPP
#define STEP_TO_BREP_HPP

#include <string>

/**
 * Converts a STEP file to a BREP file.
 *
 * @param input_path Path to the input STEP file.
 * @param output_path Path to the output BREP file.
 * @param minimum_volume Minimum volume threshold for shapes to be included.
 * @param check_geometry Whether to check the geometry for validity.
 * @param fix_geometry Whether to attempt to fix geometry issues.
 * @return True if the conversion was successful, false otherwise.
 */
int occ_step_to_brep(
    std::string input_step_file,
    std::string output_brep_file,
    double minimum_volume,
    bool check_geometry,
    bool fix_geometry);

#endif // STEP_TO_BREP_HPP
