// Copyright 2016-2019 Doug Moen
// Licensed under the Apache License, version 2.0
// See accompanying file LICENSE or https://www.apache.org/licenses/LICENSE-2.0

#ifndef LIBCURV_REACTIVE_H
#define LIBCURV_REACTIVE_H

#include <libcurv/gl_type.h>
#include <libcurv/function.h> // prereq of meaning.h
#include <libcurv/meaning.h>

namespace curv {

// A reactive value changes over time. Same idea as a Behaviour in functional
// reactive programming. An abstract class.
struct Reactive_Value : public Ref_Value
{
    GL_Type gltype_;

    Reactive_Value(
        int subty, GL_Type gltype)
    :
        Ref_Value(ty_reactive, subty),
        gltype_(gltype)
    {}

    virtual void print(std::ostream&) const override;
    virtual Shared<Operation> expr(const Phrase&);
};

// An expression over one or more reactive variables. Essentially, this is a
// lazy evaluation thunk. Reactive expressions can only be evaluated in a
// context where the values of their reactive variables are known, eg on the
// GPU while displaying an animated shape. In other contexts, evaluation is
// deferred.
struct Reactive_Expression : public Reactive_Value
{
    Shared<Operation> expr_;

    Reactive_Expression(GL_Type, Shared<Operation> expr, const Context&);

    virtual Shared<Operation> expr(const Phrase&) override;

    size_t hash() const noexcept
    {
        return expr_->hash();
    }
    bool hash_eq(const Reactive_Expression& re) const noexcept
    {
        return expr_->hash_eq(*re.expr_);
    }
};

} // namespace curv
#endif // header guard
