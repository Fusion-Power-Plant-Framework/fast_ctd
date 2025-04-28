#ifndef OCC_MERGER_HPP
#define OCC_MERGER_HPP

#include <string>

// Function to merge shapes from an input BREP file and write the result to an output BREP file
int occ_merger(std::string input_brep_file, std::string output_brep_file, double tolerance);

#endif // OCC_MERGER_HPP
