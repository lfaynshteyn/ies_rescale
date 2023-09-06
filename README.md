###ies_rescale

A small library that allows to rescale IESNA Photometric Data Profiles, more commonly referred to as simply IES Profiles.

The idea was born out of necessity to handle IES Profiles assigned to Spot Lights in a more natural way. For instance, Unreal engine [masks/clips](https://docs.unrealengine.com/4.27/en-US/BuildingWorlds/LightingAndShadows/IESLightProfiles/) the assigned IES Profile to the Spot Light's Cone Angle , whereas what the user probably wants in practice is to instead have the Profile refitted within the Cone Angle, effectively rescaling the angle values represented in the profile.

After a few iterations, the most logical and intuitive approach I arrived at was based on the assumption that the IES Profile Candela values are measured over a full hemisphere (or sphere, in the case of backscattered emissions), and thus to rescale the profile its vertical angle values would be adjusted according to the scaled projections of the original   Candela values onto the horizontal axis. The scaled Candela values obviously cause the overall emission to decrease, but this approach preserves the overall emission profile shape in a more 'natural' and visually appealing way. There’s a version of the algorithm that preserves the absolute Candela values (except  the ones lying on the horizontal axis), but causes the profile to 'bulge' outward (see the table below for comparison).

| Original IES Profile | Rescaled IES Profile (preserve_intensity = false) | Rescaled IES Profile (preserve_intensity = true) |
|----------|----------|----------|
| <center><img src="https://github.com/lfaynshteyn/lfaynshteyn.github.io/blob/main/projects/ies_rescale/img/Type%20B%20-%20Original.png" width="100%"/></center> | <center><img src="https://github.com/lfaynshteyn/lfaynshteyn.github.io/blob/main/projects/ies_rescale/img/Type%20B%20-%20PA%20False.png" width="100%"/></center> | <center><img src="https://github.com/lfaynshteyn/lfaynshteyn.github.io/blob/main/projects/ies_rescale/img/Type%20B%20-%20PA%20True.png" width="100%"/></center> |
| <center><img src="https://github.com/lfaynshteyn/lfaynshteyn.github.io/blob/main/projects/ies_rescale/img/Type%20C%20-%20Original.png" width="100%"/></center> | <center><img src="https://github.com/lfaynshteyn/lfaynshteyn.github.io/blob/main/projects/ies_rescale/img/Type%20C%20-%20PA%20False.png" width="100%"/></center> | <center><img src="https://github.com/lfaynshteyn/lfaynshteyn.github.io/blob/main/projects/ies_rescale/img/Type%20C%20-%20PA%20True.png" width="100%"/></center> |


The library supports IES Profile file parsing and serialization that is based on the [Ian Ashdown's implementation] (https://seblagarde.wordpress.com/2014/11/05/ies-light-format-specification-and-reader/).

To visualize the original and the rescaled IES profiles I recommend using [IESviewer](http://photometricviewer.com/).

## Usage

First, add the ies_rescale.cpp & ies_rescale.h to your project.
Then you can use the library as follows:

```cpp
#include "ies_rescale/ies_rescale.h"

using namespace ies_rescale;

// Read an IESNA photometric data file
const auto fname_in = std::string{ "../test/test_ies_profiles/Type C - 03.ies" };
auto ies_stream = read_file_to_stream(fname_in);

// Convert the IESNA photometric data file to a memory stream
const auto photo_data = convert_stream_to_data(*ies_stream, fname_in);

// Rescale the IESNA photometric data using the specified cone angle
const auto rescale_cone_angle = 90.f; // 180 degree rescale cone angle results in the same profile IES profile as the original.
const auto preserve_intensity = false;
const auto scaled_photo_data = rescale_ies_data(*photo_data, rescale_cone_angle, preserve_intensity);

// Dump the data into a memory buffer
const auto buffer = convert_data_to_buffer(*scaled_photo_data);

// Write the buffer out to a file
const auto file_in_path = fs::path{ fname_in };
const auto fname_out = file_in_path.parent_path().string() + "/" + file_in_path.stem().string() + "_rescaled.ies";
write_buffer_to_file(*buffer, fname_out));
```

Note: you will need **C++17** at a minimum to compile the code.
