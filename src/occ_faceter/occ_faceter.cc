#include <iostream>
#include "brep_faceter.hh"
#include "moab/ProgOptions.hpp"

#include <spdlog/spdlog.h>

static bool has_ending(std::string s, std::string ending)
{
  return s.rfind(ending) == s.length() - ending.length();
}

int occ_faceter(std::string input_brep_file,
                std::string materials_file = "",
                std::string output_h5m_file = "dagmc.h5m",
                double tolerance = 0.001,
                double scale_factor = 0.1,
                bool tol_is_absolute = false)
{
  bool add_mat_ids = true;

  if (!has_ending(input_brep_file, ".brep"))
  {
    std::cerr << "Warning: input file path should end with .brep" << std::endl;
    return 1;
  }

  if (!has_ending(output_h5m_file, ".h5m"))
  {
    std::cerr << "Warning: output file path should end with .h5m" << std::endl;
    return 1;
  }

  std::string mat_file = materials_file.empty()
                             ? input_brep_file.substr(0, input_brep_file.rfind(".brep")) + "-materials.txt"
                             : materials_file;

  FacetingTolerance facet_tol(tolerance, tol_is_absolute);

  spdlog::info(
      "Starting occ_faceter:\n"
      "  input_brep_file: {}\n"
      "  materials_file: {}\n"
      "  output_h5m_file: {}\n"
      "  tolerance: {}\n"
      "  scale_factor: {}\n"
      "  tol_is_absolute: {}",
      input_brep_file,
      materials_file.empty() ? "not provided" : materials_file,
      output_h5m_file,
      tolerance,
      scale_factor,
      tol_is_absolute);

  brep_faceter(input_brep_file, mat_file, facet_tol, output_h5m_file, add_mat_ids, scale_factor);

  return 0;
}
