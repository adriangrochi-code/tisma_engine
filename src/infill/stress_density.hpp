#pragma once

#include "config/settings.hpp"
#include "fea/voxels.hpp"
#include "geom/polygons.hpp"
#include "mesh/indexed_mesh.hpp"

#include <vector>

namespace tisma::gpu {
class Context;
}

// Infill denser where the part works hardest (docs/INFILL.md, stage 4): the
// engine's own structural analysis of the part under the user's load case
// says how loaded each voxel is, and each layer's sparse infill goes denser
// in bands where it is. Own design, after the idea of stress-guided infill
// in phase A2 of PLAN.md; see PROVENANCE.md.
namespace tisma {

struct AnalysisReport;

// How loaded each voxel is: its stress over its strength (1 / safety factor),
// over the most loaded voxel's, from 0 to 1; 0 where it is empty.
struct StressField {
    VoxelGrid grid;
    std::vector<float> load;
    bool empty() const { return load.empty(); }
};

StressField stress_field(const AnalysisReport& report);

struct AnalysisCase;
// The field for stress-guided infill: the solid part (its infill does not
// change where the load goes) under the case, at its resolution, once.
StressField infill_stress_field(const IndexedMesh& mesh, const Settings& settings, const AnalysisCase& c,
                                gpu::Context* gpu = nullptr);

// Where, in the plane at height z, the load is at least `level`: the voxels'
// squares, joined and closed over by half a voxel so diagonal neighbours
// join. Empty above or below the field.
Polygons loaded_region(const StressField& field, double z, double level);

// The density bands of stress-guided infill: from the plain density up to
// `most` (both 0 to 1) over the loads 0.15, 0.35 and 0.6.
struct DensityBand {
    double level = 0.0;   // the load from which it applies
    double density = 0.0; // 0 to 1
};
std::vector<DensityBand> density_bands(double plain, double most);

} // namespace tisma
