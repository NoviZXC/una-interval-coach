// QA.md A8: settings persistence, on the SDK's in-memory file system.
#include <gtest/gtest.h>

#include "KernelTestDoubles.hpp"
#include "SettingsStore.hpp"

using SDK::TestSupport::KernelFixture;

TEST(SettingsStore, RoundTrip)
{
    KernelFixture fixture;
    SettingsStore store(fixture.kernel, "settings.json");
    IC::Settings out;
    out.reps = 8;
    out.repDistanceM = 600;
    out.hrTarget = 135;
    out.maxRestS = 195;
    ASSERT_TRUE(store.save(out));

    IC::Settings in;
    ASSERT_TRUE(store.load(in));
    EXPECT_EQ(in.reps, 8);
    EXPECT_EQ(in.repDistanceM, 600);
    EXPECT_EQ(in.hrTarget, 135);
    EXPECT_EQ(in.maxRestS, 195);
}

TEST(SettingsStore, MissingFileKeepsDefaults)
{
    KernelFixture fixture;
    SettingsStore store(fixture.kernel, "settings.json");
    IC::Settings s;
    EXPECT_FALSE(store.load(s));
    EXPECT_EQ(s.reps, 6);
    EXPECT_EQ(s.repDistanceM, 400);
}

TEST(SettingsStore, CorruptFileGivesDefaults)
{
    KernelFixture fixture;
    ASSERT_TRUE(fixture.fileSystem.seedFile("settings.json", "{ reps: 8, oops"));
    SettingsStore store(fixture.kernel, "settings.json");
    IC::Settings s;
    EXPECT_FALSE(store.load(s));
    EXPECT_EQ(s.reps, 6);
    EXPECT_EQ(s.hrTarget, 120);

    // The next save rewrites it as valid JSON.
    ASSERT_TRUE(store.save(s));
    IC::Settings again;
    EXPECT_TRUE(store.load(again));
}

TEST(SettingsStore, OutOfRangeValuesClamped)
{
    KernelFixture fixture;
    ASSERT_TRUE(fixture.fileSystem.seedFile("settings.json",
        R"({"reps": 99, "rep_distance_m": 7, "hr_target": 999, "max_rest_s": 100000})"));
    SettingsStore store(fixture.kernel, "settings.json");
    IC::Settings s;
    ASSERT_TRUE(store.load(s));
    EXPECT_EQ(s.reps, 20);
    EXPECT_EQ(s.repDistanceM, 100);
    EXPECT_EQ(s.hrTarget, 160);
    EXPECT_EQ(s.maxRestS, 300);
}

TEST(SettingsStore, PartialFileKeepsOtherDefaults)
{
    KernelFixture fixture;
    ASSERT_TRUE(fixture.fileSystem.seedFile("settings.json", R"({"reps": 10})"));
    SettingsStore store(fixture.kernel, "settings.json");
    IC::Settings s;
    ASSERT_TRUE(store.load(s));
    EXPECT_EQ(s.reps, 10);
    EXPECT_EQ(s.repDistanceM, 400);
    EXPECT_EQ(s.maxRestS, 120);
}
