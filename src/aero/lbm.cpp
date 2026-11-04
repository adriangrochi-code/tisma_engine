#include "aero/lbm.hpp"

#include <oneapi/tbb/blocked_range.h>
#include <oneapi/tbb/parallel_for.h>

#include <algorithm>
#include <atomic>
#include <cmath>

namespace tisma::aero {

namespace {

// D3Q19: the rest velocity, the 6 faces, the 12 edges.
constexpr int kQ = 19;
constexpr int kE[kQ][3] = {{0, 0, 0},  {1, 0, 0},   {-1, 0, 0}, {0, 1, 0},  {0, -1, 0}, {0, 0, 1},  {0, 0, -1},
                           {1, 1, 0},  {-1, -1, 0}, {1, -1, 0}, {-1, 1, 0}, {1, 0, 1},  {-1, 0, -1}, {1, 0, -1},
                           {-1, 0, 1}, {0, 1, 1},   {0, -1, -1}, {0, 1, -1}, {0, -1, 1}};
constexpr int kOpposite[kQ] = {0, 2, 1, 4, 3, 6, 5, 8, 7, 10, 9, 12, 11, 14, 13, 16, 15, 18, 17};
constexpr double kW[kQ] = {1.0 / 3,  1.0 / 18, 1.0 / 18, 1.0 / 18, 1.0 / 18, 1.0 / 18, 1.0 / 18,
                           1.0 / 36, 1.0 / 36, 1.0 / 36, 1.0 / 36, 1.0 / 36, 1.0 / 36, 1.0 / 36,
                           1.0 / 36, 1.0 / 36, 1.0 / 36, 1.0 / 36, 1.0 / 36};

inline double equilibrium(int i, double rho, double ux, double uy, double uz) {
    const double eu = kE[i][0] * ux + kE[i][1] * uy + kE[i][2] * uz;
    const double uu = ux * ux + uy * uy + uz * uz;
    return kW[i] * rho * (1.0 + 3.0 * eu + 4.5 * eu * eu - 1.5 * uu);
}

} // namespace

LbmResult run_lbm(const LbmOptions& o, const std::vector<std::uint8_t>& solid, const std::function<bool(double)>& progress) {
    LbmResult result;
    const int nx = o.nx, ny = o.ny, nz = o.nz;
    const std::size_t cells = std::size_t(nx) * std::size_t(ny) * std::size_t(nz);
    if (nx < 3 || ny < 1 || nz < 1 || solid.size() != cells) {
        result.stable = false;
        return result;
    }
    const auto index = [&](int x, int y, int z) { return (std::size_t(z) * std::size_t(ny) + std::size_t(y)) * std::size_t(nx) + std::size_t(x); };
    std::vector<float> a(cells * kQ), b(cells * kQ);
    const double u0 = o.velocity;
    float inlet[kQ];
    for (int i = 0; i < kQ; ++i)
        inlet[i] = float(equilibrium(i, 1.0, u0, 0.0, 0.0));
    const bool warm = o.initial_density.size() == cells && o.initial_velocity.size() == 3 * cells;
    for (std::size_t c = 0; c < cells; ++c)
        for (int i = 0; i < kQ; ++i)
            a[std::size_t(i) * cells + c] =
                solid[c] ? 0.0f
                : warm   ? float(equilibrium(i, o.initial_density[c], o.initial_velocity[3 * c], o.initial_velocity[3 * c + 1],
                                             o.initial_velocity[3 * c + 2]))
                         : inlet[i];
    std::vector<double> sum(cells, 0.0); // double: the pressure is a small part of the density
    std::vector<float> velocity_sum(3 * cells, 0.0f);
    const double tau0 = 3.0 * o.viscosity + 0.5;
    constexpr double kMagic = 3.0 / 16.0; // TRT's Λ
    // The absorbing layer: the relaxation time rises as the square of the
    // depth into it, by 2 at the outlet.
    std::vector<double> sponge(std::size_t(nx), 0.0);
    for (int x = std::max(1, nx - o.sponge); x < nx && o.sponge > 0; ++x) {
        const double s = double(x - (nx - o.sponge) + 1) / double(o.sponge);
        sponge[std::size_t(x)] = 2.0 * s * s;
    }
    // The force on the solid, per plane of z (each plane is one task's):
    // all of it and the pressure part, summed over the averaged steps.
    std::vector<double> plane_force(std::size_t(nz) * 6, 0.0);
    const double smag = 18.0 * std::sqrt(2.0) * o.smagorinsky * o.smagorinsky;
    std::atomic<bool> finite{true};
    // Per cell: which neighbours are solid (bit i for direction i), and
    // whether it is on the domain's edge (the inlet, the outlet or a
    // periodic side), where the slow general path runs.
    std::vector<std::uint32_t> walls(cells, 0);
    std::vector<std::uint8_t> edge(cells, 0);
    std::ptrdiff_t offset[kQ];
    for (int i = 0; i < kQ; ++i)
        offset[i] = std::ptrdiff_t(kE[i][0]) + std::ptrdiff_t(kE[i][1]) * nx + std::ptrdiff_t(kE[i][2]) * std::ptrdiff_t(nx) * ny;
    tbb::parallel_for(tbb::blocked_range<int>(0, nz), [&](const tbb::blocked_range<int>& range) {
        for (int z = range.begin(); z < range.end(); ++z)
            for (int y = 0; y < ny; ++y)
                for (int x = 0; x < nx; ++x) {
                    const std::size_t c = index(x, y, z);
                    edge[c] = x == 0 || x == nx - 1 || y == 0 || y == ny - 1 || z == 0 || z == nz - 1;
                    std::uint32_t m = 0;
                    for (int i = 1; i < kQ; ++i) {
                        const int tx = x + kE[i][0];
                        if (tx < 0 || tx >= nx)
                            continue;
                        int ty = y + kE[i][1], tz = z + kE[i][2];
                        ty = ty < 0 ? ny - 1 : ty >= ny ? 0 : ty;
                        tz = tz < 0 ? nz - 1 : tz >= nz ? 0 : tz;
                        if (solid[index(tx, ty, tz)])
                            m |= 1u << i;
                    }
                    walls[c] = m;
                }
    });

    // Early stop (docs/research/cfd_fea_optimizations.md §2.5): the drag
    // along x is followed in windows of a quarter of a flow-through; after
    // 1.5 flow-throughs, once three windows in a row agree within
    // `converge`, two more windows are averaged and the run ends.
    long steps = o.steps, average_from = o.average_from;
    const long through = std::max(1L, long(std::ceil(double(nx) / std::max(1e-6, o.velocity))));
    const long window = std::max(1L, through / 4);
    std::vector<double> plane_drag(std::size_t(nz), 0.0);
    std::vector<double> windows;
    double window_sum = 0.0;
    bool stopping = false;
    for (long step = 0; step < steps; ++step) {
        const bool averaging = step >= average_from;
        const std::vector<float>& src = a;
        std::vector<float>& dst = b;
        tbb::parallel_for(tbb::blocked_range<int>(0, nz), [&](const tbb::blocked_range<int>& range) {
            for (int z = range.begin(); z < range.end(); ++z)
                for (int y = 0; y < ny; ++y)
                    for (int x = 0; x < nx; ++x) {
                        const std::size_t c = index(x, y, z);
                        if (solid[c])
                            continue;
                        double f[kQ];
                        double rho = 0.0, jx = 0.0, jy = 0.0, jz = 0.0;
                        for (int i = 0; i < kQ; ++i) {
                            f[i] = src[std::size_t(i) * cells + c];
                            rho += f[i];
                            jx += f[i] * kE[i][0], jy += f[i] * kE[i][1], jz += f[i] * kE[i][2];
                        }
                        if (averaging) {
                            sum[c] += rho;
                            if (rho > 1e-6) {
                                velocity_sum[3 * c] += float(jx / rho);
                                velocity_sum[3 * c + 1] += float(jy / rho);
                                velocity_sum[3 * c + 2] += float(jz / rho);
                            }
                        }
                        double post[kQ];
                        if (x == 0) {
                            // The inlet: the free stream, at rest density.
                            for (int i = 0; i < kQ; ++i)
                                post[i] = inlet[i];
                        } else {
                            if (!(rho > 1e-6) || !std::isfinite(rho)) {
                                finite = false;
                                rho = 1.0;
                            }
                            const double ux = jx / rho, uy = jy / rho, uz = jz / rho;
                            double feq[kQ];
                            double pxx = 0, pyy = 0, pzz = 0, pxy = 0, pxz = 0, pyz = 0;
                            const double uu15 = 1.5 * (ux * ux + uy * uy + uz * uz);
                            for (int i = 0; i < kQ; ++i) {
                                const double eu = kE[i][0] * ux + kE[i][1] * uy + kE[i][2] * uz;
                                feq[i] = kW[i] * rho * (1.0 + 3.0 * eu + 4.5 * eu * eu - uu15);
                                const double n = f[i] - feq[i];
                                const double ex = kE[i][0], ey = kE[i][1], ez = kE[i][2];
                                pxx += ex * ex * n, pyy += ey * ey * n, pzz += ez * ez * n;
                                pxy += ex * ey * n, pxz += ex * ez * n, pyz += ey * ez * n;
                            }
                            // The subgrid eddies' share of the relaxation time.
                            const double q = std::sqrt(pxx * pxx + pyy * pyy + pzz * pzz + 2.0 * (pxy * pxy + pxz * pxz + pyz * pyz));
                            const double tau = 0.5 * (tau0 + std::sqrt(tau0 * tau0 + smag * q / rho)) + sponge[std::size_t(x)];
                            const double omega = 1.0 / tau;
                            if (!o.trt) {
                                for (int i = 0; i < kQ; ++i)
                                    post[i] = f[i] - omega * (f[i] - feq[i]);
                            } else {
                                // Each direction with its opposite: the even part
                                // relaxes at omega, the odd at the rate Λ gives.
                                const double minus = 1.0 / (0.5 + kMagic / (tau - 0.5));
                                post[0] = f[0] - omega * (f[0] - feq[0]);
                                for (int i = 1; i < kQ; i += 2) {
                                    const int j = i + 1;
                                    const double even = 0.5 * (f[i] + f[j] - feq[i] - feq[j]);
                                    const double odd = 0.5 * (f[i] - f[j] - feq[i] + feq[j]);
                                    post[i] = f[i] - omega * even - minus * odd;
                                    post[j] = f[j] - omega * even + minus * odd;
                                }
                            }
                        }
                        // The momentum the links into the solid hand over:
                        // 2 e_i f_i each; at rest its equilibrium part,
                        // 2 e_i w_i rho, is the pressure's.
                        const auto exchange = [&](int i) {
                            plane_drag[std::size_t(z)] += kE[i][0] * 2.0 * kW[i] * (rho - 1.0);
                            if (!averaging)
                                return;
                            double* fz = &plane_force[std::size_t(z) * 6];
                            const double total = 2.0 * post[i], pressure = 2.0 * kW[i] * (rho - 1.0);
                            for (int k = 0; k < 3; ++k) {
                                fz[k] += kE[i][k] * total;
                                fz[3 + k] += kE[i][k] * pressure;
                            }
                        };
                        // Pushed to the neighbours; into a solid cell it
                        // bounces back here, halfway. Inside the domain the
                        // neighbours are fixed offsets away.
                        if (!edge[c]) {
                            const std::uint32_t m = walls[c];
                            for (int i = 0; i < kQ; ++i) {
                                if (m & (1u << i)) {
                                    dst[std::size_t(kOpposite[i]) * cells + c] = float(post[i]);
                                    exchange(i);
                                } else
                                    dst[std::size_t(i) * cells + std::size_t(std::ptrdiff_t(c) + offset[i])] = float(post[i]);
                            }
                            continue;
                        }
                        for (int i = 0; i < kQ; ++i) {
                            const int tx = x + kE[i][0];
                            if (tx < 0 || tx >= nx)
                                continue;
                            int ty = y + kE[i][1], tz = z + kE[i][2];
                            ty = ty < 0 ? ny - 1 : ty >= ny ? 0 : ty;
                            tz = tz < 0 ? nz - 1 : tz >= nz ? 0 : tz;
                            const std::size_t t = index(tx, ty, tz);
                            if (solid[t]) {
                                dst[std::size_t(kOpposite[i]) * cells + c] = float(post[i]);
                                exchange(i);
                            } else
                                dst[std::size_t(i) * cells + t] = float(post[i]);
                        }
                    }
        });
        // The outlet: nothing comes in from beyond it; zero gradient.
        tbb::parallel_for(tbb::blocked_range<int>(0, nz), [&](const tbb::blocked_range<int>& range) {
            for (int z = range.begin(); z < range.end(); ++z)
                for (int y = 0; y < ny; ++y) {
                    const std::size_t to = index(nx - 1, y, z), from = index(nx - 2, y, z);
                    if (solid[to] || solid[from])
                        continue;
                    for (int i = 0; i < kQ; ++i)
                        dst[std::size_t(i) * cells + to] = dst[std::size_t(i) * cells + from];
                }
        });
        std::swap(a, b);
        if (averaging)
            ++result.averaged_steps;
        if (o.converge > 0.0 && !stopping) {
            double drag = 0.0;
            for (double& d : plane_drag)
                drag += d, d = 0.0;
            window_sum += drag;
            if ((step + 1) % window == 0) {
                windows.push_back(window_sum / double(window));
                window_sum = 0.0;
                const std::size_t n = windows.size();
                const auto close = [&](double x, double y) { return std::abs(x - y) <= o.converge * std::max(std::abs(y), 1e-12); };
                if (double(step + 1) >= o.min_flow_throughs * double(through) && n >= 4 && close(windows[n - 2], windows[n - 1]) &&
                    close(windows[n - 3], windows[n - 2]) && close(windows[n - 4], windows[n - 3]) &&
                    step + 1 + 2 * window < steps) {
                    stopping = true;
                    // Keep what was averaged if the averaging had begun.
                    if (step + 1 < average_from)
                        average_from = step + 1;
                    steps = step + 1 + 2 * window;
                    result.stopped_early = true;
                }
            }
        } else {
            std::fill(plane_drag.begin(), plane_drag.end(), 0.0);
        }
        if (!finite) {
            result.stable = false;
            break;
        }
        if (progress && step % 25 == 0 && !progress(double(step) / double(steps))) {
            result.stable = false;
            break;
        }
    }
    result.steps = steps;
    result.mean_density.assign(cells, 1.0f);
    result.mean_velocity.assign(3 * cells, 0.0f);
    if (result.averaged_steps > 0)
        for (std::size_t c = 0; c < cells; ++c) {
            result.mean_density[c] = solid[c] ? 1.0f : float(sum[c] / double(result.averaged_steps));
            for (std::size_t k = 0; k < 3; ++k)
                result.mean_velocity[3 * c + k] = solid[c] ? 0.0f : velocity_sum[3 * c + k] / float(result.averaged_steps);
        }
    // The free stream's density: the plane just behind the inlet.
    double inlet_sum = 0.0;
    long inlet_cells = 0;
    const int x_ref = std::min(2, nx - 1);
    for (int z = 0; z < nz; ++z)
        for (int y = 0; y < ny; ++y)
            if (const std::size_t c = index(x_ref, y, z); !solid[c])
                inlet_sum += result.mean_density[c], ++inlet_cells;
    result.inlet_density = inlet_cells > 0 ? inlet_sum / double(inlet_cells) : 1.0;
    if (result.averaged_steps > 0) {
        double total[6] = {0, 0, 0, 0, 0, 0};
        for (int z = 0; z < nz; ++z)
            for (int k = 0; k < 6; ++k)
                total[k] += plane_force[std::size_t(z) * 6 + std::size_t(k)];
        // The pressure part was taken against rho = 1; a closed solid's links
        // sum w_i e_i to zero, so moving the reference changes nothing but
        // the round-off.
        for (int k = 0; k < 3; ++k) {
            result.force[k] = total[k] / double(result.averaged_steps);
            result.pressure_force[k] = total[3 + k] / double(result.averaged_steps);
        }
    }
    return result;
}

} // namespace tisma::aero
