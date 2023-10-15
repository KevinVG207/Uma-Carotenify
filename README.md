# Carotenify
(Currently heavily WIP.)

Carotenify is an English translation mod for Uma Musume: Pretty Derby (DMM Edition) and works together with the [Carotene](https://github.com/KevinVG207/Uma-Carotene) English translation project. In essence, it's a powered-up version of [Trainers' Legend G](https://github.com/MinamiChiwa/Trainers-Legend-G)'s translation feature but without any other features.

The mod hooks several functions in the game to replace Japanese text with English text and allows for some modifying of the text rendering settings using special tags. (Documentation coming soon™.)

This readme is adapted from [EXNOA-CarrotJuicer](https://github.com/CNA-Bld/EXNOA-CarrotJuicer/blob/master/README.md).

## Disclaimer
Carotenify is in no way associated with Uma Musume or Cygames Inc. It is the developer's belief that this tool is harmless to the beforementioned company and brand, and merely acts as a tool to improve the user experience.

**However**, this software is against the Terms of Service of the game, so **use at your own risk**. The developer of this software is not responsible for any damages caused by the use of this software.

So far, there are no known cases of bans for using any of the existing translation mods, which use the same methods.


## Usage
Please make sure that you have installed the latest Visual C++ 2019 Redistributable, otherwise the game would crash at start up time with no message at all.

1. Copy `version.dll` to the same directory with `umamusume.exe`. This should be `%USERPROFILE%\Umamusume` unless you chose an alternative installation directory in DMM.
2. If you want to combine it with CarrotJuicer, rename its `version.dll` to `carrotjuicer.dll` and place it in the same folder as `umamusume.exe`.
3. Start the game as usual with DMM launcher.

## Build
(Currently untested.)
1. Install [vcpkg](https://vcpkg.io/en/getting-started.html), and make sure to enable VS integration by running `vcpkg integrate install`.
2. `git clone`
3. Spin up Visual Studio 2019, and press "Build".

## Credits
This project's implementation is effectively copied from [EXNOA-CarrotJuicer](https://github.com/CNA-Bld/EXNOA-CarrotJuicer), which is copied from [umamusume-localify](https://github.com/GEEKiDoS/umamusume-localify), with a little bit of [Trainers' Legend G](https://github.com/MinamiChiwa/Trainers-Legend-G) sprinkled throughout.

I would like to thank all the developers on these three projects for their hard work in setting the groundwork for this project.
