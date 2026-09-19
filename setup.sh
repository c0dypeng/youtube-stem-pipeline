#!/bin/zsh
# One-click setup for song-downloader on a Mac (Apple Silicon).
#
#   1. Open Terminal (press Cmd+Space, type "Terminal", press Enter)
#   2. Type:  cd   (with a space after it), drag this folder into the window, press Enter
#   3. Type:  ./setup.sh   and press Enter
#
# It installs everything: Xcode command line tools, Homebrew, yt-dlp, ffmpeg,
# Python, the stem-separation engines, builds the downloader and downloads the
# model named in stem-model.conf. Safe to run again at any time: it skips
# whatever is already there.
set -u

here="${0:A:h}"
cd "$here"

step()  { print -P "\n%F{cyan}==> $1%f"; }
ok()    { print -P "%F{green}    ✓ $1%f"; }
fail()  { print -P "%F{red}    ✗ $1%f"; exit 1; }

if [[ "$(uname -m)" != "arm64" ]]; then
    fail "This needs an Apple Silicon Mac (M1 or newer). Intel Macs can't run the MLX engine."
fi

# ---------------------------------------------------------------- 1. Xcode CLT
step "Xcode command line tools (compiler + git)"
if xcode-select -p > /dev/null 2>&1; then
    ok "already installed"
else
    xcode-select --install 2>/dev/null
    echo "    A macOS window just popped up. Click Install, wait for it to finish,"
    echo "    then run ./setup.sh again."
    exit 1
fi

# ---------------------------------------------------------------- 2. Homebrew
step "Homebrew (Mac package manager)"
if [[ -x /opt/homebrew/bin/brew ]]; then
    ok "already installed"
else
    /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)" || fail "Homebrew install failed"
fi
eval "$(/opt/homebrew/bin/brew shellenv)"
# Make brew available in future Terminal windows too.
if ! grep -q 'brew shellenv' "$HOME/.zprofile" 2>/dev/null; then
    echo 'eval "$(/opt/homebrew/bin/brew shellenv)"' >> "$HOME/.zprofile"
fi

# ---------------------------------------------------------------- 3. brew packages
step "yt-dlp, ffmpeg and Python 3.12"
for pkg in yt-dlp ffmpeg python@3.12; do
    if brew list --versions "$pkg" > /dev/null 2>&1; then
        ok "$pkg already installed"
    else
        brew install "$pkg" || fail "could not install $pkg"
    fi
done
brew upgrade yt-dlp > /dev/null 2>&1 || true   # YouTube changes often; start fresh
py="/opt/homebrew/opt/python@3.12/bin/python3.12"
[[ -x "$py" ]] || fail "python3.12 not found at $py"

# ---------------------------------------------------------------- 4. Python engines
# separate.sh looks for the engines under $venvs (from stem-model.conf).
venvs=""
[[ -f stem-model.conf ]] && source stem-model.conf
venvs="${venvs:-$here/.venvs}"
mkdir -p "$venvs"

step "Separation engine: pymss (MLX, Apple GPU) -> $venvs/pymss"
if [[ -x "$venvs/pymss/bin/pymss" ]]; then
    ok "already installed"
else
    "$py" -m venv "$venvs/pymss" || fail "could not create venv"
    "$venvs/pymss/bin/python" -m pip install -q --upgrade pip
    "$venvs/pymss/bin/python" -m pip install -q pymss || fail "pip install pymss failed"
    ok "installed"
fi

step "Separation engine: demucs (backup option) -> $venvs/demucs"
if [[ -x "$venvs/demucs/bin/demucs" ]]; then
    ok "already installed"
else
    "$py" -m venv "$venvs/demucs" || fail "could not create venv"
    "$venvs/demucs/bin/python" -m pip install -q --upgrade pip
    "$venvs/demucs/bin/python" -m pip install -q demucs || fail "pip install demucs failed"
    ok "installed"
fi

# ---------------------------------------------------------------- 5. build
step "Building the downloader"
make > /dev/null || fail "build failed (see errors above)"
chmod +x separate.sh
ok "built ./downloader"

# ---------------------------------------------------------------- 6. model
backend="${backend:-pymss}"
model="${model:-Kim_MelBandRoformer.ckpt}"
step "Downloading the separation model: $model ($backend)"
case "$backend" in
    pymss)
        "$venvs/pymss/bin/pymss" download "$model" || fail "model download failed"
        ;;
    demucs)
        # demucs fetches weights on first use, so run it once on a second of silence.
        tmp="$(mktemp -d)"
        ffmpeg -loglevel error -f lavfi -i anullsrc=r=44100:cl=stereo -t 1 "$tmp/silence.wav"
        "$venvs/demucs/bin/demucs" -d cpu -n "$model" -o "$tmp" "$tmp/silence.wav" > /dev/null 2>&1 || fail "model download failed"
        rm -rf "$tmp"
        ;;
    *)
        echo "    backend '$backend' is not handled by setup.sh; the model will download on first use."
        ;;
esac
ok "model ready"

# ---------------------------------------------------------------- done
print -P "\n%F{green}All set.%f To use it, in this folder run:"
echo "    ./downloader                 (menu: audio / video / thumbnail / separate)"
echo "    ./downloader separate 'https://www.youtube.com/watch?v=...'"
echo "Separated songs land in downloads/separate/<song name>/ as three .wav files."
echo "To change the separation model, edit stem-model.conf."
