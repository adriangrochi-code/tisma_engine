#pragma once

#include "config/settings.hpp"

// The printer the tests print on. The engine has no speeds of its own: they
// come from the printer's profile (owner's decision, 2026-10-26), and these
// are the test printer's, the values the tests were measured with. Only the
// speeds not set are filled.
inline tisma::Settings test_printer(tisma::Settings s = {}) {
    auto fill = [](double& v, double value) {
        if (!(v > 0.0))
            v = value;
    };
    fill(s.max_feed_x, 200.0);
    fill(s.max_feed_y, 200.0);
    fill(s.max_feed_z, 12.0);
    fill(s.max_feed_e, 120.0);
    fill(s.corner_speed, 10.0);
    fill(s.wall_speed, 60.0);
    if (!(s.outer_wall_speed.value > 0.0))
        s.outer_wall_speed = {50.0, true};
    fill(s.infill_speed, 80.0);
    fill(s.solid_infill_speed, 80.0);
    fill(s.top_solid_infill_speed, 15.0);
    fill(s.support_speed, 80.0);
    fill(s.travel_speed, 130.0);
    fill(s.first_layer_speed, 30.0);
    fill(s.max_speed, 80.0);
    fill(s.retract_speed, 40.0);
    fill(s.min_print_speed, 10.0);
    // Its beads 0.4 wide, as the tests were measured: walls.width is
    // automatic (1.125 nozzles) only when nothing sets it (BH4).
    if (s.wall_width_auto) {
        s.wall_width = 0.4;
        s.wall_width_auto = false;
    }
    return s;
}
