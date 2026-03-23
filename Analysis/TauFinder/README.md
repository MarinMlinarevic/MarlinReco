# Workflow to Run Tau Reconstruction with TauFinder
## Compiling Local Versions of PandoraPFA and LCContent
LCContent and PandoraPFA must be compiled locally in order to run the `TauFinder` reconstruction algorithm.
### LCContent
Obtain a local version of the `maia-TauFinder` branch of LCContent:
```bash
git clone -b maia-TauFinder https://github.com/MarinMlinarevic/LCContent.git
cd LCContent
```

The path to PandoraPFA is specified in `CMakeLists.txt` using the following line:
```cmake
set(CMAKE_MODULE_PATH "/opt/spack/opt/spack/linux-almalinux9-x86_64/gcc-11.5.0/pandorapfa-4.11.2-3eayvdubji4xb4rrylket45ckledk7k3/cmakemodules")
```
This works in the container at `/cvmfs/unpacked.cern.ch/ghcr.io/muoncollidersoft/mucoll-sim-alma9:latest`; if you are using a different environment, the path may be different.



Compile the library:
```bash
mkdir build && cd build
cmake -B . -S ../ -DCMAKE_CXX_STANDARD=20 -DCMAKE_INSTALL_PREFIX=$(pwd)/../install
make -j 4 install
```

### DDMarlinPandora
Obtain a local version of DDMarlinPandora:
```bash
git clone https://github.com/MuonColliderSoft/DDMarlinPandora.git
cd DDMarlinPandora
```

Compile the library:
```bash
mkdir build && cd build
cmake -B . -S ../ -DCMAKE_CXX_STANDARD=20 -DCMAKE_INSTALL_PREFIX=$(pwd)/../install -DLCContent_DIR=/<your path   to>/LCContent/install/`
make -j 4 install
```

Replace the path to `libDDMarlinPandora.so` in `$MARLIN_DLL` with the path to the version just compiled (THIS MUST BE REPEATED EACH TIME YOU RUN THE CONTAINER):

```bash
export MARLIN_DLL=$(echo "$MARLIN_DLL" | sed 's#[^:]*libDDMarlinPandora\.so#/<your path to>/DDMarlinPandora/install/lib/libDDMarlinPandora.so#')
```

## Compile MarlinReco and Run `TauFinder`
Compile MarlinReco:
```bash
cd MarlinReco
mkdir build && cd build
cmake -B . -S ../ -DCMAKE_CXX_STANDARD=20 -DCMAKE_INSTALL_PREFIX=$(pwd)/../install -DLCContent_DIR=/<your path to>/LCContent/install/
make -j 4 install
```

Replace the path to `libMarlinReco.so` in `$MARLIN_DLL` with the path to the version just compiled (THIS MUST BE REPEATED EACH TIME YOU RUN THE CONTAINER):

```bash
export MARLIN_DLL=$(echo "$MARLIN_DLL" | sed "s#[^:]*libMarlinReco\.so#/<your path to>/MarlinReco/install/lib/libMarlinReco.so#")
```

Run `TauFinder`:

```bash
Marlin --global.LCIOInputFiles="reco_output.slcio" <your path to>/MarlinReco/Analysis/TauFinder/share/MyTauFinder.xml
```
