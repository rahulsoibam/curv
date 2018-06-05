
// Copyright 2016-2018 Doug Moen
// Licensed under the Apache License, version 2.0
// See accompanying file LICENSE or https://www.apache.org/licenses/LICENSE-2.0

#ifndef COMPILED_SHAPE_H
#define COMPILED_SHAPE_H

#include <libcurv/shape.h>
#include <ostream>
#include <glm/vec3.hpp>

struct Compiled_Shape final : public curv::Shape
{
    double (*dist_)(double,double,double,double);
    void (*colour_)(double,double,double,double,glm::vec3*);

    Compiled_Shape(curv::Shape_Recognizer&);

    virtual double dist(double x, double y, double z, double t) override
    {
        return dist_(x,y,z,t);
    }
    virtual curv::Vec3 colour(double x, double y, double z, double t) override
    {
        glm::vec3 c;
        colour_(x,y,z,t, &c);
        return curv::Vec3{c.x,c.y,c.z};
    }
};

void shape_to_cpp(curv::Shape_Recognizer& shape, std::ostream& out);

#endif // include guard