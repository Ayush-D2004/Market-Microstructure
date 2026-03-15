#pragma once

#include <queue>
#include <set>
#include <cmath>
#include <vector>

namespace lob {

class RollingMedian {
public:
    explicit RollingMedian(size_t window_size) : window_size_(window_size) {}

    void add(double value) {
        window_.push(value);

        // Add to heaps
        if (lowers_.empty() || value <= *lowers_.rbegin()) {
            lowers_.insert(value);
        } else {
            uppers_.insert(value);
        }

        balance();

        // Remove oldest if window is exceeded
        if (window_.size() > window_size_) {
            double oldest = window_.front();
            window_.pop();

            auto it = lowers_.find(oldest);
            if (it != lowers_.end()) {
                lowers_.erase(it);
            } else {
                uppers_.erase(uppers_.find(oldest));
            }
            balance();
        }
    }

    double get_median() const {
        if (lowers_.empty() && uppers_.empty()) return 0.0;
        
        if (lowers_.size() > uppers_.size()) {
            return *lowers_.rbegin();
        } else if (uppers_.size() > lowers_.size()) {
            return *uppers_.begin();
        } else {
            return (*lowers_.rbegin() + *uppers_.begin()) / 2.0;
        }
    }

    size_t size() const {
        return window_.size();
    }

private:
    size_t window_size_;
    std::queue<double> window_;
    std::multiset<double> lowers_; // max-heap conceptually
    std::multiset<double> uppers_; // min-heap conceptually

    void balance() {
        if (lowers_.size() > uppers_.size() + 1) {
            auto it = std::prev(lowers_.end()); // rbegin
            uppers_.insert(*it);
            lowers_.erase(it);
        } else if (uppers_.size() > lowers_.size()) {
            auto it = uppers_.begin();
            lowers_.insert(*it);
            uppers_.erase(it);
        }
    }
};

class RollingVariance {
public:
    explicit RollingVariance(size_t window_size) : window_size_(window_size), sum_(0.0), sum_sq_(0.0) {}

    void add(double value) {
        window_.push(value);
        sum_ += value;
        sum_sq_ += value * value;

        if (window_.size() > window_size_) {
            double oldest = window_.front();
            window_.pop();
            sum_ -= oldest;
            sum_sq_ -= oldest * oldest;
        }
    }

    double get_mean() const {
        if (window_.empty()) return 0.0;
        return sum_ / window_.size();
    }

    double get_std_dev() const {
        if (window_.empty()) return 0.0;
        double mean = get_mean();
        double var = (sum_sq_ / window_.size()) - (mean * mean);
        if (var < 0.0) var = 0.0; // Prevent precision issues leading to negative variance
        return std::sqrt(var);
    }

    size_t size() const {
        return window_.size();
    }

private:
    size_t window_size_;
    std::queue<double> window_;
    double sum_;
    double sum_sq_;
};

} // namespace lob
