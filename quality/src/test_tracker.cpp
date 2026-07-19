/// @file test_tracker.cpp — Multi-object tracker unit tests (no ROS2)
#include "ros2_robot_middleware/domain/perception/tracker.hpp"

#include <gtest/gtest.h>

using amr::domain::perception::Cluster;
using amr::domain::perception::MultiObjectTracker;
using amr::domain::perception::TrackedObject;
using amr::domain::perception::TrackerParams;

// ── Single detection → single track ──────────────────────────────────

TEST(TrackerTest, SingleDetection_SpawnsTrack) {
  MultiObjectTracker tracker;

  std::vector<Cluster> dets;
  dets.push_back({2.0F, 3.0F, 0.0F, 0, ""});

  auto result = tracker.update(dets, 0.2);
  ASSERT_EQ(result.size(), 1u);
  EXPECT_EQ(result[0].track_id, 0);
  EXPECT_NEAR(result[0].x, 2.0F, 0.1F);
  EXPECT_NEAR(result[0].y, 3.0F, 0.1F);
}

// ── Same detection over multiple frames → same track ID ──────────────

TEST(TrackerTest, PersistentDetection_MaintainsTrackId) {
  MultiObjectTracker tracker;

  Cluster det{1.0F, 0.0F, 0.0F, 0, ""};
  for (int i = 0; i < 5; ++i) {
    auto result = tracker.update({det}, 0.2);
    ASSERT_EQ(result.size(), 1u);
    EXPECT_EQ(result[0].track_id, 0) << "Frame " << i;
    EXPECT_GE(result[0].age, 1);
  }
}

// ── Detection disappears → track pruned after max_missed_frames ──────

TEST(TrackerTest, MissingDetection_PrunedAfterTimeout) {
  TrackerParams p;
  p.max_missed_frames = 3;
  MultiObjectTracker tracker(p);

  // Feed detection once to spawn track
  auto result = tracker.update({{1.0F, 0.0F, 0.0F, 0, ""}}, 0.2);
  ASSERT_EQ(result.size(), 1u);

  // Feed empty frames — track should be pruned after 3 misses
  for (int i = 0; i < 5; ++i) {
    result = tracker.update({}, 0.2);
  }
  EXPECT_EQ(result.size(), 0u);
}

// ── Two separated detections → two tracks ────────────────────────────

TEST(TrackerTest, TwoSeparatedDetections_TwoTracks) {
  MultiObjectTracker tracker;

  std::vector<Cluster> dets;
  dets.push_back({1.0F, 0.0F, 0.0F, 0, ""});
  dets.push_back({5.0F, 0.0F, 0.0F, 0, ""});  // 4m apart > association_radius

  auto result = tracker.update(dets, 0.2);
  ASSERT_EQ(result.size(), 2u);
  EXPECT_NE(result[0].track_id, result[1].track_id);
}

// ── Moving object → KF predicts position ─────────────────────────────

TEST(TrackerTest, MovingObject_KfPredictsPosition) {
  MultiObjectTracker tracker;

  // Object moves from (0, 0) to (2, 1) over 20 frames
  for (int i = 0; i < 20; ++i) {
    float x = 0.1F * i;
    float y = 0.05F * i;
    auto result = tracker.update({{x, y, 0.0F, 0, ""}}, 0.1);
    ASSERT_EQ(result.size(), 1u);
    EXPECT_EQ(result[0].track_id, 0);
    // After a few frames, KF should estimate velocity
    if (i > 5) {
      EXPECT_GT(std::abs(result[0].vx), 0.01F) << "KF should estimate vx after frame " << i;
    }
  }
}

// ── Reset clears all tracks ──────────────────────────────────────────

TEST(TrackerTest, Reset_ClearsAllTracks) {
  MultiObjectTracker tracker;
  tracker.update({{1.0F, 0.0F, 0.0F, 0, ""}}, 0.2);
  EXPECT_EQ(tracker.track_count(), 1u);

  tracker.reset();
  EXPECT_EQ(tracker.track_count(), 0u);

  // IDs restart from 0 after reset
  auto result = tracker.update({{3.0F, 0.0F, 0.0F, 0, ""}}, 0.2);
  EXPECT_EQ(result[0].track_id, 0);
}
