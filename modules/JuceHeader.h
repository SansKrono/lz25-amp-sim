#pragma once

// Minimal JuceHeader shim for integrating TS-808-Ultra DSP into this CMake project.
// This header provides the JUCE and chowdsp includes expected by the submodule's headers.

// JUCE modules used by the TS-808-Ultra DSP code
#include <juce_core/juce_core.h>
#include <juce_dsp/juce_dsp.h>

// Include minimal chowdsp dependencies needed by the clipper stages
#include "TS-808-Ultra/modules/chowdsp_utils/modules/chowdsp_dsp/SIMD/chowdsp_SIMDUtils.h"

// Provide SampleTypeHelpers in the expected namespace for chowdsp WDF
namespace chowdsp { namespace WDFT { namespace SampleTypeHelpers = juce::dsp::SampleTypeHelpers; } }

#include "TS-808-Ultra/modules/chowdsp_utils/modules/chowdsp_dsp/WDF/wdf.h"

// TS-808-Ultra sources assume Projucer-generated JuceHeader.h which adds this
using namespace juce;
using namespace juce::dsp;
