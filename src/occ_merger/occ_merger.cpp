#include <algorithm>
#include <cassert>
#include <cmath>

#include <spdlog/spdlog.h>

#include <BRepTools.hxx>

#include <TopoDS_Iterator.hxx>
#include <TopoDS_Builder.hxx>
#include <TopoDS_Compound.hxx>
#include <TopoDS_Solid.hxx>
#include <TopoDS_Shape.hxx>

#include "salome/geom_gluer.hxx"

#include "geometry.hpp"
#include "utils.hpp"

void occ_merger(
	std::string input_brep_file,
	std::string output_brep_file,
	double dist_tolerance,
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

	spdlog::info("");
	spdlog::info("Starting occ_merger:");
	spdlog::info("  input_brep_file: {}", input_brep_file);
	spdlog::info("  output_brep_file: {}", output_brep_file);
	spdlog::info("  dist_tolerance: {}", dist_tolerance);
	spdlog::info("");

	document inp;
	inp.load_brep_file(input_brep_file.c_str());

	spdlog::info("Brep loaded");

	TopoDS_Compound merged;
	TopoDS_Builder builder;
	builder.MakeCompound(merged);
	for (const auto &shape : inp.solid_shapes)
	{
		builder.Add(merged, shape);
	}

	spdlog::info("Compound created");

	document out;

	{
		spdlog::info("Merging shapes");

		const auto result = salome_glue_shape(merged, dist_tolerance);

		if (result.IsNull())
		{
			spdlog::error("Failed to merge shapes");
			std::exit(1);
		}

		for (TopoDS_Iterator it{result}; it.More(); it.Next())
		{
			out.solid_shapes.emplace_back(it.Value());
		}
	}

	if (inp.solid_shapes.size() != out.solid_shapes.size())
	{
		spdlog::error(
			"Number of shapes changed after merge, {} => {}",
			inp.solid_shapes.size(),
			out.solid_shapes.size());
		std::exit(1);
	}

	size_t num_changed = 0;
	for (size_t i = 0; i < inp.solid_shapes.size(); i++)
	{
		const double
			v1 = volume_of_shape(inp.solid_shapes[i]),
			v2 = volume_of_shape(out.solid_shapes[i]),
			mn = std::min(v1, v2) * dist_tolerance;

		if (std::fabs(v1 - v2) > mn)
		{
			spdlog::warn("non-trivial change in volume during merge, {} => {}", v1, v2);
			num_changed += 1;
		}
	}

	if (num_changed > 0)
	{
		std::exit(1);
	}

	spdlog::info("Writing .brep output file {}", output_brep_file);

	out.write_brep_file(output_brep_file.c_str());
}
