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

int occ_merger(std::string path_in, std::string path_out, double tolerance = 0.001)
{
	document inp;
	inp.load_brep_file(path_in.c_str());

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

		const auto result = salome_glue_shape(merged, tolerance);

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
			"number of shapes changed after merge, {} => {}",
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
			mn = std::min(v1, v2) * tolerance;

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

	spdlog::info("Writing merged shapes to file: {}", path_out);

	out.write_brep_file(path_out.c_str());

	return 0;
}
