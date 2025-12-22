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
cmake -G "Visual Studio 17 2022" -A x64 -B build -S . -DGTSAM_INSTALL_GEOGRAPHICLIB=ON -DGTSAM_BUILD_EXAMPLES_ALWAYS=OFF -DGTSAM_BUILD_TESTS=OFF -DGTSAM_WITH_TBB=ON -DTBB_DIR="../oneTBB-install/lib/cmake/TBB"
cmake --build build --config Release && cmake --install build --config Release --prefix "..\GTSAM-install"
cmake --build build --config Debug && cmake --install build --config Debug --prefix "..\GTSAM-install"
cd ..

rmdir /s /q KDDockWidgets-install
cd KDDockWidgets
rmdir /s /q KDDockWidgets-build
cmake -G "Visual Studio 17 2022" -A x64 -B "..\KDDockWidgets-build" -S . -DKDDockWidgets_QT6=ON -DKDDockWidgets_EXAMPLES=OFF
cmake --build "..\KDDockWidgets-build" --config Release && cmake --install "..\KDDockWidgets-build" --config Release --prefix "..\KDDockWidgets-install"
cmake --build "..\KDDockWidgets-build" --config Debug && cmake --install "..\KDDockWidgets-build" --config Debug --prefix "..\KDDockWidgets-install"
cd ..
