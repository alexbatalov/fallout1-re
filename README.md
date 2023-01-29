# Fallout Reference Edition

In this repository you'll find reverse engineered source code for Fallout: A Post Nuclear Role Playing Game.

This project is based on and share much of the code with [Fallout 2 Reference Edition](https://github.com/alexbatalov/fallout2-re), which I had decompiled earlier. Original Fallout engine is simpler in many ways, but it's still a different game, so I've decided to keep Fallout 1 and Fallout 2 as separate projects.

## Goal

The goal of this project is to restore original source code as close as possible with all it's imperfections. This means Windows/x86/640x480 among many other things. Original Fallout also had DOS and Mac OS X ports which are out of scope of this project.

## Status

The game can be completed from start to finish. About 6% of functions from Fallout 2 codebase is currently under review. As with F2RE there is a small number of functions that are not decompiled because they are never used in the game.

## Installation

You must own the game to play. Purchase your copy on [GOG](https://www.gog.com/game/fallout) or [Steam](https://store.steampowered.com/app/38400). Download latest build or build from source. The `fallout-re.exe` serves as a drop-in replacement for `falloutw.exe`. Copy it to your Fallout directory and run.

## Contributing

The best thing you can do is to play and report bugs or inconsistencies. Attach zipped save if needed.

Please do not submit new features or any code that is not present in Fallout 1 binary. Once decompilation/review process is completed the development will be continued in the new repository. This repository will be left intact for historical reasons.

## Special Thanks

- [c6](https://github.com/c6-dev): for extensive gameplay testing.

## Legal

The source code in this repository is produced by reverse engineering the original binary. There are couple of exceptions for reverse engineering under DMCA - documentation, interoperability, fair use. Documentation is needed to achieve interoperability. Running your legally purchased copy on modern Mac M1 for example (interoperability in action) constitutes fair use. Publishing this stuff to wide audience is questionable. Eventually it's up to Bethesda/Microsoft to takedown the project or leave it be. See [#29](https://github.com/alexbatalov/fallout2-re/issues/29) for discussion.

## License

The source code is this repository is available under the [Sustainable Use License](LICENSE.md).
