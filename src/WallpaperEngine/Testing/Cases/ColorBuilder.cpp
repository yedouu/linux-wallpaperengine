#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace Catch;

#include "WallpaperEngine/Data/Builders/ColorBuilder.h"

// 0.5f * 255 = 127.5, truncated to 127 by static_cast => 127/255
static constexpr float kAlphaHalf = 127.0f / 255.0f;

TEST_CASE ("ColorBuilder: short #RGB expands and uses the default opaque alpha") {
	const auto color = WallpaperEngine::Data::Builders::ColorBuilder::parse ("#F00");
	CHECK (color.r == Approx (1.0f));
	CHECK (color.g == Approx (0.0f));
	CHECK (color.b == Approx (0.0f));
	CHECK (color.a == Approx (1.0f));
}

TEST_CASE ("ColorBuilder: short #RGB uses the supplied alpha argument") {
	const auto color = WallpaperEngine::Data::Builders::ColorBuilder::parse ("#F0F", 0.5f);
	CHECK (color.r == Approx (1.0f));
	CHECK (color.g == Approx (0.0f));
	CHECK (color.b == Approx (1.0f));
	CHECK (color.a == Approx (kAlphaHalf));
}

TEST_CASE ("ColorBuilder: short #RGBA expands each channel") {
	const auto color = WallpaperEngine::Data::Builders::ColorBuilder::parse ("#1234");
	CHECK (color.r == Approx (0x11 / 255.0f));
	CHECK (color.g == Approx (0x22 / 255.0f));
	CHECK (color.b == Approx (0x33 / 255.0f));
	CHECK (color.a == Approx (0x44 / 255.0f));
}

TEST_CASE ("ColorBuilder: full #RRGGBB keeps the red channel and is opaque") {
	const auto color = WallpaperEngine::Data::Builders::ColorBuilder::parse ("#A0B1C2");
	CHECK (color.r == Approx (0xA0 / 255.0f));
	CHECK (color.g == Approx (0xB1 / 255.0f));
	CHECK (color.b == Approx (0xC2 / 255.0f));
	CHECK (color.a == Approx (1.0f));
}

TEST_CASE ("ColorBuilder: full #RRGGBB respects the supplied alpha argument") {
	const auto color = WallpaperEngine::Data::Builders::ColorBuilder::parse ("#A0B1C2", 0.5f);
	CHECK (color.r == Approx (0xA0 / 255.0f));
	CHECK (color.g == Approx (0xB1 / 255.0f));
	CHECK (color.b == Approx (0xC2 / 255.0f));
	CHECK (color.a == Approx (kAlphaHalf));
}

TEST_CASE ("ColorBuilder: full #RRGGBBAA") {
	const auto color = WallpaperEngine::Data::Builders::ColorBuilder::parse ("#A0B1C2D3");
	CHECK (color.r == Approx (0xA0 / 255.0f));
	CHECK (color.g == Approx (0xB1 / 255.0f));
	CHECK (color.b == Approx (0xC2 / 255.0f));
	CHECK (color.a == Approx (0xD3 / 255.0f));
}

TEST_CASE ("ColorBuilder: invalid hex notation throws") {
	REQUIRE_THROWS (WallpaperEngine::Data::Builders::ColorBuilder::parse ("#12345"));
}
