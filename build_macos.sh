mkdir -p build
cd build
arch -arm64 python3 ../BuildMacOSUniversalBinary.py --distributor="Mario Party Netplay" --no-autoupdate --arm64_qt5_path="/opt/homebrew/opt/qt6" --build_type="Release"
