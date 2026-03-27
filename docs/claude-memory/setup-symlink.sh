#!/bin/bash
# ============================================================
# Claude Code Memory 软链接初始化脚本（子模块版）
#
# 作用：将 Claude Code 的 memory 目录指向本子模块中的
#       docs/claude-memory/，使 memory 随子模块一起同步。
#
# 使用方法：
#   1. git clone / git pull 本仓库
#   2. cd 到子模块根目录（包含 docs/claude-memory/ 的那一层）
#   3. bash docs/claude-memory/setup-symlink.sh
#
# 执行后效果：
#   ~/.claude/projects/<编码路径>/memory
#   → 软链接到 <子模块根目录>/docs/claude-memory/
#   Claude Code 在子模块目录下读写 memory 时自动同步到 git 追踪的目录。
# ============================================================

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# 子模块根目录：docs/claude-memory/ 的上一级（即 docs/ 的上一级）
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
MEMORY_SRC="$SCRIPT_DIR"

# 将路径转换为 Claude Code 的编码格式（/ _ . 全替换为 -）
ENCODED=$(echo "$PROJECT_ROOT" | sed 's|/|-|g; s|_|-|g; s|\.|-|g')
CLAUDE_MEMORY_DIR="$HOME/.claude/projects/${ENCODED}/memory"

echo "子模块路径：$PROJECT_ROOT"
echo "Claude memory 目标路径：$CLAUDE_MEMORY_DIR"
echo ""

# ── 已经是正确软链接则退出 ────────────────────────────────
if [ -L "$CLAUDE_MEMORY_DIR" ] && [ "$(readlink "$CLAUDE_MEMORY_DIR")" = "$MEMORY_SRC" ]; then
    echo "✅ 软链接已存在且正确，无需操作。"
    exit 0
fi

MEMORY_MD_EXISTS=false
[ -f "$MEMORY_SRC/MEMORY.md" ] && MEMORY_MD_EXISTS=true

if [ -d "$CLAUDE_MEMORY_DIR" ] && [ ! -L "$CLAUDE_MEMORY_DIR" ]; then
    if [ "$MEMORY_MD_EXISTS" = false ]; then
        echo "📋 场景 A：本机已有 Claude memory，docs/claude-memory/ 尚无内容"
        echo "   → 将本机 memory 拷贝到 docs/claude-memory/ ..."
        cp "$CLAUDE_MEMORY_DIR"/* "$MEMORY_SRC/" 2>/dev/null || echo "   ⚠️  本机 memory 目录为空"
    else
        echo "📋 场景 B：docs/claude-memory/ 已有内容（来自 git）"
        echo "   → 备份本机旧 memory，改用 git 版本"
    fi

    BACKUP="${CLAUDE_MEMORY_DIR}.backup.$(date +%Y%m%d_%H%M%S)"
    echo "⚠️  备份本机旧 memory 到：$BACKUP"
    mv "$CLAUDE_MEMORY_DIR" "$BACKUP"
elif [ "$MEMORY_MD_EXISTS" = false ]; then
    echo "ℹ️  首次使用，Claude Code 将在首次打开子模块目录时生成 MEMORY.md"
fi

mkdir -p "$(dirname "$CLAUDE_MEMORY_DIR")"

if [ -L "$CLAUDE_MEMORY_DIR" ]; then
    rm "$CLAUDE_MEMORY_DIR"
fi

ln -s "$MEMORY_SRC" "$CLAUDE_MEMORY_DIR"

echo ""
echo "✅ 软链接创建成功："
ls -la "$CLAUDE_MEMORY_DIR"
echo ""
echo "后续操作："
echo "  - Claude Code 在此子模块目录下读写 memory 将自动写入 docs/claude-memory/"
echo "  - 定期执行 git add docs/claude-memory/ && git commit 即可同步"
