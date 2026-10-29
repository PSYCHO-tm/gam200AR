#!/usr/bin/env bash
# Builds and runs the interaction host test. Needs only g++ (C++17) and nlohmann/json.
#   Tests/run_host_tests.sh [path to Assets] [path to the folder that holds nlohmann/json.hpp]
set -euo pipefail
cd "$(dirname "$0")/.."                                   # Core/
ASSETS="${1:-../Assets}"
JSON_INC="${2:-}"
[ -n "$JSON_INC" ] && JSON_INC="-I$JSON_INC"
OUT="${TMPDIR:-/tmp}/test_interaction"
g++ -std=c++17 -Wall -Wextra -IHeader $JSON_INC \
    Tests/test_interaction.cpp \
    Source/Vector.cpp Source/Matrix.cpp Source/Raycast.cpp Source/Collider.cpp Source/Geometry.cpp \
    Source/objLoader.cpp Source/equipmentDef.cpp Source/picking.cpp Source/orderView.cpp \
    Source/InteractionSession.cpp Source/Simulation/*.cpp \
    -o "$OUT"
"$OUT" "$ASSETS"
