:: From the `third-party` folder:

rmdir /s /q oneTBB-install
cd oneTBB
rmdir /s /q build
cmake -G "Visual Studio 17 2022" -A x64 -B build -S . -DBUILD_SHARED_LIBS=OFF
cmake --build build --config Release && cmake --install build --config Release --prefix "..\oneTBB-install"
cmake --build build --config Debug && cmake --install build --config Debug --prefix "..\oneTBB-install"
cd ..

:: After commenting out `add_subdirectory (js)` in `gtsam/3rdparty/GeographicLib/CMakeLists.txt`

rmdir /s /q GTSAM-install
cd GTSAM
rmdir /s /q build
cmake -G "Visual Studio 17 2022" -A x64 -B build -S . -DGTSAM_INSTALL_GEOGRAPHICLIB=ON
cmake --build build --config Release && cmake --install build --config Release --prefix "..\GTSAM-install"
cmake --build build --config Debug && cmake --install build --config Debug --prefix "..\GTSAM-install"
cd ..
