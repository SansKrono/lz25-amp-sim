# LZ25 Amp Simulator

A guitar amp sim built with C++ & the JUCE Library. Loosely based on a well known 90's "block letter" amp head.

## Features
* Impulse response loader (load in your favourite IRs!)
* Pre & Post Gain
* Waveshaping distortion
* EQ - Bass, Mid & Treble
* Resonance
* Presence
* Output Meter

## How to Build
(Run cmd/git bash/powershell as administrator)
```
git clone <url>
cd lz25-amp-sim
git submodule update --init
mkdir out
cd out
cmake ..
cmake --build .
```
* More info & how to build on MacOs in the [pamplejuce repo](https://github.com/sudara/pamplejuce)
* You can find some impulse responses if needed [here](https://producelikeapro.com/blog/best-guitar-impulse-responses/)

## Credit
* Level Meter [https://www.youtube.com/@akashmurthy](https://www.youtube.com/watch?v=ILMdPjFQ9ps&ab_channel=AkashMurthy)
* Knobs [github.com/igorpie/Jb_knobs](https://github.com/igorpie/Jb_knobs)

## Made With
* [C++](https://isocpp.org/)
* [Juce](https://juce.com/)
* [PampleJuce](https://github.com/sudara/pamplejuce)
* [CMake](https://cmake.org/)
