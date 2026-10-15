#!/usr/bin/env bash
set -euo pipefail
project_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
qt_env="${QT_ENV_FILE:-$HOME/Qt/qt6-8.3-env.sh}"
if [[ ! -f "$qt_env" ]]; then
    printf '找不到 Qt 环境脚本：%s\n' "$qt_env" >&2
    exit 2
fi
source "$qt_env"
export QT_QPA_PLATFORM=xcb
export QSG_RHI_BACKEND=opengl
export QSG_INFO=1
if [[ -z "${DISPLAY:-}" ]]; then
    printf '请在图形桌面终端运行，或设置 DISPLAY 和 XAUTHORITY。\n' >&2
    exit 2
fi
cmake -S "$project_dir" -B "$project_dir/build/kylin" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$QT_ROOT" \
    -DCALIBURN_BUILD_EXAMPLE=ON -DCALIBURN_BUILD_TESTS=OFF
cmake --build "$project_dir/build/kylin" --target CaliburnMultiScreenApp --parallel 2
exec "$project_dir/build/kylin/bin/CaliburnMultiScreenApp" "$@"
