#pragma once

// The sketch's constraints and dimensions solved together (docs/spec/features/
// cad_design.md, rule 5): the geometry moved as little as it can to meet them
// all, the degrees of freedom left, what is fully fixed, and a constraint that
// is redundant or contradicts the others refused. Own design (see
// PROVENANCE.md): each constraint is one or more equations in the sketch's
// coordinates, solved by minimum-norm Gauss-Newton steps; the degrees of
// freedom are the coordinates less the rank of the equations' Jacobian.

#include <cstddef>
#include <string>
#include <vector>

#include "cad/sketch.hpp"

namespace tisma::cad {

struct SketchSolve {
    bool solved = true;     // every equation met within 1e-6 mm
    double residual = 0.0;  // the largest equation's error left
    int unknowns = 0;       // the sketch's coordinates
    int dof = 0;            // the degrees of freedom left (unknowns less the rank)
    // Per entity: every coordinate of it fixed by the constraints (drawn in
    // black, or white on the dark theme).
    std::vector<bool> entity_fixed;
};

// The degrees of freedom at the sketch as it is, without moving anything.
SketchSolve analyze_sketch(const Sketch& sketch);

// Moves the sketch's geometry to meet its constraints and dimensions, as
// little as it can, then counts what is left. When they cannot be met, or
// meeting them would turn a rectangle inside out (its width or height
// through zero) or take a radius to zero, the sketch stays as it was and the
// answer says it is not solved.
SketchSolve solve_sketch(Sketch& sketch);

// Adds a constraint or dimension by hand (rule 5): the sketch solved with it
// when it can be met and it fixes something new; otherwise the sketch stays
// as it was and the answer says why.
enum class AddRule { Added, Redundant, Conflicting, Invalid };
AddRule add_rule(Sketch& sketch, const GeoConstraint& rule);

// A rectangle's width or height, or a circle's diameter, set to `value`
// ("width", "height", "diameter"): the dimensions on it follow, and the
// sketch is solved so its constraints still hold. A rectangle's first
// corner stays where it can. False when the entity has no such size.
bool set_entity_size(Sketch& sketch, std::size_t entity, const std::string& what, double value);

// How many equations a rule makes (Fixed, Coincident and Concentric two; the
// rest one).
int equations_of(GeoConstraint::Kind kind);

} // namespace tisma::cad
