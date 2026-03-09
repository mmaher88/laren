# Laren

Arabizi-to-Arabic transliteration engine and Fcitx5 input method for Linux.
A replacement for [Microsoft Maren](https://learn.microsoft.com/en-us/archive/blogs/robmar/microsoft-maren-type-arabic-in-roman-characters-on-the-fly).

Type in Latin characters (Arabizi) and get Arabic script suggestions system-wide — in any application.

```
ezayak  →  إزيك
7abibi  →  حبيبي
3ala    →  على
salam   →  سلام
```

[**Try the interactive demo**](https://mmaher88.github.io/laren/) · [**Wiki**](https://github.com/mmaher88/laren/wiki)

## Installation

### Arch Linux (AUR)

```bash
yay -S fcitx5-laren
```

### Fedora

```bash
sudo dnf config-manager addrepo --from-repofile=https://download.opensuse.org/repositories/home:/mmaher88:/laren/Fedora_$(rpm -E %fedora)/home:mmaher88:laren.repo
sudo dnf install fcitx5-laren
```

### Ubuntu / Debian

```bash
# Add the repository key and source
DISTRO="xUbuntu_$(lsb_release -rs)"  # or Debian_12
curl -fsSL "https://download.opensuse.org/repositories/home:/mmaher88:/laren/${DISTRO}/Release.key" | sudo gpg --dearmor -o /etc/apt/keyrings/laren.gpg
echo "deb [signed-by=/etc/apt/keyrings/laren.gpg] https://download.opensuse.org/repositories/home:/mmaher88:/laren/${DISTRO}/ /" | sudo tee /etc/apt/sources.list.d/laren.list
sudo apt update && sudo apt install fcitx5-laren
```

### openSUSE Tumbleweed

```bash
sudo zypper ar https://download.opensuse.org/repositories/home:/mmaher88:/laren/openSUSE_Tumbleweed/home:mmaher88:laren.repo
sudo zypper refresh && sudo zypper install fcitx5-laren
```

### From source

**Ubuntu / Debian:**
```bash
sudo apt install cmake g++ fcitx5-modules-dev libfcitx5core-dev
cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j$(nproc)
sudo cmake --install build
```

**Fedora:**
```bash
sudo dnf install cmake gcc-c++ fcitx5-devel
cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j$(nproc)
sudo cmake --install build
```

**Arch:**
```bash
sudo pacman -S cmake fcitx5
cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j$(nproc)
sudo cmake --install build
```

## Setup

After installing, enable the input method:

1. Open **Fcitx5 Configuration** (or run `fcitx5-configtool`)
2. Add **Laren** to your input method list
3. Use your Fcitx5 trigger key (default `Ctrl+Space`) to switch to Laren
4. Start typing in Arabizi — candidates appear as you type
5. Press `Space` or `Enter` to commit the top candidate, or a number key to pick one

## Arabizi Mappings

| Input | Arabic | Input | Arabic |
|-------|--------|-------|--------|
| `2`   | ء / ق  | `sh`  | ش      |
| `3`   | ع      | `kh`  | خ      |
| `5`   | خ      | `gh`  | غ      |
| `7`   | ح      | `th`  | ث / ذ  |
| `8`   | ق      | `dh`  | ذ / ض  |
| `9`   | ص      | `ch`  | تش     |

Short vowels (a, i, u, e, o) are treated as optional to match Arabic's abjad writing system.
See the [wiki](https://github.com/mmaher88/laren/wiki) for the full mapping reference.

## How it works

Laren uses a rule-based pipeline — no neural models, fully predictable and debuggable:

1. **Normalize** — lowercase, collapse repeats, strip whitespace
2. **Expand** — apply Arabizi rules to generate candidate Arabic forms
3. **Filter** — look up candidates in a 375k-word Arabic dictionary (trie)
4. **Rank** — score by word frequency, exact match, and recency
5. **Present** — show top candidates for selection

See [ARCHITECTURE.md](ARCHITECTURE.md) for the full design.

## Building and testing

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
cd build && ctest --output-on-failure
```

## License

[GPL-3.0-or-later](LICENSE)
