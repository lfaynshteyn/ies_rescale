#include <iostream>
#include <filesystem>
namespace fs = std::filesystem;
#include <string_view>

#include <gtest/gtest.h>

#include "ies_rescale.h"

namespace {
	auto rescale_ies_file(const std::string_view fname_in, const std::string_view fname_out, const float rescale_cone_angle, const bool preserve_intensity) -> bool {
		using namespace ies_rescale;

		// Read an IESNA photometric data file
		auto ies_stream = read_file_to_stream(fname_in);
		if (!ies_stream) {
			std::cerr << "Error reading the IES profile file " << fname_in << "\n";
			return false;
		}

		// Convert the IESNA photometric data file to a memory stream
		const auto photo_data = convert_stream_to_data(*ies_stream, fname_in);
		if (!photo_data) {
			std::cerr << "Failed to convert the file " << fname_in << " to a memory stream\n";
			return false;
		}

		// Rescale the IESNA photometric data using the specified cone angle
		const auto scaled_photo_data = rescale_ies_data(*photo_data, rescale_cone_angle, preserve_intensity);
		if (!scaled_photo_data) {
			std::cerr << "Failed to rescale the IES profile " << photo_data->file.name << "\n";
			return false;
		}

		// Dump the data into a memory buffer
		const auto buffer = convert_data_to_buffer(*scaled_photo_data);
		if (!buffer) {
			std::cerr << "Failed to convert the rescaled IES profile to a buffer\n";
			return false;
		}

		// Write the buffer out to a file
		if (!write_buffer_to_file(*buffer, fname_out)) {
			std::cerr << "Failed to write the buffer out to the IES profile file " << fname_out << "\n";
			return false;
		}

		return true;
	}

	// Helper functions tests
	TEST(IesRescale, TestsOnFiles) {

		if (0) {
			const auto fname_in = std::string{ "../test/test_ies_profiles/Type C - 03.ies" };
			const auto file_in_path = fs::path{ fname_in };
			const auto fname_out = file_in_path.parent_path().string() + "/" + file_in_path.stem().string() + "_rescaled.ies";

			const auto rescale_cone_angle = 180.f; // 180 degree rescale cone angle results in the same profile IES profile as the original.
			const auto preserve_intensity = false;

			const auto res = rescale_ies_file(fname_in, fname_out, rescale_cone_angle, preserve_intensity);
			EXPECT_TRUE(res);

			// Test that the rescaled file is the same as the original
			{
				using namespace ies_rescale;

				auto ies_stream_in = read_file_to_stream(fname_in);
				EXPECT_TRUE(ies_stream_in);
				const auto photo_data_in = convert_stream_to_data(*ies_stream_in, fname_in);
				EXPECT_TRUE(photo_data_in);

				auto ies_stream_out = read_file_to_stream(fname_out);
				EXPECT_TRUE(ies_stream_out);
				const auto photo_data_out = convert_stream_to_data(*ies_stream_out, fname_out);
				EXPECT_TRUE(photo_data_out);

				EXPECT_EQ(photo_data_in.value(), photo_data_out.value());
			}
		}

		if (1) {
			const auto fname_in = std::string{ "../test/test_ies_profiles/Type B - 03.ies" };
			const auto file_in_path = fs::path{ fname_in };
			const auto fname_out = file_in_path.parent_path().string() + "/" + file_in_path.stem().string() + "_rescaled.ies";

			const auto rescale_cone_angle = 90.f; // 180 degree rescale cone angle results in the same profile IES profile as the original.
			const auto preserve_intensity = false;

			const auto res = rescale_ies_file(fname_in, fname_out, rescale_cone_angle, preserve_intensity);
		}

	}

} // namespace

auto main(int argc, char** argv) -> int {
	std::cout << "ies_rescale v" << IES_RESCALE_VERSION << " unit tests\n\n";

	::testing::InitGoogleTest(&argc, argv);
	const auto tests_ret = RUN_ALL_TESTS();

	return tests_ret;
}

