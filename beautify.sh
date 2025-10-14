#!/bin/bash

set -e
set -x

# Find certain .cpp and .h files and run clang-format on them
find ./apps/conference -type f \( -name "*.cpp" -o -name "*.h" \) -exec clang-format -i {} +

find . -type f \( -name "*.py" \) -exec python3 -m black {} +

echo "Formatting complete."
