#!/usr/bin/env sh
set -eu

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
REPOS_DIR="$ROOT_DIR/repos"
GIT_NAME="${GIT_AUTHOR_NAME:-TangibleResearch}"
GIT_EMAIL="${GIT_AUTHOR_EMAIL:-tangibleresearch@users.noreply.github.com}"

copy_dir() {
  src="$1"
  dst="$2"
  mkdir -p "$(dirname "$dst")"
  rsync -a --delete \
    --exclude '.git' \
    --exclude '.venv' \
    --exclude 'node_modules' \
    --exclude 'target' \
    --exclude 'dist' \
    --exclude '__pycache__' \
    --exclude '*.pyc' \
    --exclude '*.aif' \
    --exclude '*.a2bc' \
    --exclude 'infravm' \
    "$src" "$dst"
}

write_mit() {
  repo="$1"
  cat > "$repo/LICENSE" <<'EOF'
MIT License

Copyright (c) 2026 TangibleResearch

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
EOF
}

write_readme() {
  repo="$1"
  title="$2"
  body="$3"
  {
    printf '# %s\n\n' "$title"
    printf '%s\n\n' "$body"
    printf '## License\n\nMIT\n'
  } > "$repo/README.md"
}

prepare_infraos() {
  repo="$REPOS_DIR/InfraOS"
  mkdir -p "$repo"
  copy_dir "$ROOT_DIR/infraos-backend/" "$repo/infraos-backend/"
  copy_dir "$ROOT_DIR/infraos-ui/" "$repo/infraos-ui/"
  copy_dir "$ROOT_DIR/shell/" "$repo/shell/"
  mkdir -p "$repo/docs"
  cp "$ROOT_DIR/docs/infraos-design.md" "$repo/docs/" 2>/dev/null || true
  write_readme "$repo" "InfraOS" "InfraOS is the web console and backend control plane for AInfra/InfraVM systems. It includes the dashboard, auth, IDE, Ops Assistant, object registry APIs, and developer shell."
  write_mit "$repo"
}

prepare_infravm() {
  repo="$REPOS_DIR/InfraVM"
  mkdir -p "$repo"
  copy_dir "$ROOT_DIR/infravm/" "$repo/infravm/"
  mkdir -p "$repo/optimizer"
  copy_dir "$ROOT_DIR/optimizer/c/" "$repo/optimizer/c/"
  mkdir -p "$repo/docs"
  cp "$ROOT_DIR/docs/pointrun.md" "$repo/docs/" 2>/dev/null || true
  write_readme "$repo" "InfraVM" "InfraVM is the C11 virtual machine/runtime for loading AIF files, resolving PointRun execution, and calling model connectors."
  write_mit "$repo"
}

prepare_aif() {
  repo="$REPOS_DIR/AIF"
  mkdir -p "$repo"
  copy_dir "$ROOT_DIR/ainfra-compiler/" "$repo/ainfra-compiler/"
  mkdir -p "$repo/docs" "$repo/examples"
  cp "$ROOT_DIR/docs/aif-format.md" "$repo/docs/" 2>/dev/null || true
  cp "$ROOT_DIR/docs/language.md" "$repo/docs/" 2>/dev/null || true
  cp "$ROOT_DIR/language.md" "$repo/" 2>/dev/null || true
  cp "$ROOT_DIR/examples/"*.ainfra "$repo/examples/" 2>/dev/null || true
  cp "$ROOT_DIR/examples/"*.a2 "$repo/examples/" 2>/dev/null || true
  write_readme "$repo" "AIF" "AIF is the compiled object format and language/compiler package for AInfra source. It contains the Rust lexer, parser, compiler, format docs, and examples."
  write_mit "$repo"
}

prepare_ainfra() {
  repo="$REPOS_DIR/AInfra"
  mkdir -p "$repo"
  copy_dir "$ROOT_DIR/ainfra-compiler/" "$repo/ainfra-compiler/"
  copy_dir "$ROOT_DIR/infravm/" "$repo/infravm/"
  copy_dir "$ROOT_DIR/infraos-backend/" "$repo/infraos-backend/"
  copy_dir "$ROOT_DIR/infraos-ui/" "$repo/infraos-ui/"
  copy_dir "$ROOT_DIR/optimizer/" "$repo/optimizer/"
  copy_dir "$ROOT_DIR/shell/" "$repo/shell/"
  copy_dir "$ROOT_DIR/docs/" "$repo/docs/"
  mkdir -p "$repo/examples"
  cp "$ROOT_DIR/examples/"*.ainfra "$repo/examples/" 2>/dev/null || true
  cp "$ROOT_DIR/examples/"*.a2 "$repo/examples/" 2>/dev/null || true
  cp "$ROOT_DIR/Makefile" "$repo/" 2>/dev/null || true
  cp "$ROOT_DIR/push.sh" "$repo/" 2>/dev/null || true
  cp "$ROOT_DIR/language.md" "$repo/" 2>/dev/null || true
  write_readme "$repo" "AInfra" "AInfra is the combined system: AInfra language/compiler, AIF object format, InfraVM runtime, InfraOS control plane, optimizer modules, examples, and developer tooling."
  write_mit "$repo"
}

generate() {
  rm -rf "$REPOS_DIR"
  mkdir -p "$REPOS_DIR"
  prepare_ainfra
  prepare_infravm
  prepare_infraos
  prepare_aif
  echo "generated repos in $REPOS_DIR"
}

publish_repo() {
  repo="$1"
  remote="$2"
  (
    cd "$repo"
    git init
    git checkout -B main
    git remote remove origin >/dev/null 2>&1 || true
    git remote add origin "$remote"
    git add -A
    if git diff --cached --quiet; then
      echo "$(basename "$repo"): no changes to commit"
    else
      git -c user.name="$GIT_NAME" -c user.email="$GIT_EMAIL" commit -m "first commit"
    fi
    git push -u origin main
  )
}

push_all() {
  generate
  publish_repo "$REPOS_DIR/AInfra" "https://github.com/TangibleResearch/AInfra.git"
  publish_repo "$REPOS_DIR/InfraVM" "https://github.com/TangibleResearch/InfraVM.git"
  publish_repo "$REPOS_DIR/InfraOS" "https://github.com/TangibleResearch/InfraOS.git"
  publish_repo "$REPOS_DIR/AIF" "https://github.com/TangibleResearch/AIF.git"
}

case "${1:-push}" in
  generate)
    generate
    ;;
  push)
    push_all
    ;;
  clean)
    rm -rf "$REPOS_DIR"
    echo "removed $REPOS_DIR"
    ;;
  *)
    echo "Usage: ./push.sh [generate|push|clean]"
    exit 2
    ;;
esac
