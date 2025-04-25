// Contains code taken from http://www.salome-platform.org/

// The original code had the following license:

// Copyright (C) 2007-2021  CEA/DEN, EDF R&D, OPEN CASCADE
//
// Copyright (C) 2003-2007  OPEN CASCADE, EADS/CCR, LIP6, CEA/DEN,
// CEDRAT, EDF R&D, LEG, PRINCIPIA R&D, BUREAU VERITAS
//
// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 2.1 of the License, or (at your option) any later version.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public
// License along with this library; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
//
// See http://www.salome-platform.org/ or email : webmaster.salome@opencascade.com
//

#include <exception>
#include <stdexcept>
#include <optional>

#include <Standard.hxx>
#include <Standard_Macro.hxx>
#include <Standard_Boolean.hxx>
#include <Standard_TypeDef.hxx>

#include <gp_Pnt.hxx>
#include <gp_Pnt2d.hxx>

#include <BRepLib.hxx>
#include <BRepTools.hxx>
#include <BRep_Builder.hxx>

#include <NCollection_UBTree.hxx>
#include <NCollection_UBTreeFiller.hxx>
#include <NCollection_IndexedDataMap.hxx>

#include <TColStd_ListOfInteger.hxx>
#include <TColStd_MapOfInteger.hxx>

#include <IntTools_Context.hxx>
#include <IntTools_Tools.hxx>
#include <BOPTools_AlgoTools.hxx>
#include <BOPTools_AlgoTools2D.hxx>

#include <TopoDS.hxx>
#include <TopoDS_Iterator.hxx>
#include <TopoDS_Builder.hxx>
#include <TopoDS_Compound.hxx>
#include <TopoDS_Shape.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Vertex.hxx>

#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopAbs_ShapeEnum.hxx>

#include <TopTools_MapOfShape.hxx>
#include <TopTools_ListOfShape.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopTools_DataMapOfShapeListOfShape.hxx>
#include <TopTools_DataMapOfShapeShape.hxx>

#include <Geom2d_TrimmedCurve.hxx>
#include <GeomAPI_ProjectPointOnSurf.hxx>
#include <GeomAPI_ProjectPointOnCurve.hxx>

#include "geom_gluer.hxx"

// unnamed namespace for internal linkage
namespace
{
	std::ostream &
	operator<<(std::ostream &str, TopAbs_ShapeEnum type)
	{
		const char *name;
		switch (type)
		{
		case TopAbs_COMPOUND:
			name = "COMPOUND";
			break;
		case TopAbs_COMPSOLID:
			name = "COMPSOLID";
			break;
		case TopAbs_SOLID:
			name = "SOLID";
			break;
		case TopAbs_SHELL:
			name = "SHELL";
			break;
		case TopAbs_FACE:
			name = "FACE";
			break;
		case TopAbs_WIRE:
			name = "WIRE";
			break;
		case TopAbs_EDGE:
			name = "EDGE";
			break;
		case TopAbs_VERTEX:
			name = "VERTEX";
			break;
		case TopAbs_SHAPE:
			name = "SHAPE";
			break;
		default:
			return str << "[UNKNOWN " << type << ']';
		}
		return str << name;
	}

	bool isCompoundShape(const TopoDS_Shape &shape)
	{
		switch (shape.ShapeType())
		{
		case TopAbs_COMPOUND:
		case TopAbs_COMPSOLID:
			return true;
		default:
			return false;
		}
	}

	class BoundingSphere
	{
		gp_Pnt center;
		Standard_Real radius;
		Standard_Real gap;

	public:
		BoundingSphere() : center{0., 0., 0.}, radius{0.}, gap{0.} {}

		BoundingSphere(const gp_Pnt &center, Standard_Real radius, Standard_Real gap)
			: center{center}, radius{radius}, gap{gap} {}

		// Add, IsOut and SquareExtent are all required by the
		// NCollection_UBTree template

		void Add(const BoundingSphere &rhs)
		{
			gp_Pnt midpoint{0.5 * (center.XYZ() + rhs.center.XYZ())};

			radius = midpoint.Distance(center) + std::max(radius, rhs.radius + rhs.gap - gap);
			center = midpoint;
		}

		bool IsOut(const BoundingSphere &rhs) const
		{
			const Standard_Real dist = center.SquareDistance(rhs.center);
			const Standard_Real od = radius + gap + rhs.radius + rhs.gap;

			return dist > (od * od);
		}

		Standard_Real SquareExtent() const
		{
			const Standard_Real od = radius + gap;
			return 4 * od * od;
		}
	};

	using TreeFiller = NCollection_UBTreeFiller<Standard_Integer, BoundingSphere>;
	using VertexTree = TreeFiller::UBTree;

	void
	fill_tree_with_verticies(
		VertexTree &tree,
		const TopTools_IndexedMapOfShape &verticies,
		Standard_Real tolerance)
	{
		TreeFiller filler(tree);

		for (int i = 1; i <= verticies.Extent(); i++)
		{
			const TopoDS_Vertex &v = TopoDS::Vertex(verticies(i));

			BoundingSphere s{BRep_Tool::Pnt(v), BRep_Tool::Tolerance(v), tolerance};
			filler.Add(i, s);
		}

		filler.Fill();
	}

	class VertexSelector : public VertexTree::Selector
	{
		BoundingSphere sphere;
		TColStd_MapOfInteger fence;
		TColStd_ListOfInteger indicies;

	public:
		VertexSelector(const TopoDS_Vertex &vertex, Standard_Real tolerance)
			: sphere{BRep_Tool::Pnt(vertex), BRep_Tool::Tolerance(vertex), tolerance} {}

		bool Reject(const BoundingSphere &other) const override
		{
			return sphere.IsOut(other);
		}

		bool Accept(const Standard_Integer &index) override
		{
			if (fence.Add(index))
			{
				indicies.Append(index);
				return true;
			}
			return false;
		}

		const TColStd_ListOfInteger &Indices() const
		{
			return indicies;
		}
	};

	class MultiShapeKey
	{
		TopTools_MapOfShape key;
		// overflowing sum is defined for unsigned types
		unsigned hashsum;

	public:
		using KeyIterator = TopTools_MapOfShape::Iterator;

		MultiShapeKey(const TopoDS_Shape &shape) : key{1}
		{
			key.Add(shape);
			hashsum = (unsigned)shape.HashCode(INT_MAX);
		}

		MultiShapeKey(const TopTools_ListOfShape &shapes) : key(shapes.Extent()), hashsum{0}
		{
			for (const auto &shape : shapes)
			{
				key.Add(shape);
				hashsum += (unsigned)shape.HashCode(INT_MAX);
			}
		}

		struct Hasher
		{
			static Standard_Integer HashCode(
				const MultiShapeKey &key, const Standard_Integer upper)
			{
				return ::HashCode(key.hashsum, upper);
			}

			static Standard_Boolean IsEqual(
				const MultiShapeKey &lhs, const MultiShapeKey &rhs)
			{
				if (lhs.key.Extent() != rhs.key.Extent())
				{
					return false;
				}
				for (KeyIterator it{lhs.key}; it.More(); it.Next())
				{
					if (!rhs.key.Contains(it.Value()))
					{
						return false;
					}
				}
				return true;
			}
		};
	};

	template <typename T>
	using MultiShapeKeyedList = NCollection_IndexedDataMap<MultiShapeKey, T, MultiShapeKey::Hasher>;

	gp_Pnt
	PointOnShape(const TopoDS_Shape &shape)
	{
		switch (shape.ShapeType())
		{
		case TopAbs_EDGE:
		{
			Standard_Real a, b;
			const auto curve = BRep_Tool::Curve(TopoDS::Edge(shape), a, b);
			if (!curve)
			{
				throw std::runtime_error("unable to get the curve of an edge");
			}
			gp_Pnt result;
			curve->D0(IntTools_Tools::IntermediatePoint(a, b), result);
			return result;
		}
		case TopAbs_FACE:
		{
			const TopoDS_Face &face = TopoDS::Face(shape);
			Standard_Real UMin, UMax, VMin, VMax;
			BRepTools::UVBounds(face, UMin, UMax, VMin, VMax);
			const Handle(Geom_Surface) surface = BRep_Tool::Surface(face);
			if (!surface)
			{
				throw std::runtime_error("unable to get the surface of a face");
			}
			gp_Pnt result;
			surface->D0(
				IntTools_Tools::IntermediatePoint(UMin, UMax),
				IntTools_Tools::IntermediatePoint(VMin, VMax),
				result);
			return result;
		}
			// must be an EDGE or FACE at the moment
		default:
			// LOG(ERROR) << "PointOnShape passed: " << shape.ShapeType() << '\n';
			throw std::runtime_error("shape is neither an EDGE nor FACE");
		}
	}

	class shape_merger
	{
		IntTools_Context &ctx;
		Standard_Real tolerance;

		std::optional<gp_Pnt> ProjectPointOnShape(const gp_Pnt &point, const TopoDS_Shape &shape);
		TopTools_ListOfShape FindNearby(const TopoDS_Shape &shape, const TopTools_ListOfShape &others);
		TopTools_IndexedDataMapOfShapeListOfShape FindNearbyPairwise(const TopTools_ListOfShape &shapes);

	public:
		typedef MultiShapeKeyedList<TopTools_ListOfShape> ShapeKeyedShapeList;

		shape_merger(IntTools_Context &context, Standard_Real tol) : ctx{context}, tolerance{tol} {}

		void RefineCoincidentShapes(ShapeKeyedShapeList &coincident_shapes);
	};

	std::optional<gp_Pnt>
	shape_merger::ProjectPointOnShape(
		const gp_Pnt &point, const TopoDS_Shape &shape)
	{
		switch (shape.ShapeType())
		{
		case TopAbs_EDGE:
		{
			const TopoDS_Edge &edge = TopoDS::Edge(shape);
			if (!BRep_Tool::Degenerated(edge))
			{
				Standard_Real f, l;
				auto curve = BRep_Tool::Curve(edge, f, l);
				if (curve)
				{
					Standard_Real U;
					if (ctx.ProjectPointOnEdge(point, edge, U))
					{
						gp_Pnt result;
						curve->D0(U, result);
						return result;
					}
				}
			}
			return {};
		}
		case TopAbs_FACE:
		{
			GeomAPI_ProjectPointOnSurf &aProj = ctx.ProjPS(TopoDS::Face(shape));
			aProj.Perform(point);
			if (aProj.IsDone())
			{
				return aProj.NearestPoint();
			}
			return {};
		}
		default:
			throw std::runtime_error("shape must be an EDGE for FACE");
		}
	}

	TopTools_ListOfShape
	shape_merger::FindNearby(
		const TopoDS_Shape &shape, const TopTools_ListOfShape &others)
	{
		gp_Pnt p1 = PointOnShape(shape);

		TopTools_ListOfShape result;
		for (const TopoDS_Shape &other : others)
		{
			if (shape.IsSame(other))
			{
				result.Append(other);
			}
			else
			{
				if (auto p2 = ProjectPointOnShape(p1, other))
				{
					if (p1.SquareDistance(*p2) < tolerance * tolerance)
					{
						result.Append(other);
					}
				}
			}
		}
		return result;
	}

	TopTools_IndexedDataMapOfShapeListOfShape
	shape_merger::FindNearbyPairwise(const TopTools_ListOfShape &shapes)
	{
		if (shapes.IsEmpty())
		{
			throw std::runtime_error("input list is empty");
		}
		if (shapes.Extent() == 1)
		{
			// Nothing to do
			return {};
		}

		TopTools_IndexedDataMapOfShapeListOfShape result;

		TopTools_MapOfShape processed;
		while (shapes.Extent() != processed.Extent())
		{
			for (const TopoDS_Shape &shape : shapes)
			{
				if (processed.Contains(shape))
				{
					continue;
				}
				// not sure why salome ignores these
				if (shape.ShapeType() == TopAbs_EDGE &&
					BRep_Tool::Degenerated(TopoDS::Edge(shape)))
				{
					processed.Add(shape);
					continue;
				}
				// note that we expect to find ourselves
				const auto nearby = FindNearby(shape, shapes);
				if (nearby.IsEmpty())
				{
					throw std::runtime_error("geometric coincidence check failed");
				}
				result.Add(shape, nearby);
				for (const TopoDS_Shape &shape : nearby)
				{
					processed.Add(shape);
				}
			}
		}

		return result;
	}

	void shape_merger::RefineCoincidentShapes(ShapeKeyedShapeList &coincident_shapes)
	{
		TopTools_IndexedDataMapOfShapeListOfShape refined;
		for (ShapeKeyedShapeList::Iterator it{coincident_shapes}; it.More(); it.Next())
		{
			TopTools_ListOfShape &shapes = it.ChangeValue();
			//
			auto found = FindNearbyPairwise(shapes);
			decltype(found)::Iterator found_it{found};
			if (!found_it.More())
			{
				continue;
			}
			shapes.Clear();
			shapes.Append(found_it.ChangeValue());
			for (found_it.Next(); found_it.More(); found_it.Next())
			{
				refined.Add(found_it.Value().First(), found_it.Value());
			}
		}

		for (decltype(refined)::Iterator it{refined}; it.More(); it.Next())
		{
			coincident_shapes.Add({it.Key()}, it.Value());
		}
	}

	TopoDS_Shape
	MakeContainer(const TopAbs_ShapeEnum type)
	{
		BRep_Builder builder;
		switch (type)
		{
		case TopAbs_COMPSOLID:
		{
			TopoDS_CompSolid cs;
			builder.MakeCompSolid(cs);
			return cs;
		}
		case TopAbs_SOLID:
		{
			TopoDS_Solid solid;
			builder.MakeSolid(solid);
			return solid;
		}
		case TopAbs_SHELL:
		{
			TopoDS_Shell shell;
			builder.MakeShell(shell);
			return shell;
		}
		case TopAbs_WIRE:
		{
			TopoDS_Wire wire;
			builder.MakeWire(wire);
			return wire;
		}
		default:
			throw std::runtime_error("unsupported shape type");
		}
	}

	Standard_Boolean
	IsUPeriodic(const Handle(Geom_Surface) & aS)
	{
		GeomAdaptor_Surface aGAS{aS};
		GeomAbs_SurfaceType type = aGAS.GetType();
		return (
			type == GeomAbs_Cylinder ||
			type == GeomAbs_Cone ||
			type == GeomAbs_Sphere);
	}

	void
	RefinePCurveForEdgeOnFace(
		const TopoDS_Edge &edge,
		const TopoDS_Face &face,
		const Standard_Real u_min,
		const Standard_Real u_max)
	{
		Standard_Real p1, p2;
		Handle(Geom2d_Curve) curve = BRep_Tool::CurveOnSurface(edge, face, p1, p2);
		if (curve.IsNull() || BRep_Tool::IsClosed(edge, face))
		{
			return;
		}
		gp_Pnt2d pnt;
		curve->D0(IntTools_Tools::IntermediatePoint(p1, p2), pnt);
		Standard_Real x = pnt.X();
		if (x < u_min || x > u_max)
		{
			// need to rebuild
			Handle(Geom2d_Curve) curve;
			BRep_Builder{}.UpdateEdge(
				edge, curve, face, BRep_Tool::Tolerance(edge));
		}
	}

	Standard_Integer
	BuildPCurveForEdgeOnFace(
		const TopoDS_Edge &edge_old,
		const TopoDS_Edge &edge_new,
		const TopoDS_Face &face,
		const Handle(IntTools_Context) & context)
	{
		const Standard_Real tolerance = 1e-7;

		//
		if (BOPTools_AlgoTools2D::HasCurveOnSurface(edge_new, face))
		{
			return 0;
		}
		//
		// Try to copy PCurve from old edge to the new one.
		if (BOPTools_AlgoTools2D::AttachExistingPCurve(edge_old, edge_new, face, context) == 0)
		{
			// The PCurve is attached successfully.
			return 0;
		}

		//
		BOPTools_AlgoTools2D::BuildPCurveForEdgeOnFace(edge_new, face);
		Standard_Real first, last;
		Handle(Geom2d_Curve) curve = BRep_Tool::CurveOnSurface(edge_new, face, first, last);
		if (curve.IsNull())
		{
			return 1;
		}
		//
		if (!BRep_Tool::IsClosed(edge_old, face))
		{
			return 0;
		}
		//
		// 1. bUClosed - direction of closeness
		//
		Standard_Real tmp1, tmp2;
		TopoDS_Edge edge_tmp = edge_old;
		edge_tmp.Orientation(TopAbs_FORWARD);
		Handle(Geom2d_Curve) curve_fwd = BRep_Tool::CurveOnSurface(edge_tmp, face, tmp1, tmp2);
		//
		edge_tmp.Orientation(TopAbs_REVERSED);
		Handle(Geom2d_Curve) curve_rev = BRep_Tool::CurveOnSurface(edge_tmp, face, tmp1, tmp2);
		//
		gp_Pnt2d upnt_fwd, upnt_rev;
		gp_Vec2d uvec_fwd, uvec_rev;

		{
			Standard_Real u = IntTools_Tools::IntermediatePoint(first, last);
			curve_fwd->D1(u, upnt_fwd, uvec_fwd);
			curve_rev->D1(u, upnt_rev, uvec_rev);
		}

		//
		// 2. aP2D - point on curve, that corresponds to u
		Standard_Real fwd_u, fwd_v, rev_u, rev_v;
		upnt_fwd.Coord(fwd_u, fwd_v);
		upnt_rev.Coord(rev_u, rev_v);
		//
		GeomAPI_ProjectPointOnCurve &projector = context->ProjPC(edge_new);

		gp_Pnt point;
		BRep_Tool::Surface(face)->D0(fwd_u, fwd_v, point);
		projector.Perform(point);
		if (!projector.NbPoints())
		{
			return 2;
		}
		gp_Vec2d pnt_v;
		gp_Pnt2d pnt_p;
		Standard_Real edge_u, edge_v;
		curve->D1(projector.LowerDistanceParameter(), pnt_p, pnt_v);
		pnt_p.Coord(edge_u, edge_v);
		//
		// 3. Build the second 2D curve
		//
		Handle(Geom2d_TrimmedCurve) new_curve = new Geom2d_TrimmedCurve(
			Handle(Geom2d_Curve)::DownCast(curve->Copy()), first, last);
		//
		const gp_Vec2d distance{upnt_fwd, upnt_rev};
		gp_Vec2d translation = distance;
		if (fabs(gp_Dir2d{distance} * gp::DX2d()) < tolerance)
		{
			// V Closed
			if (fabs(edge_v - rev_v) < tolerance)
			{
				translation.Reverse();
			}
		}
		else
		{
			// U Closed
			if (fabs(edge_u - rev_u) < tolerance)
			{
				translation.Reverse();
			}
		}
		new_curve->Translate(translation);
		//
		// 5. Update the edge
		BRep_Builder builder;
		Standard_Real edge_tol = BRep_Tool::Tolerance(edge_new);
		if (pnt_v * uvec_fwd < 0)
		{
			builder.UpdateEdge(edge_new, new_curve, curve, face, edge_tol);
		}
		else
		{
			builder.UpdateEdge(edge_new, curve, new_curve, face, edge_tol);
		}
		return 0;
	}

	class gluedetector
	{
	public:
		gluedetector(const TopoDS_Shape &theShape, const Standard_Real aT, IntTools_Context &ctx) : myArgument{theShape},
																									myTolerance{aT},
																									merger{ctx, aT}
		{

			// perform detection
			DetectVertices();
			// LOG(TRACE) << "DetectVertices done\n";

			DetectShapes(TopAbs_EDGE);
			// LOG(TRACE) << "DetectShapes(EDGE) done\n";

			DetectShapes(TopAbs_FACE);
			// LOG(TRACE) << "DetectShapes(FACE) done\n";
		}

		const TopTools_DataMapOfShapeListOfShape &Images() { return myImages; }

	protected:
		void DetectVertices();
		void DetectShapes(const TopAbs_ShapeEnum aType);
		MultiShapeKey ShapePassKey(const TopoDS_Shape &shape);

		TopoDS_Shape myArgument;
		Standard_Real myTolerance;
		shape_merger merger;

		TopTools_DataMapOfShapeListOfShape myImages;
		TopTools_DataMapOfShapeShape myOrigins;
	};

	void
	gluedetector::DetectVertices()
	{
		TopTools_IndexedMapOfShape verticies;

		// explore *distinct* verticies
		TopExp::MapShapes(myArgument, TopAbs_VERTEX, verticies);
		if (verticies.IsEmpty())
		{
			throw std::runtime_error("no vertices in source shape");
		}

		VertexTree bounding_tree;
		fill_tree_with_verticies(bounding_tree, verticies, myTolerance);

		//
		//---------------------------------------------------
		// Chains
		TColStd_MapOfInteger processed;
		using IntMapIterator = TColStd_MapOfInteger::Iterator;

		for (int i_vertex = 1; i_vertex <= verticies.Extent(); i_vertex++)
		{
			if (processed.Contains(i_vertex))
			{
				continue;
			}

			TopTools_MapOfShape result;

			{
				TColStd_MapOfInteger processing;
				processing.Add(i_vertex);
				for (;;)
				{
					TColStd_MapOfInteger remaining;
					for (IntMapIterator it{processing}; it.More(); it.Next())
					{
						const auto &vertex = TopoDS::Vertex(verticies(it.Key()));
						if (result.Contains(vertex))
						{
							continue;
						}

						VertexSelector nearby{vertex, myTolerance};
						bounding_tree.Select(nearby);

						for (auto idx : nearby.Indices())
						{
							if (!processing.Contains(idx))
							{
								remaining.Add(idx);
							}
						}
					}
					if (remaining.IsEmpty())
					{
						break;
					}
					for (IntMapIterator it{processing}; it.More(); it.Next())
					{
						processed.Add(it.Key());
						result.Add(verticies(it.Key()));
					}
					processing.Assign(remaining);
				}
				processed.Add(i_vertex);
			}

			TopTools_MapOfShape::Iterator it{result};
			if (it.More())
			{
				TopoDS_Shape vertex{it.Key()};
				TopTools_ListOfShape related;
				for (; it.More(); it.Next())
				{
					related.Append(it.Key());
					myOrigins.Bind(it.Key(), vertex);
				}
				myImages.Bind(vertex, related);
			}
		}
	}

	MultiShapeKey
	gluedetector::ShapePassKey(const TopoDS_Shape &shape)
	{
		TopTools_ListOfShape parts;
		switch (shape.ShapeType())
		{
		case TopAbs_FACE:
			for (TopExp_Explorer it{shape, TopAbs_EDGE}; it.More(); it.Next())
			{
				const auto &edge = TopoDS::Edge(it.Current());
				if (BRep_Tool::Degenerated(edge))
				{
					continue;
				}
				if (myOrigins.IsBound(edge))
				{
					parts.Append(myOrigins.Find(edge));
				}
				else
				{
					parts.Append(edge);
				}
			}
			break;
		case TopAbs_EDGE:
			for (TopExp_Explorer it{shape, TopAbs_VERTEX}; it.More(); it.Next())
			{
				const auto &vertex = TopoDS::Vertex(it.Current());
				const auto orient = vertex.Orientation();
				if (orient == TopAbs_FORWARD || orient == TopAbs_REVERSED)
				{
					if (myOrigins.IsBound(vertex))
					{
						parts.Append(myOrigins.Find(vertex));
					}
					else
					{
						parts.Append(vertex);
					}
				}
			}
			break;
		default:
			throw std::runtime_error("shape type must be FACE or EDGE");
		}
		return {parts};
	}

	void
	gluedetector::DetectShapes(const TopAbs_ShapeEnum type)
	{
		typedef MultiShapeKeyedList<TopTools_ListOfShape> CoincidentShapeList;

		TopTools_IndexedMapOfShape aMF;
		TopTools_ListIteratorOfListOfShape aItLS;
		CoincidentShapeList coincident_shapes;
		//
		TopExp::MapShapes(myArgument, type, aMF);
		//
		for (decltype(aMF)::Iterator it{aMF}; it.More(); it.Next())
		{
			const TopoDS_Shape &shape = it.Value();

			MultiShapeKey aPKF = ShapePassKey(shape);
			//
			if (coincident_shapes.Contains(aPKF))
			{
				TopTools_ListOfShape &aLSDF = coincident_shapes.ChangeFromKey(aPKF);
				aLSDF.Append(shape);
			}
			else
			{
				TopTools_ListOfShape aLSDF;
				aLSDF.Append(shape);
				coincident_shapes.Add(aPKF, aLSDF);
			}
		}
		// LOG(TRACE) << "before RefineCoincidentShapes!\n";
		// check geometric coincidence, note this ~50% of total execution time for
		// me
		merger.RefineCoincidentShapes(coincident_shapes);
		// LOG(TRACE) << "after RefineCoincidentShapes!\n";
		//
		// Images/Origins
		for (CoincidentShapeList::Iterator it{coincident_shapes}; it.More(); it.Next())
		{
			const TopTools_ListOfShape &dups = it.Value();
			if (dups.IsEmpty())
			{
				throw std::runtime_error("DetectShapes got an empty list");
			}
			//
			if (dups.Extent() == 1)
			{
				continue;
			}
			//
			const TopoDS_Shape &shape = dups.First();
			//
			if (shape.ShapeType() == TopAbs_EDGE &&
				BRep_Tool::Degenerated(TopoDS::Edge(shape)))
			{
				continue;
			}
			//
			myImages.Bind(shape, dups);
			//
			// origins
			for (const TopoDS_Shape &dup : dups)
			{
				if (!myOrigins.IsBound(dup))
				{
					myOrigins.Bind(dup, shape);
				}
			}
		}
	}

	class geomgluer2
	{
	public:
		geomgluer2(const TopoDS_Shape &theShape) : myArgument{theShape},
												   myContext{new IntTools_Context{}}
		{
		}

		TopoDS_Shape Perform(Standard_Real tolerance);

	protected:
		TopoDS_Shape BuildResult();

		void FillVertices();
		void FillCompounds();
		void FillBRepShapes(const TopAbs_ShapeEnum theType);
		void FillContainers(const TopAbs_ShapeEnum theType);
		void FillCompound(const TopoDS_Shape &theC);

		TopoDS_Shape CopyBRepShape(const TopoDS_Shape &source);
		TopoDS_Edge CopyEdge(const TopoDS_Edge source);
		TopoDS_Face CopyFace(const TopoDS_Face source);

		bool is_bound_in_origins(const TopoDS_Shape &shape) const;
		bool is_child_bound_in_origins(const TopoDS_Shape &shape) const;

	protected:
		const TopoDS_Shape myArgument;
		const Handle(IntTools_Context) myContext;

		TopTools_DataMapOfShapeListOfShape myImagesToWork;
		TopTools_DataMapOfShapeShape myOriginsToWork;
		TopTools_DataMapOfShapeShape myOrigins;
	};

	bool
	geomgluer2::is_child_bound_in_origins(const TopoDS_Shape &shape) const
	{
		if (myOrigins.IsBound(shape))
		{
			return true;
		}
		if (isCompoundShape(shape))
		{
			for (TopoDS_Iterator it{shape}; it.More(); it.Next())
			{
				if (is_child_bound_in_origins(it.Value()))
				{
					return true;
				}
			}
		}
		return false;
	}

	bool
	geomgluer2::is_bound_in_origins(const TopoDS_Shape &shape) const
	{
		if (myOrigins.IsBound(shape))
		{
			return true;
		}
		for (TopoDS_Iterator it{shape}; it.More(); it.Next())
		{
			if (is_child_bound_in_origins(it.Value()))
			{
				return true;
			}
		}
		return false;
	}

	TopoDS_Shape
	geomgluer2::Perform(Standard_Real tolerance)
	{
		gluedetector detector{myArgument, tolerance, *myContext};

		myImagesToWork = detector.Images();
		myOriginsToWork.Clear();

		if (!myImagesToWork.Extent())
		{
			// LOG(WARNING) << "no shapes to glue detected\n";
			return myArgument;
		}

		for (decltype(myImagesToWork)::Iterator it{myImagesToWork}; it.More(); it.Next())
		{
			const TopoDS_Shape &key = it.Key();

			for (const TopoDS_Shape &shape : it.Value())
			{
				myOriginsToWork.Bind(shape, key);
			}
		}

		// LOG(TRACE) << "images and work assembled";
		FillVertices();
		// LOG(TRACE) << "FillVertices done\n";
		FillBRepShapes(TopAbs_EDGE);
		// LOG(TRACE) << "FillBRepShapes(EDGE) done\n";
		FillContainers(TopAbs_WIRE);
		// LOG(TRACE) << "FillContainers(WIRE) done\n";
		FillBRepShapes(TopAbs_FACE);
		// LOG(TRACE) << "FillBRepShapes(FACE) done\n";
		FillContainers(TopAbs_SHELL);
		// LOG(TRACE) << "FillContainers(SHELL) done\n";
		FillContainers(TopAbs_SOLID);
		// LOG(TRACE) << "FillContainers(SOLID) done\n";
		FillContainers(TopAbs_COMPSOLID);
		// LOG(TRACE) << "FillContainers(COMPSOLID) done\n";
		FillCompounds();
		// LOG(TRACE) << "FillCompounds done\n";
		auto result = BuildResult();
		// LOG(TRACE) << "BuildResult done\n";
		BRepLib::SameParameter(result, tolerance, Standard_True);
		// LOG(TRACE) << "SameParameter done\n";
		return result;
	}

	void
	geomgluer2::FillVertices()
	{
		for (decltype(myImagesToWork)::Iterator it{myImagesToWork}; it.More(); it.Next())
		{
			const TopoDS_Shape &key = it.Key();
			if (key.ShapeType() != TopAbs_VERTEX)
			{
				continue;
			}
			const TopTools_ListOfShape &verticies = it.Value();
			TopoDS_Vertex result;
			BOPTools_AlgoTools::MakeVertex(verticies, result);
			for (const auto &vertex : verticies)
			{
				myOrigins.Bind(vertex, result);
			}
		}
	}

	void
	geomgluer2::FillBRepShapes(const TopAbs_ShapeEnum theType)
	{
		TopTools_MapOfShape processed;
		//
		for (TopExp_Explorer ex{myArgument, theType}; ex.More(); ex.Next())
		{
			const TopoDS_Shape &original = ex.Current();
			//
			if (!processed.Add(original))
			{
				continue;
			}
			//
			bool bIsToWork = myOriginsToWork.IsBound(original);
			if (!is_bound_in_origins(original) && !bIsToWork)
			{
				continue;
			}
			//
			TopoDS_Shape replacement = CopyBRepShape(original);
			//
			// myImages / myOrigins
			if (bIsToWork)
			{
				const TopoDS_Shape &aSkey = myOriginsToWork.Find(original);
				//
				for (const auto &aEx : myImagesToWork.Find(aSkey))
				{
					myOrigins.Bind(aEx, replacement);
					processed.Add(aEx);
				}
			}
			else
			{
				myOrigins.Bind(original, replacement);
			}
		}
	}

	void
	geomgluer2::FillContainers(const TopAbs_ShapeEnum type)
	{
		BRep_Builder builder;

		TopTools_MapOfShape processed;

		for (TopExp_Explorer ex{myArgument, type}; ex.More(); ex.Next())
		{
			const TopoDS_Shape &original = ex.Current();
			if (!processed.Add(original))
			{
				// already processed, ignore
				continue;
			}
			if (!is_bound_in_origins(original))
			{
				continue;
			}
			//
			TopoDS_Shape replacement = MakeContainer(type);
			replacement.Orientation(original.Orientation());
			//
			for (TopoDS_Iterator it{original}; it.More(); it.Next())
			{
				const TopoDS_Shape &child = it.Value();
				if (myOrigins.IsBound(child))
				{
					TopoDS_Shape repl = myOrigins.Find(child);
					if (BOPTools_AlgoTools::IsSplitToReverse(repl, child, myContext))
					{
						repl.Reverse();
					}
					//
					builder.Add(replacement, repl);
				}
				else
				{
					builder.Add(replacement, child);
				}
			}
			myOrigins.Bind(original, replacement);
		}
	}

	void
	geomgluer2::FillCompound(const TopoDS_Shape &shape)
	{
		if (!is_bound_in_origins(shape))
		{
			return;
		}
		BRep_Builder builder;
		TopoDS_Compound compound;
		builder.MakeCompound(compound);
		for (TopoDS_Iterator it{shape}; it.More(); it.Next())
		{
			const TopoDS_Shape &child = it.Value();
			if (isCompoundShape(child))
			{
				FillCompound(child);
			}
			if (myOrigins.IsBound(child))
			{
				TopoDS_Shape repl = myOrigins.Find(child);
				repl.Orientation(child.Orientation());
				builder.Add(compound, repl);
			}
			else
			{
				builder.Add(compound, child);
			}
		}
		myOrigins.Bind(shape, compound);
	}

	void
	geomgluer2::FillCompounds()
	{
		for (TopoDS_Iterator it{myArgument}; it.More(); it.Next())
		{
			const TopoDS_Shape &shape = it.Value();
			if (isCompoundShape(shape))
			{
				FillCompound(shape);
			}
		}
	}

	TopoDS_Edge
	geomgluer2::CopyEdge(TopoDS_Edge source)
	{
		source.Orientation(TopAbs_FORWARD);

		TopoDS_Vertex v1, v2;
		TopExp::Vertices(source, v1, v2);

		double
			p1 = BRep_Tool::Parameter(v1, source),
			p2 = BRep_Tool::Parameter(v2, source);

		if (myOrigins.IsBound(v1))
		{
			v1 = TopoDS::Vertex(myOrigins.Find(v1));
		}
		if (myOrigins.IsBound(v2))
		{
			v2 = TopoDS::Vertex(myOrigins.Find(v2));
		}

		v1.Orientation(TopAbs_FORWARD);
		v2.Orientation(TopAbs_REVERSED);

		TopoDS_Edge result;
		BOPTools_AlgoTools::MakeSplitEdge(source, v1, p1, v2, p2, result);

		if (BRep_Tool::Degenerated(source))
		{
			BRep_Builder builder;
			builder.Degenerated(result, true);
			builder.UpdateEdge(result, BRep_Tool::Tolerance(source));
		}

		return result;
	}

	TopoDS_Face
	geomgluer2::CopyFace(TopoDS_Face source)
	{
		BRep_Builder builder;
		source.Orientation(TopAbs_FORWARD);

		//
		TopoDS_Face result;
		Standard_Boolean is_simple_shape;

		{
			TopLoc_Location loc;
			auto surface = BRep_Tool::Surface(source, loc);
			is_simple_shape = IsUPeriodic(surface);
			builder.MakeFace(result, surface, loc, BRep_Tool::Tolerance(source));
		}

		Standard_Real aUMin, aUMax, aVMin, aVMax;
		BRepTools::UVBounds(source, aUMin, aUMax, aVMin, aVMax);

		// iterate over wires
		for (TopoDS_Iterator it_wires{source}; it_wires.More(); it_wires.Next())
		{
			const TopoDS_Shape &orig_wire = it_wires.Value();

			if (!myOrigins.IsBound(orig_wire))
			{
				builder.Add(result, orig_wire);
				continue;
			}

			TopoDS_Shape wire = myOrigins.Find(orig_wire);
			// clear contents
			{
				TopTools_ListOfShape edges;
				for (TopoDS_Iterator it{wire}; it.More(); it.Next())
				{
					edges.Append(it.Value());
				}
				for (const TopoDS_Shape &edge : edges)
				{
					builder.Remove(wire, edge);
				}
			}

			// refill contents
			for (TopoDS_Iterator it_edges{orig_wire}; it_edges.More(); it_edges.Next())
			{
				const TopoDS_Edge &orig_edge = TopoDS::Edge(it_edges.Value());

				TopoDS_Edge edge = orig_edge;
				if (myOrigins.IsBound(orig_edge))
				{
					edge = TopoDS::Edge(myOrigins.Find(orig_edge));
				}

				if (BRep_Tool::Degenerated(edge))
				{
					edge.Orientation(orig_edge.Orientation());
				}
				else
				{
					TopoDS_Edge orig_fwd = orig_edge;
					edge.Orientation(TopAbs_FORWARD);
					orig_fwd.Orientation(TopAbs_FORWARD);
					if (is_simple_shape)
					{
						RefinePCurveForEdgeOnFace(edge, source, aUMin, aUMax);
					}
					if (BuildPCurveForEdgeOnFace(orig_fwd, edge, source, myContext))
					{
						continue;
					}
					Standard_Boolean should_reverse = BOPTools_AlgoTools::IsSplitToReverse(
						edge, orig_fwd, myContext);
					edge.Orientation(orig_edge.Orientation());
					if (should_reverse)
						edge.Reverse();
				}
				builder.Add(wire, edge);
			}
			builder.Add(result, wire);
		}
		return result;
	}

	TopoDS_Shape
	geomgluer2::CopyBRepShape(const TopoDS_Shape &source)
	{
		switch (source.ShapeType())
		{
		case TopAbs_EDGE:
			return CopyEdge(TopoDS::Edge(source));
		case TopAbs_FACE:
			return CopyFace(TopoDS::Face(source));
		default:
			throw std::runtime_error("shape must be an EDGE for FACE");
		}
	}

	TopoDS_Shape
	geomgluer2::BuildResult()
	{
		BRep_Builder builder;
		TopoDS_Compound compound;
		builder.MakeCompound(compound);
		//
		for (TopoDS_Iterator it{myArgument}; it.More(); it.Next())
		{
			const TopoDS_Shape &aCX = it.Value();
			if (myOrigins.IsBound(aCX))
			{
				builder.Add(compound, myOrigins.Find(aCX).Oriented(aCX.Orientation()));
			}
			else
			{
				builder.Add(compound, aCX);
			}
		}
		TopoDS_Compound result;
		builder.MakeCompound(result);
		for (TopExp_Explorer it{compound, TopAbs_SOLID}; it.More(); it.Next())
		{
			builder.Add(result, it.Current());
		}
		return result;
	}
}

TopoDS_Shape
salome_glue_shape(const TopoDS_Shape &shape, Standard_Real tolerance)
{
	try
	{
		geomgluer2 gluer(shape);
		return gluer.Perform(tolerance);
	}
	catch (std::exception &err)
	{
		// LOG(FATAL) << "failed to glue shapes: " << err.what() << '\n';
		std::exit(1);
	}
}
