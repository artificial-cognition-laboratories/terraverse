<div align="center">
    <img src="textures/base/pack/logo.png" width="32%">
    <h1>Terraverse</h1>
    <a href="https://www.gnu.org/licenses/old-licenses/lgpl-2.1.en.html"><img src="https://img.shields.io/badge/license-LGPLv2.1%2B-blue.svg" alt="3"></a>
</div>
<br>

## Install

Linux x86_64 only for now, and early: expect rough edges.

```bash
curl -fsSL https://terraverse.arclabs.it/install.sh | sh
```

The game and your worlds live in `~/.terraverse`. `terraverse update` moves to the latest version. `terraverse versions`, `use <version>`, `remove <version>` and `rollback` manage the others. You'll need `curl`, `tar`, `zstd` and `openssl`.

## Overview

Terraverse is built on [Luanti](https://github.com/luanti-org/luanti), a free open-source voxel game engine with easy modding and game creation. The fork has 2 primary goals. Build a open source sandbox for open ended cognition experiments, with first class integration with the (axon)[https://github.com/artificial-cognition-laboratories/axon] platform, and to create an open alternative to minecraft with community first class while keeping the game free to all. 

Weve rebuilt the engine to make worlds spherical. Adding a whole new layer for content, world generation and physics to be added to the game.

## Key Mechanics

- Spherical worlds
- realistic geology
- realistic weather
- visitable solar system
- newtonian physics
- First class modding
- view distance always to horizon with lod.
- full planet creation controls
- elements and compounds
- chemical reactions
- tides
- simulated water
- first class agent support for research usecases

## Spherical Worlds

As we know mapping cubical voxels to a shephe without distortion is impossible. So with some clever math we can hide these distortions as much as possible.

TerraFirm worlds are cube spheres


Table of Contents
------------------

1. [Further Documentation](#further-documentation)
2. [Default Controls](#default-controls)
3. [Paths](#paths)
4. [Configuration File](#configuration-file)
5. [Command-line Options](#command-line-options)
6. [Compiling](#compiling)
7. [Docker](#docker)
8. [Version Scheme](#version-scheme)


Paths
-----
Locations:

* `bin`   - Compiled binaries
* `share` - Distributed read-only data
* `user`  - User-created modifiable data

Where each location is on each platform:

* Windows .zip / RUN_IN_PLACE source:
    * `bin`   = `bin`
    * `share` = `.`
    * `user`  = `.`
* Windows installed:
    * `bin`   = `C:\Program Files\Minetest\bin (Depends on the install location)`
    * `share` = `C:\Program Files\Minetest (Depends on the install location)`
    * `user`  = `%APPDATA%\Minetest` or `%MINETEST_USER_PATH%`
* Linux installed:
    * `bin`   = `/usr/bin`
    * `share` = `/usr/share/minetest`
    * `user`  = `~/.minetest` or `$MINETEST_USER_PATH`
* macOS:
    * `bin`   = `Contents/MacOS`
    * `share` = `Contents/Resources`
    * `user`  = `Contents/User` or `~/Library/Application Support/minetest` or `$MINETEST_USER_PATH`

Worlds can be found as separate folders in: `user/worlds/`

Configuration file
------------------
- Default location:
    `user/minetest.conf`
- This file is created by closing Luanti for the first time.
- A specific file can be specified on the command line:
    `--config <path-to-file>`
- A run-in-place build will look for the configuration file in
    `location_of_exe/../minetest.conf` and also `location_of_exe/../../minetest.conf`

Command-line options
--------------------
- Use `--help`

Compiling
---------

- [Compiling - common information](doc/compiling/README.md)
- [Compiling on GNU/Linux](doc/compiling/linux.md)
- [Compiling on Windows](doc/compiling/windows.md)
- [Compiling on MacOS](doc/compiling/macos.md)

Docker
------

- [Developing minetestserver with Docker](doc/developing/docker.md)
- [Running a server with Docker](doc/docker_server.md)

Version scheme
--------------
We use `major.minor.patch` since 5.0.0-dev. Prior to that we used `0.major.minor`.

- Major is incremented when the release contains breaking changes, all other
numbers are set to 0.
- Minor is incremented when the release contains new non-breaking features,
patch is set to 0.
- Patch is incremented when the release only contains bugfixes and very
minor/trivial features considered necessary.

Since 5.0.0-dev and 0.4.17-dev, the dev notation refers to the next release,
i.e.: 5.0.0-dev is the development version leading to 5.0.0.
Prior to that we used `previous_version-dev`.
