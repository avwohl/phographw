# Building

The README covers the plain CMake build. This page covers the Visual Studio workflow and the MSIX installer.

## Open in Visual Studio

    cmake -B build
    start build\Phograph.sln

Select the **Phograph** target, set the configuration to **Release** or **Debug**, and press **F5** to build and run.

## Building the MSIX Package

To build an MSIX installer for sideloading or Windows Store submission:

    packaging\build_msix.bat Release

This will:

1. Build the executable with CMake (Release config)
2. Stage the EXE, manifest, and store assets
3. Create `resources.pri` via MakePri.exe
4. Pack the MSIX via MakeAppx.exe
5. Sign with a self-signed development certificate

The output is `build\Phograph.msix`.
