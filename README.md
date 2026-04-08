# MarlinReco
![linux](https://github.com/iLCSoft/MarlinReco/workflows/linux/badge.svg)
[![Coverity Scan Build Status](https://scan.coverity.com/projects/12363/badge.svg)](https://scan.coverity.com/projects/ilcsoft-marlinreco)

Assembly of various Marlin processor for reconstruction.

MarlinReco is distributed under the [GPLv3 License](http://www.gnu.org/licenses/gpl-3.0.en.html)

[![License](https://www.gnu.org/graphics/gplv3-127x51.png)](https://www.gnu.org/licenses/gpl-3.0.en.html)


## Compilation
```shell
cd MarlinReco
mkdir build && cd $_
cmake .. # use the appropriate compiler flags
[make format]  # apply Cpp format guidelines in place (optional)
make -j 4 install
```
This builds MarlinReco in the `build` directory and installs it in the `lib` directory in this repository. 
You can specify different build, source and install paths using arguments of the `cmake` command, e.g.

```bash
cmake -B . -S ../ -DCMAKE_CXX_STANDARD=20 -DCMAKE_INSTALL_PREFIX=$(pwd)/../install -DLCContent_DIR=/<your path to>/LCContent/install/
```

Replace the path to `libMarlinReco.so` in `$MARLIN_DLL` with the path to the version just compiled (THIS MUST BE REPEATED EACH TIME YOU RUN THE CONTAINER):

```bash
export MARLIN_DLL=$(echo "$MARLIN_DLL" | sed "s#[^:]*libMarlinReco\.so#/<your path to>/MarlinReco/install/lib/libMarlinReco.so#")
```

## License and Copyright
Copyright (C), MarlinReco Authors

MarlinReco is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License long with this program.  If not, see <http://www.gnu.org/licenses/>.
