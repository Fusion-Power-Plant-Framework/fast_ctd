#include <iostream>
#include "moab/ProgOptions.hpp"

#include <spdlog/spdlog.h>

#include "brep_faceter.hpp"

static bool has_ending(std::string s, std::string ending)
{
  return s.rfind(ending) == s.length() - ending.length();
}

void occ_faceter(std::string input_brep_file,
                 std::string output_h5m_file,
                 std::string materials_file,
                 double lin_deflection_tol,
                 bool tol_is_absolute,
                 double ang_deflection_tol,
                 double scale_factor,
                 bool logging)
{
  if (logging)
  {
    spdlog::set_level(spdlog::level::debug);
  }
  else
  {
    spdlog::set_level(spdlog::level::err);
  }

  bool add_mat_ids = true;

  if (!has_ending(input_brep_file, ".brep"))
  {
    spdlog::error("Input file path should end with .brep: {}", input_brep_file);
    std::exit(1);
  }
  if (!has_ending(output_h5m_file, ".h5m"))
  {
    spdlog::error("Output file path should end with .h5m: {}", output_h5m_file);
    std::exit(1);
  }

  if (lin_deflection_tol < 0)
  {
    spdlog::error("Linear deflection tolerance ({}) should not be negative", lin_deflection_tol);
    std::exit(1);
  }
  if (ang_deflection_tol < 0)
  {
    spdlog::error("Angular deflection tolerance ({}) should not be negative", ang_deflection_tol);
    std::exit(1);
  }

  FacetingTolerance facet_tol(lin_deflection_tol, tol_is_absolute, ang_deflection_tol);

  spdlog::info("");
  spdlog::info("Starting occ_faceter:");
  spdlog::info("  input_brep_file: {}", input_brep_file);
  spdlog::info("  output_h5m_file: {}", output_h5m_file);
  spdlog::info("  materials_file: {}", materials_file);
  spdlog::info("  lin_deflection_tol: {}", lin_deflection_tol);
  spdlog::info("  tol_is_absolute (false -> lin_deflection_tol is relative to the edge length): {}", tol_is_absolute);
  spdlog::info("  ang_deflection_tol: {}", ang_deflection_tol);
  spdlog::info("  scale_factor: {}", scale_factor);
  spdlog::info("");

  brep_faceter(input_brep_file, materials_file, facet_tol, output_h5m_file, add_mat_ids, scale_factor);
}
