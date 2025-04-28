#ifndef OCC_MERGER_HPP
#define OCC_MERGER_HPP

#include <string>

// Function to merge shapes from an input BREP file and write the result to an output BREP file
int occ_merger(std::string path_in, std::string path_out, double tolerance = 0.001);

#endif // OCC_MERGER_HPP
