# WinDIO

**Bare bones** of an audio output library for Windows written in C++ with Windows API.
Outputs a specific sound based on provided frequency.

Heavily inspired by: [OlcNoiseMaker](https://github.com/OneLoneCoder/synth/blob/master/olcNoiseMaker.h)

Created as an incentive to learn more about Windows API and, at the very least, the basics of threading plus audio programming.

**Be cautious** when using this, as working with loud sounds and high/low frequencies can (and 
quite possibly will) damage your hearing.

## Used in
- [MaiDai](https://github.com/zermil/maidai)

## Quick start

### Compile/Build (Test):

Remember to link against 'winmm' and 'pthreads'

```console
> build
```

You can also use MSVC build batch file

```console
> msvc
```