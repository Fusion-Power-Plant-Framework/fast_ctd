#include <array>
#include <cstdlib>
#include <cassert>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

#include <STEPCAFControl_Reader.hxx>
#include <XCAFApp_Application.hxx>
#include <XCAFDoc.hxx>
#include <XCAFDoc_DocumentTool.hxx>
#include <XCAFDoc_ShapeTool.hxx>
#include <XCAFDoc_ColorTool.hxx>
#include <XCAFDoc_Material.hxx>
#include <XCAFDoc_Color.hxx>

#include <TDocStd_Document.hxx>
#include <TDataStd_Name.hxx>
#include <TDataStd_TreeNode.hxx>
#include <TCollection_AsciiString.hxx>

#include <BRepTools.hxx>
#include <BRep_Builder.hxx>

#include <TopExp_Explorer.hxx>
#include <TopoDS_Builder.hxx>
#include <TopoDS_Shape.hxx>
#include <TopoDS_CompSolid.hxx>

#include <Units_Quantity.hxx>
#include <Quantity_Color.hxx>

#include <ShapeFix_Shape.hxx>
#include <ShapeFix_Wireframe.hxx>

#include <spdlog/spdlog.h>

#include "geometry.hpp"
#include "utils.hpp"

static void
assign_cstring(std::string &dst, const TCollection_ExtendedString &src)
{
	dst.resize((size_t)src.LengthOfCString());
	auto str = (char *)dst.data();
	size_t len = (size_t)src.ToUTF8CString(str);

	if (len > dst.length())
	{
		spdlog::error("potential memory corruption from utf8 string overflow. expected={} bytes got={}", dst.length(), len);
		std::exit(1);
	}
	else if (len < dst.length())
	{
		spdlog::warn("utf8 string not the specified length. expected={} bytes got={}", dst.length(), len);
		dst.resize(len);
	}
}

static inline void
assign_cstring(std::string &dst, const TCollection_AsciiString &src)
{
	dst.assign(src.ToCString(), (size_t)src.Length());
}

static inline void
assign_cstring(std::string &dst, const TCollection_HAsciiString &src)
{
	dst.assign(src.ToCString(), (size_t)src.Length());
}

static bool
get_label_name(const TDF_Label &label, std::string &name)
{
	Handle(TDataStd_Name) attr;
	if (!label.FindAttribute(TDataStd_Name::GetID(), attr))
	{
		return false;
	}

	assign_cstring(name, attr->Get());

	return true;
}

static bool
get_color_info(const TDF_Label &label, std::string &hexcode)
{
	static const std::array<XCAFDoc_ColorType, 3> types = {
		XCAFDoc_ColorGen,
		XCAFDoc_ColorSurf,
		XCAFDoc_ColorCurv,
	};

	Handle(TDataStd_TreeNode) node;
	Handle(XCAFDoc_Color) attr;

	for (const auto type : types)
	{
		if (label.FindAttribute(XCAFDoc::ColorRefGUID(type), node) &&
			node->HasFather() &&
			node->Father()->Label().FindAttribute(XCAFDoc_Color::GetID(), attr))
		{
			assign_cstring(hexcode, Quantity_ColorRGBA::ColorToHex(attr->GetColorRGBA()));

			return true;
		}
	}

	return false;
}

static bool
get_material_info(const TDF_Label &label, std::string &name, double &density)
{
	Handle(TDataStd_TreeNode) node;
	Handle(XCAFDoc_Material) attr;

	if (label.FindAttribute(XCAFDoc::MaterialRefGUID(), node) &&
		node->HasFather() &&
		node->Father()->Label().FindAttribute(XCAFDoc_Material::GetID(), attr))
	{
		assign_cstring(name, attr->GetName());
		density = attr->GetDensity();

		return true;
	}

	return false;
}

class collector
{
	document doc;

	double minimum_volume;

	int n_groups, n_small, n_negative_volume;

	std::vector<std::string> added_comps_info;

	void add_solids(const TDF_Label &label)
	{
		n_groups += 1;

		std::string color;
		std::string label_name{"unnammed"};
		std::string material_name{"unknown"};
		double material_density = 0;

		get_label_name(label, label_name);
		get_color_info(label, color);
		get_material_info(label, material_name, material_density);

		TopoDS_Shape doc_shape;
		if (!XCAFDoc_ShapeTool::GetShape(label, doc_shape))
		{
			spdlog::error("unable to get shape {}", label_name);
			std::exit(1);
		}

		// add the solids to our list of things to do
		for (TopExp_Explorer ex{doc_shape, TopAbs_SOLID}; ex.More(); ex.Next())
		{
			spdlog::trace("calculating volume of shape");
			const TopoDS_Shape &shape = ex.Current();
			const auto volume = volume_of_shape(shape);
			spdlog::trace("done calculating volume of shape");
			if (volume < minimum_volume)
			{
				if (volume < 0)
				{
					n_negative_volume += 1;
					spdlog::info("ignoring part of shape '{}' due to negative volume, {}", label_name, volume);
				}
				else
				{
					n_small += 1;
					spdlog::info("ignoring part of shape '{}' because it's too small, {} < {}", label_name, volume, minimum_volume);
				}
				continue;
			}

			doc.solid_shapes.emplace_back(shape);

			added_comps_info.push_back(
				std::to_string(n_groups) + ',' + label_name);
		}
	}

public:
	collector(double minimum_volume) : minimum_volume{minimum_volume},
									   n_groups{0}, n_small{0}, n_negative_volume{0}
	{
	}

	void add_label(XCAFDoc_ShapeTool &shapetool, const TDF_Label &label)
	{
		if (shapetool.IsAssembly(label))
		{
			// loop over other labelled parts
			TDF_LabelSequence components;
			XCAFDoc_ShapeTool::GetComponents(label, components);
			for (auto const &comp : components)
			{
				add_label(shapetool, comp);
			}
		}
		else
		{
			add_solids(label);
		}
	}

	void log_summary()
	{
		spdlog::info("enumerated {} groups, resulting in {} solids", n_groups, doc.solid_shapes.size());
		if (n_small > 0)
		{
			spdlog::warn("{} solids were excluded because they were too small", n_small);
		}
		if (n_negative_volume > 0)
		{
			spdlog::warn("{} solids were excluded because they had negative volume", n_negative_volume);
		}
	}

	void fix_shapes(double precision, double max_tolerance)
	{
		for (auto &shape : doc.solid_shapes)
		{
			ShapeFix_Shape fixer{shape};
			fixer.SetPrecision(precision);
			fixer.SetMaxTolerance(max_tolerance);
			auto fixed = fixer.Perform();
			if (fixed)
			{
				std::ostringstream log;

				log << "shapefixer=" << fixed;
				if (fixer.Status(ShapeExtend_DONE1))
					log << ", some free edges were fixed";
				if (fixer.Status(ShapeExtend_DONE2))
					log << ", some free wires were fixed";
				if (fixer.Status(ShapeExtend_DONE3))
					log << ", some free faces were fixed";
				if (fixer.Status(ShapeExtend_DONE4))
					log << ", some free shells were fixed";
				if (fixer.Status(ShapeExtend_DONE5))
					log << ", some free solids were fixed";
				if (fixer.Status(ShapeExtend_DONE6))
					log << ", shapes in compound(s) were fixed";

				spdlog::info(log.str());

				shape = fixer.Shape();
			}
		}
	}

	void fix_wireframes(double precision, double max_tolerance)
	{
		int nshape = 0;
		for (auto &shape : doc.solid_shapes)
		{
			ShapeFix_Wireframe fixer{shape};
			fixer.SetPrecision(precision);
			fixer.SetMaxTolerance(max_tolerance);
			fixer.ModeDropSmallEdges() = Standard_True;
			auto small_res = fixer.FixSmallEdges();
			auto gap_res = fixer.FixWireGaps();

			if (!(small_res || gap_res))
			{
				continue;
			}

			std::ostringstream log;
			log << "Fixing shape " << nshape++;

			if (small_res)
			{
				if (fixer.StatusSmallEdges(ShapeExtend_OK))
					log << ", no small edges were found";
				if (fixer.StatusSmallEdges(ShapeExtend_DONE1))
					log << ", some small edges were fixed";
				if (fixer.StatusSmallEdges(ShapeExtend_FAIL1))
					log << ", failed to fix some small edges";
			}

			if (gap_res)
			{
				if (fixer.StatusWireGaps(ShapeExtend_OK))
					log << ", no gaps were found";
				if (fixer.StatusWireGaps(ShapeExtend_DONE1))
					log << ", some gaps in 3D were fixed";
				if (fixer.StatusWireGaps(ShapeExtend_DONE2))
					log << ", some gaps in 2D were fixed";
				if (fixer.StatusWireGaps(ShapeExtend_FAIL1))
					log << ", failed to fix some gaps in 3D";
				if (fixer.StatusWireGaps(ShapeExtend_FAIL2))
					log << ", failed to fix some gaps in 2D";
			}

			spdlog::info(log.str());

			shape = fixer.Shape();
		}
	}

	void validate_geometry()
	{
		auto ninvalid = doc.count_invalid_shapes();
		if (ninvalid)
		{
			spdlog::error("{} shapes were not valid", ninvalid);
			std::exit(1);
		}
		spdlog::info("Geometry checks passed");
	}

	void write_brep_file(const char *path)
	{
		doc.write_brep_file(path);
	}

	const std::vector<std::string> &get_added_comps_info() const
	{
		return added_comps_info;
	}
};

static void
load_step_file(const char *path, collector &col)
{
	auto app = XCAFApp_Application::GetApplication();

	STEPCAFControl_Reader reader;
	reader.SetNameMode(true);
	reader.SetColorMode(true);
	reader.SetMatMode(true);

	spdlog::info("Reading step file {}", path);

	if (reader.ReadFile(path) != IFSelect_RetDone)
	{
		spdlog::error("unable to read STEP file {}", path);
		std::exit(1);
	}

	spdlog::debug("transferring into doc");

	Handle(TDocStd_Document) doc;
	app->NewDocument("MDTV-XCAF", doc);
	if (!reader.Transfer(doc))
	{
		spdlog::error("failed to Transfer into document");
		std::exit(1);
	}

	spdlog::debug("getting toplevel shapes");

	TDF_LabelSequence toplevel;
	auto shapetool = XCAFDoc_DocumentTool::ShapeTool(doc->Main());

	shapetool->GetFreeShapes(toplevel);

	spdlog::debug("loading {} toplevel shape(s)", toplevel.Length());
	for (const auto &label : toplevel)
	{
		col.add_label(*shapetool, label);
	}
}

std::vector<std::string> occ_step_to_brep(
	std::string input_step_file,
	std::string output_brep_file,
	double minimum_volume,
	bool check_geometry,
	bool fix_geometry,
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

	if (minimum_volume < 0)
	{
		spdlog::error("Minimum shape volume ({}) should not be negative", minimum_volume);
		std::exit(1);
	}

	spdlog::info("");
	spdlog::info("Starting occ_step_to_brep:");
	spdlog::info("  input_step_file: {}", input_step_file);
	spdlog::info("  output_brep_file: {}", output_brep_file);
	spdlog::info("  minimum_volume: {}", minimum_volume);
	spdlog::info("  check_geometry: {}", check_geometry);
	spdlog::info("  fix_geometry: {}", fix_geometry);
	spdlog::info("");

	collector doc(minimum_volume);
	load_step_file(input_step_file.c_str(), doc);

	doc.log_summary();

	if (fix_geometry)
	{
		spdlog::debug("fixing wireframes");
		doc.fix_wireframes(0.01, 0.00001);
		spdlog::debug("fixing shapes");
		doc.fix_shapes(0.01, 0.00001);
	}

	if (check_geometry)
	{
		spdlog::debug("Checking geometry");
		doc.validate_geometry();
	}

	spdlog::info("writing brep file {}", output_brep_file);

	doc.write_brep_file(output_brep_file.c_str());

	return doc.get_added_comps_info();
}
