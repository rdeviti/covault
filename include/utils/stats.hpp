#pragma once

#define duration(a) std::chrono::duration<double>(a).count()
#define time_now() std::chrono::high_resolution_clock::now()
#define seconds_now()                                                 \
    std::chrono::duration_cast<std::chrono::seconds>(                 \
        std::chrono::high_resolution_clock::now().time_since_epoch()) \
        .count()

struct aggr_stats {
    std::vector<double> tpc;
    std::vector<double> mr;
};

// compute mean and standard deviation of a std::vector
std::tuple<double, double> compute_mean_stdev(std::vector<double> v) {
    double sum = std::accumulate(v.begin(), v.end(), 0.0);
    double mean = sum / v.size();
    std::vector<double> diff(v.size());
    std::transform(v.begin(), v.end(), diff.begin(),
                   [mean](double x) { return x - mean; });
    double sq_sum =
        std::inner_product(diff.begin(), diff.end(), diff.begin(), 0.0);
    double stdev = std::sqrt(sq_sum / v.size());
    return std::make_tuple(mean, stdev);
}