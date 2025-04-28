#include <iostream>
#include "brep_faceter.hh"
#include "moab/ProgOptions.hpp"

#include <spdlog/spdlog.h>

static bool has_ending(std::string s, std::string ending)
{
  return s.rfind(ending) == s.length() - ending.length();
}

int occ_faceter(std::string input_brep_file,
                std::string materials_file,
                std::string output_h5m_file,
                double tolerance,
                double scale_factor,
                bool tol_is_absolute)
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

  spdlog::info("Starting occ_faceter:");
  spdlog::info("  input_brep_file: {}", input_brep_file);
  spdlog::info("  materials_file: {}", mat_file);
  spdlog::info("  output_h5m_file: {}", output_h5m_file);
  spdlog::info("  tolerance: {}", tolerance);
  spdlog::info("  scale_factor: {}", scale_factor);
  spdlog::info("  tol_is_absolute: {}", tol_is_absolute);

  brep_faceter(input_brep_file, mat_file, facet_tol, output_h5m_file, add_mat_ids, scale_factor);

  return 0;
}
