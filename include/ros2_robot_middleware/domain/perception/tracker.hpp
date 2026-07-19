#pragma once
/// @file   tracker.hpp
/// @brief  Multi-object tracker — nearest-neighbor data association with per-track KF.
///
/// Input:  vector<Cluster> from DBSCAN (each frame)
/// Output: vector<TrackedObject> with persistent IDs across frames
///
/// Algorithm:
///   1. Predict all active tracks forward (KF predict)
///   2. Match detections → tracks via Euclidean nearest-neighbor
///   3. Unmatched detections → spawn new tracks
///   4. Unmatched tracks → increment miss counter → prune if exceeded
///
/// Pure domain logic, zero ROS2 dependency, fully unit-testable.

#include "ros2_robot_middleware/domain/perception/cluster_detector.hpp"
#include "ros2_robot_middleware/domain/perception/kalman_filter.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace amr {
namespace domain {
namespace perception {

/// Tracked object with persistent ID + KF state
struct TrackedObject {
  float x = 0.0F;
  float y = 0.0F;
  float vx = 0.0F;
  float vy = 0.0F;
  int   track_id = 0;
  int   age = 0;       // frames since creation
  int   miss_count = 0; // consecutive frames without detection
};

struct TrackerParams {
  float association_radius = 0.5F;  // max distance for detection→track match (m)
  int   max_missed_frames   = 5;    // prune track after N consecutive misses
  int   max_tracks          = 20;   // max active tracks
};

class MultiObjectTracker {
public:
  explicit MultiObjectTracker(const TrackerParams &p = TrackerParams{}) : params_(p) {}

  /// Process one frame of detections. dt = prediction step (seconds).
  std::vector<TrackedObject> update(const std::vector<Cluster> &detections,
                                     double dt = 0.2) {
    // Step 1: Predict all active tracks forward
    for (auto &t : tracks_) {
      t.kf.predict(dt, 0.0, 0.0);  // no IMU accel in this KF (position-only tracking)
      t.miss_count++;               // assume unmatched until proven otherwise
    }

    // Step 2: Greedy nearest-neighbor association
    int n_det = static_cast<int>(detections.size());
    int n_trk = static_cast<int>(tracks_.size());
    std::vector<bool> det_matched(n_det, false);
    std::vector<bool> trk_matched(n_trk, false);

    // For each unmatched track, find the closest unmatched detection
    // within association_radius
    for (int t = 0; t < n_trk; ++t) {
      double best_dist = params_.association_radius;
      int    best_det  = -1;

      for (int d = 0; d < n_det; ++d) {
        if (det_matched[d]) continue;

        double dx = tracks_[t].kf.x() - detections[d].x;
        double dy = tracks_[t].kf.y() - detections[d].y;
        double dist = std::sqrt(dx * dx + dy * dy);

        if (dist < best_dist) {
          best_dist = dist;
          best_det  = d;
        }
      }

      if (best_det >= 0) {
        // Match found — KF update with detection
        tracks_[t].kf.update(detections[best_det].x, detections[best_det].y);
        tracks_[t].miss_count = 0;
        tracks_[t].age++;
        det_matched[best_det] = true;
        trk_matched[t] = true;
      }
    }

    // Step 3: Spawn new tracks for unmatched detections
    for (int d = 0; d < n_det; ++d) {
      if (det_matched[d]) continue;
      if (static_cast<int>(tracks_.size()) >= params_.max_tracks) break;

      InternalTrack new_track;
      new_track.kf.predict(0.01, 0.0, 0.0);  // tiny predict to settle P
      new_track.kf.update(detections[d].x, detections[d].y);
      new_track.id = next_id_++;
      new_track.age = 1;
      new_track.miss_count = 0;
      tracks_.push_back(new_track);
    }

    // Step 4: Prune stale tracks
    tracks_.erase(
        std::remove_if(tracks_.begin(), tracks_.end(),
                       [this](const InternalTrack &t) {
                         return t.miss_count > params_.max_missed_frames;
                       }),
        tracks_.end());

    // Step 5: Build output
    std::vector<TrackedObject> result;
    result.reserve(tracks_.size());
    for (const auto &t : tracks_) {
      TrackedObject obj;
      obj.x          = t.kf.x();
      obj.y          = t.kf.y();
      obj.vx         = t.kf.vx();
      obj.vy         = t.kf.vy();
      obj.track_id   = t.id;
      obj.age        = t.age;
      obj.miss_count = t.miss_count;
      result.push_back(obj);
    }
    return result;
  }

  /// Number of active tracks
  size_t track_count() const { return tracks_.size(); }

  /// Clear all tracks
  void reset() { tracks_.clear(); next_id_ = 0; }

private:
  struct InternalTrack {
    KalmanFilter2D<> kf;
    int id = 0;
    int age = 0;
    int miss_count = 0;
  };

  TrackerParams params_;
  std::vector<InternalTrack> tracks_;
  int next_id_ = 0;
};

}  // namespace perception
}  // namespace domain
}  // namespace amr
