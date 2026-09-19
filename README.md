# youtube-stem-pipeline

> It's mainly for myself to download stuff and source-separate faster, but I'd like to share it with people.

Paste a YouTube link, get the song as a `.wav` **plus a vocals stem and an
instrumental stem**, separated locally on your Mac's GPU in about a minute.
No web service, no upload, no Logic Pro. The separation model is a one-line
setting, so you can swap in any of the ~300 open models the engine supports.

Works on Apple Silicon Macs (M1 or newer). Tested on a MacBook Pro M3, 16 GB.

## What you get

```
downloads/separate/<song name>/
├── <song name>.wav                 the original mix, 44.1 kHz stereo
├── <song name> (Vocals).wav
└── <song name> (Instrumental).wav
```

The same tool also does plain audio, video and thumbnail downloads:

| mode | output | folder |
|---|---|---|
| `audio` | `.wav` | `downloads/audio/` |
| `video` | `.mov` | `downloads/video/` |
| `thumbnail` | `.jpg` | `downloads/thumbnail/` |
| `separate` | `.wav` + vocals + instrumental | `downloads/separate/<song>/` |

## Install (one command)

1. Open **Terminal** (`Cmd + Space`, type `Terminal`, Enter).
2. Type `cd ` (with a space), drag this folder into the window, press Enter.
3. Run:

   ```
   ./setup.sh
   ```

   It installs Homebrew, `yt-dlp`, `ffmpeg`, Python 3.12, the separation
   engines ([pymss](https://github.com/pymss-project/pymss) and
   [demucs](https://github.com/adefossez/demucs)) into `.venvs/`, builds the
   downloader, and fetches the default model (about 200 MB to 1 GB depending
   on the model). Safe to run again; it skips whatever is already installed.
   If macOS pops up a "command line developer tools" dialog, click Install
   and run `./setup.sh` again afterwards.

## Use

```
./downloader
```

Pick a number, paste the link, done. Or skip the menu:

```
./downloader separate 'https://www.youtube.com/watch?v=XXXXXXXXXXX'
./downloader audio    'https://www.youtube.com/watch?v=XXXXXXXXXXX'
```

YouTube changes constantly, so `yt-dlp` has to keep up. The downloader checks
`yt-dlp`'s age once a day and updates it through Homebrew when it is older than
a month. If a download fails anyway, it updates `yt-dlp` and retries once
before giving up.

## Changing the separation model

Open `stem-model.conf` in any text editor. Three lines matter:

```
backend=pymss
model=BS-Roformer-Resurrection.ckpt
device=mlx
```

- `backend` picks the engine: `pymss` (MLX, fastest on Apple GPUs, ~300 models),
  `demucs` (Meta's Demucs models) or `audio-separator`
  ([python-audio-separator](https://github.com/nomadkaraoke/python-audio-separator),
  optional, not installed by `setup.sh`).
- `model` is the model name as the engine knows it. For `pymss`, list them with
  `.venvs/pymss/bin/pymss list`; for `demucs` use `htdemucs`, `htdemucs_ft` or
  `htdemucs_6s`.
- `device` is `mlx` or `mps` for pymss, `mps` or `cpu` for the others.

Save the file and run the downloader again. A model you have not used before
is downloaded on first use. To try a model for a single run without editing
the file, set the same names as environment variables:

```
STEM_MODEL=Kim_MelBandRoformer.ckpt ./downloader separate 'https://...'
```

Models that are not in the pymss catalog (for example new releases on Hugging
Face) can be added with their checkpoint and config:

```
.venvs/pymss/bin/pymss register my_model --type bs_roformer --model path/to/model.ckpt --config path/to/config.yaml
```

and then used with `model=my_model`. The optional `params=` line in the config
passes extra flags to the engine, e.g. a larger `overlap_size` for slightly
cleaner stems at the cost of time.

### Which model?

Measured on a MacBook Pro M3 (16 GB) with a 4-minute stereo song, whole run
including model load. Quality is the vocals SDR on the
[MVSep multisong leaderboard](https://mvsep.com/quality_checker/multisong_leaderboard?sort=vocals),
where Logic Pro's built-in Stem Splitter scores 11.36 for reference.

| backend | model | device | time | quality | note |
|---|---|---|---|---|---|
| pymss | `BS-Roformer-Resurrection.ckpt` | mlx | 80 s | 11.34 | **default**, ties Logic Pro |
| pymss | `Kim_MelBandRoformer.ckpt` | mlx | 63 s | 10.98 | fastest RoFormer-class option |
| pymss | `kimmel_unwa_ft2.ckpt` | mlx | ~63 s | – | Kim fine-tuned by unwa, same speed |
| pymss | `bs_roformer_voc_hyperacev2.ckpt` | mlx | ~5.5 min | 11.40 | too slow here |
| pymss | `model_bs_roformer_ep_317_sdr_12.9755.ckpt` | mlx | ~3.5 min | 10.87 | the classic UVR model, too slow |
| demucs | `htdemucs` | mps | 30 s | ~8.8 | fast, clearly worse |
| demucs | `htdemucs_ft` | mps | 114 s | ~8.8 | |

Not in the table: models that beat Logic Pro on the leaderboard (unwa's BS-Roformer-Leap-Xe at 11.76)
run at 0.3–0.5x realtime on an M3, i.e. 8–12 minutes per song. On a faster Mac they
may be worth registering (see above).

## How it works

1. `downloader` (a small C++ program) calls `yt-dlp` to fetch the audio and
   convert it to a 44.1 kHz stereo `.wav` with `ffmpeg`.
2. It creates `downloads/separate/<song name>/`, moves the `.wav` there and
   runs `separate.sh` on it.
3. `separate.sh` reads `stem-model.conf`, runs the chosen engine, and renames
   the two stems to `<song name> (Vocals).wav` and `<song name> (Instrumental).wav`.

Everything runs on your machine. The only network access is YouTube (via
`yt-dlp`) and the one-time model download.

## Build from source

```
make            # needs the Xcode command line tools
```

`setup.sh` does this for you.

## Credits

- [yt-dlp](https://github.com/yt-dlp/yt-dlp) and [ffmpeg](https://ffmpeg.org/) for downloading and converting.
- [pymss](https://github.com/pymss-project/pymss) for the MLX inference engine, and
  [Music-Source-Separation-Training](https://github.com/ZFTurbo/Music-Source-Separation-Training)
  by ZFTurbo, which most of these models were trained with.
- Model authors: [unwa](https://huggingface.co/pcunwa) (Resurrection, HyperACE, Leap),
  [Kimberley Jensen](https://huggingface.co/KimberleyJSN/melbandroformer) (Mel-Band RoFormer),
  viperx (BS-RoFormer ep_317), Meta ([demucs](https://github.com/adefossez/demucs)).
  Models are downloaded from their authors at runtime and keep their own licenses.
- [MVSep](https://mvsep.com/) for the quality leaderboard.

## License

MIT for the code in this repository. See `LICENSE`.
