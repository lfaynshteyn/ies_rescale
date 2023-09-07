/*
 *************************************************************************
 *
 *  IESNA.C - IESNA LM-63 Photometric Data Module
 *
 *  Version:    1.00D
 *
 *  History:    95/08/15 - Created.
 *              95/08/29 - Version 1.00A release.
 *              95/09/01 - Fixed memory deallocation error in IE_Flush.
 *              95/09/04 - Version 1.00B release.
 *              96/01/28 - Added IE_CalcCU_Array, IE_CalcCoeff,
 *                         IE_CalcCU, and IE_CalcData functions.
 *                       - Added zonal multiplier equation constants.
 *                       - Added IE_Cosine array.
 *                       - Added CIE luminaire classification type
 *                         strings.
 *                       - Fixed error messages in IE_GetLine.
 *              96/01/30 - Version 1.00C release.
 *              98/03/07 - Fixed file close problem in IE_ReadFile.
 *              98/03/09 - Version 1.00D release.
 *
 *  Compilers:  Any ANSI C-compliant compiler
 *
 *  Author:     Ian Ashdown, P. Eng.
 *              byHeart Consultants Limited
 *              620 Ballantree Road
 *              West Vancouver, B.C.
 *              Canada V7S 1W3
 *              Tel. (604) 922-6148
 *              Fax. (604) 987-7621
 *
 *  Copyright 1995-1998 byHeart Consultants Limited
 *
 *  Permission: The following source code is copyrighted. However, it may
 *              be freely copied, redistributed, and modified for personal
 *              use or for royalty-free inclusion in commercial programs.
 *
 *************************************************************************
 */

#include <stdlib.h>
#include <iostream>
#include <cstdarg>
#include <string>
#include <cmath>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <cassert>

#include "ies_rescale.h"

namespace ies_rescale {

	// Forward declarations
	static auto ie_populate_array(memstream& file_stream, const std::size_t size) -> std::vector<float>;
	static auto ie_populate_list(memstream& file_stream, const std::string_view format, ...) -> bool;
	static auto ie_read_tilt(memstream& file_stream) -> std::optional<IE_Data::Lamp::Tilt>;
	static auto ie_get_line(memstream& file_stream) -> std::string;

	/*
	 *************************************************************************
	 *
	 *  IE_ReadFile - Read IESNA-Format Photometric Data File
	 *
	 *  Purpose:    To read an IESNA-format photometric data file.
	 *
	 *  Setup:      bool IE_ReadFile
	 *              (
	 *                char *fname,
	 *                IE_Data *pdata
	 *              )
	 *
	 *  Where:      fname is the IESNA-format data file name.
	 *              pdata is a pointer to the photometric data structure the
	 *                file is to be read into.
	 *
	 *  Return:     true if the file was successfully read; otherwise false.
	 *
	 *  Note:       The file is parsed in accordance with the following
	 *              Illuminating Engineering Society of North America
	 *              publications:
	 *
	 *                IES Computer Committee: "IES Recommended Standard File
	 *                Format for Electronic Transfer of Photometric Data and
	 *                Related Information," IES Publication LM-63-1995 (to
	 *                be published).
	 *
	 *                IES Computer Committee: "IES Recommended Standard File
	 *                Format for Electronic Transfer of Photometric Data and
	 *                Related Information," IES Publication LM-63-1991.
	 *
	 *                IES Computer Committee: "IES Recommended Standard File
	 *                Format for Electronic Transfer of Photometric Data,"
	 *                IES Publication LM-63-1986.
	 *
	 *              The latest edition of this publication (currently
	 *              LM-63-1991) may be purchased from:
	 *
	 *                Illuminating Engineering Society of North America
	 *                120 Wall Street, 17th Floor
	 *                New York, NY 10005
	 *
	 *                Tel. (212) 248-5000
	 *
	 *************************************************************************
	 */

	auto read_file_to_stream(const std::string_view fname) -> std::optional<memstream> {
		auto file_data = std::vector<uint8_t>{};
		{
			// Open the IES file
			auto file = std::ifstream{ std::string{fname}, std::ios::binary };
			if (!file || !file.is_open()) {
				return {};
			}

			// Stop eating new lines in binary mode
			file.unsetf(std::ios::skipws);

			// Get the file size
			auto fileSize = std::streampos{};

			file.seekg(0, std::ios::end);
			fileSize = file.tellg();
			file.seekg(0, std::ios::beg);

			// Reserve capacity
			file_data.reserve(fileSize);

			// Read the data
			file_data.insert(
				file_data.begin(),
				std::istream_iterator<uint8_t>(file),
				std::istream_iterator<uint8_t>()
			);
		}

		if (file_data.empty()) {
			return {};
		}

		return std::optional<memstream>{ file_data };
	}

	auto convert_stream_to_data(memstream& file_stream, const std::string_view ies_file_name) -> std::optional<IE_Data> {

		auto data = IE_Data{};

		// Save file name (optional)
		data.file.name = ies_file_name;

		// Read the first line
		auto text_buffer = ie_get_line(file_stream);
		if (text_buffer.empty()) {
			return {};
		}

		{
			// Determine file format
			if (text_buffer == "IESNA:LM-63-1995") {
				// File is LM-63-1995 format
				data.file.format = IE_Data::File::Format::IESNA_95;
			}
			else if (text_buffer == "IESNA:LM-63-2002") {
				// File is LM-63-2002 format
				data.file.format = IE_Data::File::Format::IESNA_02;
			}
			else if (text_buffer == "IESNA91") {
				// File is LM-63-1991 format
				data.file.format = IE_Data::File::Format::IESNA_91;
			}
			else {
				// File is presumably LM-63-1986 format
				data.file.format = IE_Data::File::Format::IESNA_86;

				file_stream.rewind(); // First line is a label line or "TILT="
			}

			// Read label lines
			while (true) {
				text_buffer = ie_get_line(file_stream);
				if (text_buffer.empty()) {
					return {};
				}

				// Check for "TILT" keyword indicating end of label lines
				if (text_buffer.compare(0, 5, "TILT=") == 0) {
					break;
				}

				// Instantiate a new label
				data.labels.push_back(text_buffer);
			}
		}

		auto tilt_str = std::string{};               // TILT line parameter

		{
			tilt_str = text_buffer.substr(5);  // Point to TILT line parameter

			// Save the TILT data file name
			data.lamp.tilt_fname = tilt_str;
		}

		{
			// Check for TILT data
			if (tilt_str != "NONE") {
				if (tilt_str != "INCLUDE") {
					// Open the TILT data file
					auto tilt_stream = read_file_to_stream(tilt_str);
					if (!tilt_stream) {
						return {};
					}
					else {
						// Read the TILT data from the TILT data file
						auto tilt = ie_read_tilt(*tilt_stream);
						if (!tilt) {
							return {};
						}

						data.lamp.tilt = std::move(*tilt);
					}
				}
				else {
					// Read the TILT data from the IESNA data file
					auto tilt = ie_read_tilt(file_stream);
					if (!tilt) {
						return {};
					}

					data.lamp.tilt = std::move(*tilt);
				}
			}
		}

		{
			// Read in next two lines
			const char str[] = "%d%f%f%d%d%d%d%f%f%f";
			if (
				!ie_populate_list(
					file_stream, str,
					&(data.lamp.num_lamps), &(data.lamp.lumens_lamp),
					&(data.lamp.multiplier), &(data.photo.num_vert_angles),
					&(data.photo.num_horz_angles), &(data.photo.gonio_type),
					&(data.units), &(data.dim.width), &(data.dim.length),
					&(data.dim.height)
				)
			) {
				return {};
			}

		}

		{
			const char str[] = "%f%f%f";
			if (!ie_populate_list(file_stream, str, &(data.elec.ball_factor), &(data.elec.blp_factor), &(data.elec.input_watts))) {
				return {};
			}
		}

		{
			// Read in vertical angles array
			data.photo.vert_angles = ie_populate_array(file_stream, data.photo.num_vert_angles);
			if (data.photo.vert_angles.empty()) {
				return {};
			}
			// Read in horizontal angles array
			data.photo.horz_angles = ie_populate_array(file_stream, data.photo.num_horz_angles);
			if (data.photo.horz_angles.empty()) {
				return {};
			}
		}

		{
			// Allocate space for the candela values array pointers
			data.photo.candelas = std::vector<std::vector<float>>(data.photo.num_horz_angles);

			// Read in candela values arrays
			for (int i = 0; i < data.photo.num_horz_angles; i++) {
				// Read in candela values
				data.photo.candelas[i] = ie_populate_array(file_stream, data.photo.num_vert_angles);
				if (data.photo.candelas[i].empty()) {
					return {};
				}
			}
		}

		return std::optional<IE_Data>{std::move(data)};
	}


	auto convert_data_to_buffer(const IE_Data& data) -> std::optional<std::vector<uint8_t>> {

		auto buffer = std::vector<uint8_t>{};

		auto append_vector = [&buffer](const std::vector<uint8_t>& v) {
			buffer.insert(std::end(buffer), std::begin(v), std::end(v));
			};

		auto append_string = [&append_vector](const std::string& s, const std::string s_end = "\n") {
			const auto s_final = s + s_end;
			const auto v = std::vector<uint8_t>(s_final.begin(), s_final.end());
			append_vector(v);
			};

		auto append_int = [&append_vector](const int i, const std::string s_end = "\n") {
			const auto s_final = std::to_string(i) + s_end;
			const auto v = std::vector<uint8_t>(s_final.begin(), s_final.end());
			append_vector(v);
			};

		auto append_float = [&append_vector](const float f, const std::string s_end = "\n", const std::streamsize precision = 2) {

			auto oss = std::ostringstream{};
			oss << std::fixed << std::setprecision(precision) << f;
			auto s = oss.str();
			// Remove trailing zeros
			s.erase(s.find_last_not_of('0') + 1, std::string::npos);
			// Remove trailing decimal point if it's the last character
			if (s.back() == '.') {
				s.pop_back();
			}

			const auto s_final = s + s_end;
			const auto v = std::vector<uint8_t>(s_final.begin(), s_final.end());
			append_vector(v);
			};

		// Output the format string
		{
			auto format = std::string{};

			switch (data.file.format) {
			case IE_Data::File::Format::IESNA_95:
				format = "IESNA:LM-63-1995";
				break;

			case IE_Data::File::Format::IESNA_02:
				format = "IESNA:LM-63-2002";
				break;

			case IE_Data::File::Format::IESNA_91:
				format = "IESNA91";
				break;

			case IE_Data::File::Format::IESNA_86:
				format = "IESNA86";
				break;

			default:
				return {};
			}

			append_string(format);
		}

		// Output the labels
		for (const auto& label : data.labels) {
			append_string(label);
		}

		// Output the tilt data
		if (data.lamp.tilt_fname == "NONE") {
			append_string("TILT=" + data.lamp.tilt_fname);
		}
		else {
			// Otherwise, we always embed (i.e. INCLUDE) the TILT data in the output .ies file
			append_string("TILT=INCLUDE");
			append_int((int)data.lamp.tilt.orientation);
			append_int(data.lamp.tilt.num_pairs);

			for (int i = 0; i < (int)data.lamp.tilt.angles.size(); ++i) {
				const auto angle = data.lamp.tilt.angles[i];
				if (i < (int)data.lamp.tilt.angles.size() - 1)
					append_float(angle, " ");
				else
					append_float(angle, "\n");
			}

			for (int i = 0; i < (int)data.lamp.tilt.mult_factors.size(); ++i) {
				const auto mult_factor = data.lamp.tilt.mult_factors[i];
				if (i < (int)data.lamp.tilt.mult_factors.size() - 1)
					append_float(mult_factor, " ");
				else
					append_float(mult_factor, "\n");
			}
		}

		{
			// Output lamp parameters
			append_int(data.lamp.num_lamps, " ");
			append_float(data.lamp.lumens_lamp, " ");
			append_float(data.lamp.multiplier, " ");
			append_int(data.photo.num_vert_angles, " ");
			append_int(data.photo.num_horz_angles, " ");
			append_int(data.photo.gonio_type, " ");
			append_int(data.units, " ");
			append_float(data.dim.width, " ");
			append_float(data.dim.length, " ");
			append_float(data.dim.height, "\n");
		}

		{
			// Output elec parameters
			append_float(data.elec.ball_factor, " ");
			append_float(data.elec.blp_factor, " ");
			append_float(data.elec.input_watts, "\n");
		}

		// Output vertical angles
		for (int i = 0; i < data.photo.num_vert_angles; ++i) {
			const float angle = data.photo.vert_angles[i];
			if (i < data.photo.num_vert_angles - 1)
				append_float(angle, " ");
			else
				append_float(angle, "\n");
		}

		// Output horizontal angles
		for (int i = 0; i < data.photo.num_horz_angles; ++i) {
			const float angle = data.photo.horz_angles[i];
			if (i < data.photo.num_horz_angles - 1)
				append_float(angle, " ");
			else
				append_float(angle, "\n");
		}

		// Output candela values arrays
		for (int i = 0; i < data.photo.num_horz_angles; ++i)
		{
			for (int j = 0; j < data.photo.num_vert_angles; ++j) {
				const float candela = data.photo.candelas[i][j];
				if (j < data.photo.num_vert_angles - 1)
					append_float(candela, " ");
				else
					append_float(candela, "\n");
			}
		}

		return std::optional<std::vector<uint8_t>>{std::move(buffer)};
	}

	auto write_buffer_to_file(const std::vector<uint8_t>& buffer, const std::string_view file_name) -> bool {
		auto file = std::ofstream(std::string{file_name}, std::ios::binary);
		if (!file || !file.is_open()) {
			std::cerr << "Could not open file " << file_name << "\n";
			return false;
		}

		// Write the buffer to the file
		if (!file.write((const char*)buffer.data(), buffer.size())) {
			std::cerr << "Could not write to file " << file_name << "\n";
			return false;
		}

		return true;
	}


	//! Rescale the vertical angles (and associated candela values) originally defined in the [0 : 180] degrees range (i.e. a hemisphere)
	//! into the new cone angle in the [0 : rescale_cone_angle] range.
	//! The assumption is that the original emission profile was measured over a hemisphere (i.e. 180 degrees) tangent to the lamp's surface,
	//! and that the rescaled emission profile will be 'fitted' inside the new rescaled code specified by the rescale_cone_angle parameter.
	//! \param[in]		data						The IES data to rescale
	//! \param[in]		rescale_cone_angle			The new cone in degrees to rescale the vertical angles to
	//! \param[in]		preserve_intensity			TYhe flag indicating whether to preserve the intensity values of the original IES data (default: false)
	//! \return			std::optional<IE_Data>
	//!         A rescaled IES data on success or an empty object on failure
	auto rescale_ies_data(const IE_Data& data, const float rescale_cone_angle, const bool preserve_intensity) -> std::optional<IE_Data> {

		// Make sure the rescale cone angle is valid.
		if (rescale_cone_angle < 0.f || rescale_cone_angle > 180.f) {
			return {};
		}

		constexpr auto PI = 3.14159265358979323846;

		auto degrees_to_radians = [&PI](const auto degrees) -> decltype(degrees) {
			return degrees * (decltype(degrees))(PI / 180.0);
			};

		auto radians_to_degrees = [&PI](const auto radians) -> decltype(radians) {
			return radians * (decltype(radians))(180.0 / PI);
			};

		static const auto horizontal_threshold_angle_rad = degrees_to_radians(90.f + 1.f);
		static const auto threshold_projected_on_y = std::abs(std::cos(horizontal_threshold_angle_rad));

		// Calculate the uniform scale factor that will be applied to all horizontal projected values.
		// Note, #rescale_cone_angle values of/close to 0 will cause the emission profile to be 'squashed' into a single vertical line.
		const auto projected_x_scale = std::sin(degrees_to_radians(rescale_cone_angle * .5f));
		assert((projected_x_scale >= 0.f && projected_x_scale <= 1.f) && "Invalid projected x scale");

		// Make a copy of the input data, that will eventually be rescaled.
		auto scaled_data = data;

		// Rescale all output candela value arrays.
		for (auto i = 0; i < data.photo.num_horz_angles; ++i) {
			const auto horz_angle = data.photo.horz_angles[i];

			for (int j = 0; j < data.photo.num_vert_angles; ++j) {

				const auto candela = data.photo.candelas[i][j];

				if (candela <= 0.f) {
					// If the candela value is 0, there's no need to rescale it.
					continue;
				}

				const auto vert_angle = data.photo.vert_angles[j];

				const auto is_top_hemisphere = vert_angle > 90.f;

				//if (is_top_hemisphere) {
				//	int n = 0;
				//}

				const auto vert_angle_orig = is_top_hemisphere ? 180.f - vert_angle : vert_angle;
				const auto vert_angle_orig_rad = degrees_to_radians(vert_angle_orig);
				const auto candela_projected_on_y_orig = candela * std::cos(vert_angle_orig_rad);
				const auto candela_projected_on_x_orig = candela * std::sin(vert_angle_orig_rad);

				// Detect the 90 (or close to it) degree vertical angle case
				const auto is_angle_almost_90 = std::abs(std::cos(vert_angle_orig_rad)) <= threshold_projected_on_y;

				auto scaled_angle = float{ 0.f };
				auto scaled_candela = float{ 0.f };

				if (!preserve_intensity) {
					// Uniformly shift/scale the projected X-axis candela values towards the center of the emission profile (i.e. Y-axis).
					// This will naturally cause the foreshortening of all candela values, but will preserve the overall emission profile shape in a more 'natural' way if you will
					// (hence this is the default mode).
					const auto candela_projected_on_x_scaled = candela_projected_on_x_orig * projected_x_scale;
					const auto scaled_angle_rad = std::atan(candela_projected_on_x_scaled / candela_projected_on_y_orig);

					scaled_angle = is_angle_almost_90 ? vert_angle_orig : radians_to_degrees(scaled_angle_rad);
					scaled_candela = std::sqrt(candela_projected_on_y_orig * candela_projected_on_y_orig + candela_projected_on_x_scaled * candela_projected_on_x_scaled);
				}
				else {
					// Similarly, shift/scale the projected X-axis candela values towards the center of the emission profile,
					// but use the original candela value as the hypotenuse, so that the original emission intensity values are preserved (except for the values on/close to the horizontal X-axis,
					// as doing so will produce emission profiles with 'inflated waist' that always stays the same width regardless of the specified #rescale_cone_angle value).
					// This causes the profile to be sort of 'funneled' into the new cone angle (as if you're closing an umbrella),
					// which can result in sharp features in the IES profile becoming ever more sliver-like with smaller #rescale_cone_angle values.
					// This will better preserve the overall amount of light emitted by the luminaire, but will cause the emission profile to appear distorted in the shape of a teardrop.
					const auto candela_projected_on_x_scaled = candela_projected_on_x_orig * projected_x_scale;
					const auto scaled_angle_rad = std::asin(candela_projected_on_x_scaled / candela);

					// We can't allow the near-horizontal angles to be shifted/rotated into either hemisphere as in the case of double-sided emitters it'll cause gaps in the center of the emission profile.
					scaled_angle = is_angle_almost_90 ? vert_angle_orig : radians_to_degrees(scaled_angle_rad);
					scaled_candela = is_angle_almost_90 ? candela_projected_on_x_scaled : candela;
				}

				scaled_data.photo.vert_angles[j] = (is_top_hemisphere ? 180.f - scaled_angle : scaled_angle);
				scaled_data.photo.candelas[i][j] = scaled_candela;

			}
		}

		return std::optional<IE_Data>{scaled_data};
	}


	//! Read TILT data from a memstream contacting IESNA-format data into a photometric data structure.
	//! \param[in]		mem_stream								The memory stream initialized with the content of an IES profile file
	//! \return			std::optional<IE_Data::Lamp::Tilt>		The read TILT data on success or an empty object of failure
	//! Note: the file can be either part of a full IESNA-format data file or a separate TILT data file that was specified in the parent IESNA-format file on the "TILT=" line.
	static auto ie_read_tilt(memstream& mem_stream) -> std::optional<IE_Data::Lamp::Tilt> {
		// Read the lamp-to-luminaire geometry line
		auto value_buffer = ie_get_line(mem_stream);
		if (value_buffer.empty()) {
			return {};
		}

		auto tilt = IE_Data::Lamp::Tilt{};

		{
			// Get the lamp-to-luminaire geometry value
			tilt.orientation = (IE_Data::Lamp::Tilt::Orientation)std::stoi(value_buffer);

			// Read the number of angle-multiplying factor pairs line
			value_buffer = ie_get_line(mem_stream);
			if (value_buffer.empty()) {
				return {};
			}
		}

		{
			// Get the number of angle-multiplying factor pairs value
			tilt.num_pairs = std::stoi(value_buffer);

			if (tilt.num_pairs > 0) {
				// Read in the angle values
				tilt.angles = ie_populate_array(mem_stream, tilt.num_pairs);
				if (tilt.angles.empty()) {
					return {};
				}

				// Read in the multiplying factor values
				tilt.mult_factors = ie_populate_array(mem_stream, tilt.num_pairs);
				if (tilt.mult_factors.empty()) {
					return {};
				}
			}
		}

		return std::optional<IE_Data::Lamp::Tilt>{ tilt };
	}


	//! Read in one or more lines from a memstream contacting IESNA-format data and convert their substrings to a list of floating point and/or integer values.
	//! \param[in]		mem_stream			The memory stream initialized with the content of an IES profile file
	//! \param[in]		format				The format string containing the following specifiers:
	//!									%d - read in integer value
	//!									%f - read in floating point value
	//! \param[out]	...					A pointer to a variable of the appropriate type must follow the format parameter in the same order (similar to "scanf").
	//! \return
	//!         A boolean value indicating success or failure
	static auto ie_populate_list(memstream& mem_stream, const std::string_view format, ...) -> bool {

		// Todo: rewrite this function as a variadic template function to avoid the archaic and error-prone va_list stuff.

		// Read in the first line
		auto buffer = ie_get_line(mem_stream);
		if (buffer.empty()) {
			return false;
		}

		auto p_buffer = &buffer[0];
		for (; ; ) {   // Skip over leading delimiters
			const auto c = *p_buffer;
			if (c == '\0') {      // End of current line?
				return false;
			}
			else if (isspace(c) != 0) {
				++p_buffer;
			}
			else {
				break;
			}
		}

		va_list p_args;         // Variable list argument pointer
		va_start(p_args, format);       // Set up for optional arguments
		const auto* p_format = format.data();  // Format string pointer

		for (; ; ) {
			if (*p_format != '%') {          // Check format specifier delimiter
				va_end(p_args);   // Clean up
				return false;
			}

			// Get and validate format specifier
			const auto type = *(p_format + 1);
			switch (type) {
			case 'd':
			case 'D':
			{
				auto itemp = int{ 0 };
				auto iss = std::istringstream{ p_buffer };
				iss >> itemp;
				if (iss.fail()) {
					assert(false && "failed to parse the current substring to its integer point value");
					va_end(p_args);   // Clean up
					return false;
				}

				*(va_arg(p_args, int*)) = itemp;

				for (; ; ) {    // Advance buffer pointer past the substring
					const auto c = *p_buffer;
					if ((isdigit(c) != 0) || c == '-') {
						++p_buffer;
					}
					else {
						break;
					}
				}
				break;
			}
			case 'f':
			case 'F':
			{
				auto ftemp = float{ 0.f };
				auto iss = std::istringstream{ p_buffer };
				iss >> ftemp;
				if (iss.fail()) {
					assert(false && "failed to parse the current substring to its floating point value");
					va_end(p_args);   // Clean up
					return false;
				}

				*(va_arg(p_args, float*)) = ftemp;

				for (; ; ) {    // Advance buffer pointer past the substring
					const auto c = *p_buffer;
					if ((isdigit(c) != 0) || c == '.' || c == '-') {
						++p_buffer;
					}
					else {
						break;
					}
				}
				break;
			}
			default:
				va_end(p_args);   // Clean up
				return false;
			}

			// Point to next format specifier

			++p_format;             // Skip over format specifier delimiter
			if (*p_format == '\0') { // End of format string ?
				va_end(p_args);   // Clean up
				return false;
			}

			++p_format;             // Skip over format specifier parameter
			if (*p_format == '\0') { // End of format string ?
				break;
			}

			for (; ; ) {        // Skip over delimiters
				const auto c = *p_buffer;
				if (c == '\0') {   // End of current line?
					// Get next line
					buffer = ie_get_line(mem_stream);
					if (buffer.empty()) {
						va_end(p_args);   // Clean up
						return false;
					}

					p_buffer = &buffer[0];
				}
				else if ((isspace(c) != 0) || c == ',') {
					++p_buffer;
				}
				else {
					break;
				}
			}
		}

		va_end(p_args);   // Clean up
		return true;
	}


	//! Read in one or more lines from an IESNA-format data file and convert their substrings to an array of floating point numbers.
	//! \param[in]		mem_stream			The memory stream initialized with the content of an IES profile file
	//! \param[in]		size				The number of floats to read in
	//! \return
	//!         A populated array of floats on success, an empty array on failure
	static auto ie_populate_array(memstream& mem_stream, const std::size_t size) -> std::vector<float> {
		auto buffer = std::string{};

		// Read in the first line 
		buffer = ie_get_line(mem_stream);
		if (buffer.empty()) {
			return {};
		}

		auto p_buffer = &buffer[0];
		auto c = char{};
		for (; ; ) {  // Skip over leading delimiters
			c = *p_buffer;

			if (c == '\0') {     // End of current line?
				return {};
			}
			else if ((isspace(c) != 0)) {
				p_buffer++;
			}
			else {
				break;
			}
		}

		auto array = std::vector<float>(size, 0.f);

		for (auto i = size_t{ 0 }; ; ) {   // Parse the array elements

			// Convert the current substring to its floating point value
			float ftemp;          // Temporary floating point variable
			std::istringstream iss(p_buffer);
			iss >> ftemp;
			if (iss.fail()) {
				assert(false && "failed to parse the current substring to its floating point value");
				return {};
			}

			array[i++] = ftemp;

			if (i == array.size()) {     // All substrings converted ?
				break;
			}

			for (; ; ) {        // Advance buffer pointer past the substring

				c = *p_buffer;
				if ((isdigit(c) != 0) || c == '.' || c == '-') {
					p_buffer++;
				}
				else {
					break;
				}
			}

			for (; ; ) {        // Skip over delimiters
				if (c == '\0') {   // End of current line?
					// Get next line
					buffer = ie_get_line(mem_stream);
					if (buffer.empty()) {
						return {};
					}

					p_buffer = &buffer[0];
				}
				else if ((isspace(c) != 0) || c == ',') {
					p_buffer++;
				}
				else {
					break;
				}

				c = *p_buffer;        // Get next delimiter
			}
		}

		return array;
	}


	//! Get a line from the memstream and stores it in the provided buffer
	//! \param[in]	mem_stream			The memory stream initialized with the content of an IES profile file
	//! \return
	//!         A string containing the read line on success or an empty string on failure
	//!         \c nullptr on failure
	static auto ie_get_line(memstream& mem_stream) -> std::string {
		auto buffer = std::string{};
		std::getline(mem_stream, buffer);

		if (!buffer.empty() && buffer.back() == 0x0D) {
			buffer.pop_back();
		}

		if (buffer.size()) {
			return buffer;
		}

		return "";
	}

} // namespace ies_rescale