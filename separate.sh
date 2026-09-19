#!/bin/zsh
# Splits one wav into "<name> (Vocals).wav" and "<name> (Instrumental).wav".
#
#   separate.sh <input.wav> <output_dir>
#
# The model is pluggable. Defaults come from stem-model.conf next to this
# script; STEM_BACKEND / STEM_MODEL / STEM_DEVICE / STEM_VENVS in the
# environment override it for a single run, e.g.
#   STEM_BACKEND=demucs STEM_MODEL=htdemucs STEM_DEVICE=mps ./separate.sh song.wav out/
set -u

if [[ $# -ne 2 ]]; then
    echo "usage: separate.sh <input.wav> <output_dir>" >&2
    exit 2
fi
in="$1"
out="$2"
here="${0:A:h}"

# stem-model.conf is plain key=value lines, so it can simply be sourced.
[[ -f "$here/stem-model.conf" ]] && source "$here/stem-model.conf"
backend="${STEM_BACKEND:-${backend:-pymss}}"
model="${STEM_MODEL:-${model:-Kim_MelBandRoformer.ckpt}}"
device="${STEM_DEVICE:-${device:-mlx}}"
venvs="${STEM_VENVS:-${venvs:-$here/.venvs}}"
# Optional extra flags for the backend, e.g. params="--param overlap_size=176400"
params="${STEM_PARAMS:-${params:-}}"
extra=(${=params})

name="${${in:t}%.*}"
tmp="$(mktemp -d "${TMPDIR:-/tmp}/stem-XXXXXX")"
trap 'rm -rf "$tmp"' EXIT
mkdir -p "$out"

echo "separate.sh: backend=$backend model=$model device=$device"

case "$backend" in
    pymss)
        # MLX engine (fastest on Apple Silicon). device=mlx runs the whole model
        # in MLX fp16; device=mps is the plain PyTorch path.
        "$venvs/pymss/bin/pymss" infer "$model" --device "$device" \
            -i "$in" -o "$tmp" --format wav --wav-bit-depth PCM_16 "${extra[@]}" || exit 1
        ;;
    demucs)
        dev="$device"; [[ "$dev" == mlx ]] && dev=mps
        "$venvs/demucs/bin/demucs" -d "$dev" -n "$model" --two-stems vocals -o "$tmp" "${extra[@]}" "$in" || exit 1
        ;;
    audio-separator)
        "$venvs/audio-separator/bin/audio-separator" "$in" -m "$model" \
            --output_dir "$tmp" --output_format WAV "${extra[@]}" || exit 1
        ;;
    *)
        echo "separate.sh: unknown backend '$backend' (use pymss, demucs or audio-separator)" >&2
        exit 1
        ;;
esac

# Each backend names its stems differently (vocals/no_vocals, Vocals/Instrumental,
# vocals/other). Whatever mentions vocals is the vocal stem; the rest is the
# instrumental. "no_vocals" is matched before the generic "vocal" pattern.
voc=""; inst=""
for f in "$tmp"/**/*.wav(N); do
    case "${f:t:l}" in
        *no_vocals*|*instrumental*|*other*) inst="$f" ;;
        *vocal*)                            voc="$f" ;;
    esac
done

if [[ -z "$voc" || -z "$inst" ]]; then
    echo "separate.sh: could not find both stems in the model output:" >&2
    ls -R "$tmp" >&2
    exit 1
fi

mv "$voc"  "$out/$name (Vocals).wav"
mv "$inst" "$out/$name (Instrumental).wav"
echo "wrote: $out/$name (Vocals).wav"
echo "wrote: $out/$name (Instrumental).wav"
