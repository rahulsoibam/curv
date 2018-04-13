// Copyright 2016-2018 Doug Moen
// Licensed under the Apache License, version 2.0
// See accompanying file LICENSE or https://www.apache.org/licenses/LICENSE-2.0

#include <iostream>
#include <cmath>
#include <cstdlib>
#include <chrono>
#include <openvdb/openvdb.h>
#include <openvdb/tools/VolumeToMesh.h>

#include "export.h"
#include <curv/shape.h>
#include <curv/exception.h>
#include <curv/die.h>

using openvdb::Vec3s;
using openvdb::Vec3d;
using openvdb::Vec3i;

enum Mesh_Format {
    stl_format,
    obj_format
};

void export_mesh(Mesh_Format, curv::Value value,
    curv::System& sys, const curv::Context& cx, const Export_Params& params,
    std::ostream& out);

void export_stl(curv::Value value,
    curv::System& sys, const curv::Context& cx, const Export_Params& params,
    std::ostream& out)
{
    export_mesh(stl_format, value, sys, cx, params, out);
}

void export_obj(curv::Value value,
    curv::System& sys, const curv::Context& cx, const Export_Params& params,
    std::ostream& out)
{
    export_mesh(obj_format, value, sys, cx, params, out);
}

void put_triangle(std::ostream& out, Vec3s v0, Vec3s v1, Vec3s v2)
{
    out << "facet normal 0 0 0\n"
        << " outer loop\n"
        << "  vertex " << v0.x() << " " << v0.y() << " " << v0.z() << "\n"
        << "  vertex " << v1.x() << " " << v1.y() << " " << v1.z() << "\n"
        << "  vertex " << v2.x() << " " << v2.y() << " " << v2.z() << "\n"
        << " endloop\n"
        << "endfacet\n";
}

double param_to_double(Export_Params::const_iterator i)
{
    char *endptr;
    double result = strtod(i->second.c_str(), &endptr);
    if (endptr == i->second.c_str() || result != result) {
        // error
        std::cerr << "invalid number in: -O "<< i->first.c_str() << "='"
            << i->second.c_str() << "'\n";
        exit(EXIT_FAILURE);
    }
    return result;
}

void export_mesh(Mesh_Format format, curv::Value value,
    curv::System& sys, const curv::Context& cx, const Export_Params& params,
    std::ostream& out)
{
    curv::Shape_Recognizer shape(cx, sys);
    if (!shape.recognize(value) && !shape.is_3d_)
        throw curv::Exception(cx, "mesh export: not a 3D shape");

#if 0
    for (auto p : params) {
        std::cerr << p.first << "=" << p.second << "\n";
    }
#endif

    Vec3d size(
        shape.bbox_.xmax - shape.bbox_.xmin,
        shape.bbox_.ymax - shape.bbox_.ymin,
        shape.bbox_.zmax - shape.bbox_.zmin);
    double volume = size.x() * size.y() * size.z();
    double infinity = 1.0/0.0;
    if (volume == infinity || volume == -infinity) {
        throw curv::Exception(cx, "mesh export: shape is infinite");
    }

    double voxelsize;
    auto res_p = params.find("res");
    if (res_p != params.end()) {
        double res = param_to_double(res_p);
        if (res <= 0.0) {
            throw curv::Exception(cx, curv::stringify(
                "mesh export: invalid parameter res=",res_p->second));
        }
        voxelsize = res;
    } else {
        voxelsize = cbrt(volume / 100'000);
        if (voxelsize < 0.1) voxelsize = 0.1;
    }

    // This is the range of voxel coordinates.
    // For meshing to work, we need to specify at least a thin band of voxels
    // surrounding the sphere boundary, both inside and outside. To provide a
    // margin for error, I'll say that we need to populate voxels 2 units away
    // from the surface.
    Vec3i voxelrange_min(
        int(floor(shape.bbox_.xmin/voxelsize)) - 2,
        int(floor(shape.bbox_.ymin/voxelsize)) - 2,
        int(floor(shape.bbox_.zmin/voxelsize)) - 2);
    Vec3i voxelrange_max(
        int(ceil(shape.bbox_.xmax/voxelsize)) + 2,
        int(ceil(shape.bbox_.ymax/voxelsize)) + 2,
        int(ceil(shape.bbox_.zmax/voxelsize)) + 2);

    std::cerr
        << "resolution="<<voxelsize<<": "
        << (voxelrange_max.x() - voxelrange_min.x() + 1) << "×"
        << (voxelrange_max.y() - voxelrange_min.y() + 1) << "×"
        << (voxelrange_max.z() - voxelrange_min.z() + 1)
        << " voxels. Use '-O res=N' to change resolution.\n";
    std::cerr.flush();

    openvdb::initialize();

    // Create a FloatGrid and populate it with a signed distance field.
    std::chrono::time_point<std::chrono::steady_clock> start_time, end_time;
    start_time = std::chrono::steady_clock::now();

    // 2.0 is the background (or default) distance value for this
    // sparse array of voxels. Each voxel is a `float`.
    openvdb::FloatGrid::Ptr grid = openvdb::FloatGrid::create(2.0);

    // Attach a scaling transform that sets the voxel size in world space.
    grid->setTransform(
        openvdb::math::Transform::createLinearTransform(voxelsize));

    // Identify the grid as a signed distance field.
    grid->setGridClass(openvdb::GRID_LEVEL_SET);

    // Populate the grid.
    // I assume each distance value is in the centre of a voxel.
    auto accessor = grid->getAccessor();
    for (int x = voxelrange_min.x(); x <= voxelrange_max.x(); ++x) {
        for (int y = voxelrange_min.y(); y <= voxelrange_max.y(); ++y) {
            for (int z = voxelrange_min.z(); z <= voxelrange_max.z(); ++z) {
                accessor.setValue(openvdb::Coord{x,y,z},
                    shape.dist(x*voxelsize, y*voxelsize, z*voxelsize, 0.0));
            }
        }
    }
    end_time = std::chrono::steady_clock::now();
    std::chrono::duration<double> render_time = end_time - start_time;
    int nvoxels =
        (voxelrange_max.x() - voxelrange_min.x() + 1) *
        (voxelrange_max.y() - voxelrange_min.y() + 1) *
        (voxelrange_max.z() - voxelrange_min.z() + 1);
    std::cerr
        << "Rendered " << nvoxels
        << " voxels in " << render_time.count() << "s ("
        << int(nvoxels/render_time.count()) << " voxels/s).\n";
    std::cerr.flush();

    // convert grid to a mesh
    double adaptivity = 0.0;
    auto adaptive_p = params.find("adaptive");
    if (adaptive_p != params.end()) {
        if (adaptive_p->second.empty())
            adaptivity = 1.0;
        else {
            adaptivity = param_to_double(adaptive_p);
            if (adaptivity < 0.0 || adaptivity > 1.0) {
                throw curv::Exception(cx,
                    "mesh export: parameter 'adaptive' must be in range 0...1");
            }
        }
    }
    openvdb::tools::VolumeToMesh mesher(0.0, adaptivity);
    mesher(*grid);

    // output a mesh file
    int ntri = 0;
    int nquad = 0;
    switch (format) {
    case stl_format:
        out << "solid curv\n";
        for (int i=0; i<mesher.polygonPoolListSize(); ++i) {
            openvdb::tools::PolygonPool& pool = mesher.polygonPoolList()[i];
            for (int j=0; j<pool.numTriangles(); ++j) {
                // swap ordering of nodes to get outside-normals
                put_triangle(out,
                    mesher.pointList()[ pool.triangle(j)[0] ],
                    mesher.pointList()[ pool.triangle(j)[2] ],
                    mesher.pointList()[ pool.triangle(j)[1] ]);
                ++ntri;
            }
            for (int j=0; j<pool.numQuads(); ++j) {
                // swap ordering of nodes to get outside-normals
                put_triangle(out,
                    mesher.pointList()[ pool.quad(j)[0] ],
                    mesher.pointList()[ pool.quad(j)[2] ],
                    mesher.pointList()[ pool.quad(j)[1] ]);
                put_triangle(out,
                    mesher.pointList()[ pool.quad(j)[0] ],
                    mesher.pointList()[ pool.quad(j)[3] ],
                    mesher.pointList()[ pool.quad(j)[2] ]);
                ntri += 2;
            }
        }
        out << "endsolid curv\n";
        break;
    case obj_format:
        for (int i = 0; i < mesher.pointListSize(); ++i) {
            auto& pt = mesher.pointList()[i];
            out << "v " << pt.x() << " " << pt.y() << " " << pt.z() << "\n";
        }
        for (int i=0; i<mesher.polygonPoolListSize(); ++i) {
            openvdb::tools::PolygonPool& pool = mesher.polygonPoolList()[i];
            for (int j=0; j<pool.numTriangles(); ++j) {
                // swap ordering of nodes to get outside-normals
                auto& tri = pool.triangle(j);
                out << "f " << tri[0]+1 << " "
                            << tri[2]+1 << " "
                            << tri[1]+1 << "\n";
                ++ntri;
            }
            for (int j=0; j<pool.numQuads(); ++j) {
                // swap ordering of nodes to get outside-normals
                auto& q = pool.quad(j);
                out << "f " << q[0]+1 << " "
                            << q[3]+1 << " "
                            << q[2]+1 << " "
                            << q[1]+1 << "\n";
                ++nquad;
            }
        }
        break;
    default:
        curv::die("bad mesh format");
    }

    if (ntri == 0 && nquad == 0) {
        std::cerr << "WARNING: no mesh was created (no volumes were found).\n"
          << "Maybe you should try a smaller resolution.\n";
    } else {
        if (ntri > 0)
            std::cerr << ntri << " triangles";
        if (ntri > 0 && nquad > 0)
            std::cerr << ", ";
        if (nquad > 0)
            std::cerr << nquad << " quads";
        std::cerr << ".";
        if (adaptivity < 1.0)
            std::cerr << " Use '-O adaptive' to reduce triangle count.";
        std::cerr << "\n";
    }
}