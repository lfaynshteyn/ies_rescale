/*
 *************************************************************************
 *
 *  IESNA.H - IESNA LM-63 Photometric Data Module Include File
 *
 *  Version:    1.00D
 *
 *  History:    95/08/15 - Created.
 *              95/08/29 - Version 1.00A release.
 *              95/09/01 - Revised IE_MaxLabel and IE_MaxLine definitions.
 *              95/09/04 - Version 1.00B release.
 *              96/01/28 - Added CIE luminaire type classification
 *                         definitions.
 *                       - Added calculated photometric data definitions.
 *                       - Added IE_Calc and IE_Zonal data structures.
 *                       - Added IE_CalcData and IE_CalcCU_Array function
 *                         prototypes.
 *                       - Added coefficients of utilization array
 *                         dimensions.
 *                       - Added IE_CU_Array and IE_CIE_Type
 *                         declarations.
 *                       - Added definition for pi.
 *              96/01/30 - Version 1.00C release.
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

 // --->
 // 
 // The following is a summary of the major changes
 //	from LM-63-1995 to LM-63-2002:
 //	- The first line in the file is now IESNA : LM - 63 - 2002
 //	(see Section 5.1).
 //	- All lines can now be 256 characters in length
 //	(see Section 6).
 //	- All IESNA LM-63-2002 filenames shall now have
 //	the file extension ies or IES.
 //	- The following keywords are now required : [TEST] ,
 //	[TESTLAB], [MANUFAC], and [ISSUEDATE]
 //	(see Section 5.2).
 //	-[DATE] keyword replaced with[ISSUEDATE]
 //	(see Annex B).
 //	-[BLOCK] and [ENDBLOCK] keywords have been
 //	removed.
 //	- All tilt filenames(TILT = <filename>) shall now
 //	have the file extension tlt or TLT(tilt format
 //	moved to Annex G).
 //	- Definitions of the Luminous Opening have been
 //	expanded and, in some cases, modified.The fol -
 //	ANSI / IESNA LM-63-02
 //	2
 //	lowing shapes are directly affected : Circular,
 //	Sphere, Vertical Cylinder, Horizontal Cylinder,
 //	Ellipse, and Ellipsoid(see Table 1 and Annex D).
 //	- The allowance for horizontal angles starting at 90
 //	degrees and ending at 270 degrees for Type C
 //	photometry has been removed.
 //	-[LAMPPOSITION] keyword has been added(see
 //		Annex E).
 // <---

#ifndef IES_RESCALE_H
#define IES_RESCALE_H

#include <string>
#include <string_view>
#include <vector>
#include <optional>
#include <streambuf>
#include <istream>

namespace ies_rescale {

	namespace detail {
		class membuf : public std::streambuf {
		private:
			std::vector<uint8_t> buffer_;

		protected:
			membuf() {
				this->setg(nullptr, nullptr, 0);
			}

			membuf(const std::vector<uint8_t>& buf)
				: buffer_(buf) {
				reset();
			}

			void reset() {
				auto p = reinterpret_cast<char*>(buffer_.data());
				this->setg(p, p, p + buffer_.size());
			}

			auto underflow() -> int_type override {
				// This implementation can't underflow, since all the data is stored in the buffer_ upon construction.
				// So we simply return EOF to handle such things as a call to std::getline() in the absence of a terminating newline character.
				return traits_type::eof();
			}

		};
	}

	class memstream : virtual private detail::membuf, virtual public std::istream {
	public:
		memstream()
			: membuf()
			, std::istream(static_cast<std::streambuf*>(this))
		{}

		memstream(const std::vector<uint8_t>& buffer)
			: membuf(buffer)
			, std::istream(static_cast<std::streambuf*>(this))
		{}

		void rewind() {
			this->reset();
		}
	};

	// IESNA Standard File data
	struct IE_Data {
		struct File {						// File information
			std::string name;               // Name
			enum Format {                   // Format
				IESNA_86,					// LM-63-1986
				IESNA_91,					// LM-63-1991
				IESNA_95,					// LM-63-1995
				IESNA_02,					// LM-63-2002 - the latest format and the one supported by Nvidia Iray
			} format;

			bool operator==(const File& other) const {
				return
					//name == other.name &&	// Do not compare the name attribute as it's optional and the otherwise identical IES files may have different names.
					format == other.format
					;
			}

			bool operator!=(const File& other) const {
				return !(*this == other);
			}
		} file;

		std::vector<std::string> labels;	// Label lines

		struct Lamp {						// Lamp data
			int num_lamps;					// Number of lamps
			float lumens_lamp;				// Lumens per lamp
			float multiplier;				// Candela multiplying factor
			std::string tilt_fname;         // TILT file name pointer (optional)

			struct Tilt {                   // TILT data structure
				enum Orientation {          // Lamp-to-luminaire geometry
					LampVert = 1,           // Lamp vertical base up or down
					LampHorz = 2,           // Lamp horizontal
					LampTilt = 3            // Lamp tilted
				} orientation;

				int num_pairs;						// # of angle-multiplying factor pairs
				std::vector<float> angles;			// Angles array pointer
				std::vector<float> mult_factors;	// Multiplying factors array pointer

				bool operator==(const Tilt& other) const {
					return
						orientation == other.orientation
						&& num_pairs == other.num_pairs
						&& angles == other.angles
						&& mult_factors == other.mult_factors
						;
				}

				bool operator!=(const Tilt& other) const {
					return !(*this == other);
				}

			} tilt;

			bool operator==(const Lamp& other) const {
				return
					num_lamps == other.num_lamps
					&& lumens_lamp == other.lumens_lamp
					&& multiplier == other.multiplier
					&& tilt_fname == other.tilt_fname
					&& tilt == other.tilt
					;
			}

			bool operator!=(const Lamp& other) const {
				return !(*this == other);
			}
		} lamp;

		enum Units {						// Measurement units
			Feet = 1,						// Imperial
			Meters = 2						// Standard Internationale
		} units;

		struct Dim {						// Luminous cavity dimensions
			float width;					// Opening width
			float length;					// Opening length
			float height;					// Cavity height

			bool operator==(const Dim& other) const {
				return
					width == other.width
					&& length == other.length
					&& height == other.height
					;
			}

			bool operator!=(const Dim& other) const {
				return !(*this == other);
			}
		} dim;

		struct Elec {						// Electrical data
			float ball_factor;				// Ballast factor
			float blp_factor;				// Ballast-lamp photometric factor
			float input_watts;				// Input watts

			auto operator==(const Elec& other) const {
				return
					ball_factor == other.ball_factor
					&& blp_factor == other.blp_factor
					&& input_watts == other.input_watts
					;
			}

			auto operator!=(const Elec& other) const {
				return !(*this == other);
			}
		} elec;

		struct Photo {						// Photometric data
			enum IE_Gonio_Type {            // Photometric goniometer type
				Type_A = 3,					// Type A
				Type_B = 2,					// Type B
				Type_C = 1					// Type C
			} gonio_type;

			int num_vert_angles;			// Number of vertical angles
			int num_horz_angles;			// Number of horizontal angles

			std::vector<float> vert_angles;         // Vertical angles array
			std::vector<float> horz_angles;         // Horizontal angles array

			std::vector<std::vector<float>> candelas;           // Candela values array pointers array

			bool operator==(const Photo& other) const {
				return
					gonio_type == other.gonio_type
					&& num_vert_angles == other.num_vert_angles
					&& num_horz_angles == other.num_horz_angles
					&& vert_angles == other.vert_angles
					&& horz_angles == other.horz_angles
					&& candelas == other.candelas
					;
			}

			bool operator!=(const Photo& other) const {
				return !(*this == other);
			}
		} photo;

		bool operator==(const IE_Data& other) const {
			return
				file == other.file
				&& labels == other.labels
				&& lamp == other.lamp
				&& units == other.units
				&& dim == other.dim
				&& elec == other.elec
				&& photo == other.photo
				;
		}

		bool operator!=(const IE_Data& other) const {
			return !(*this == other);
		}
	}; // struct IE_Data


	auto read_file_to_stream(const std::string_view file_name) -> std::optional<memstream>;
	auto convert_stream_to_data(memstream& file_stream, const std::string_view ies_file_name = "") -> std::optional<IE_Data>;
	auto convert_data_to_buffer(const IE_Data& data) ->std::optional<std::vector<uint8_t>>;
	auto write_buffer_to_file(const std::vector<uint8_t>& buffer, const std::string_view file_name) -> bool;
	auto rescale_ies_data(const IE_Data& in_data, const float rescale_angle, const bool preserve_intensity = false) -> std::optional<IE_Data>;

} // namespace ies_rescale

#endif // IES_RESCALE_H