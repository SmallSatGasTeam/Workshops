# Setting Up Your F' Workshop Environment

Welcome! This guide walks you through everything you need to install before working with F' (F Prime), from zero to a cloned, ready-to-build repository. Follow the sections in order — each one builds on the last.

**What you'll set up:**
1. [Git](#1-install-git)
2. [A compiler toolchain](#2-install-a-compiler-toolchain)
3. [Python 3.10+, pip, and virtual environments](#3-install-python-pip-and-virtual-environments)
4. [The workshop repository](#4-clone-and-set-up-the-repository)

You'll also want to review the official [F' installation guide](https://fprime.jpl.nasa.gov/latest/docs/getting-started/installing-fprime/) alongside this doc, since it's kept up to date with the latest F' requirements.

---

## 1. Install Git

Pick the instructions for your platform.

### WSL

```
sudo add-apt-repository ppa:git-core/ppa
sudo apt update
sudo apt install git -y
git --version
```

### Linux

**Ubuntu / Debian**

```
sudo apt update
sudo apt install git -y
```

**Fedora / RHEL / CentOS**

```
sudo dnf install git -y
```

**Arch**

```
sudo pacman -S git
```

### macOS

1. Install Homebrew:

   ```
   /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
   ```

2. Install Git via Homebrew:

   ```
   brew install git
   ```

3. Verify:

   ```
   git --version
   ```

### Configure Git and SSH access

Once Git is installed, set up your identity so commits are properly attributed:

```
git config --global user.name "Your Real Name"
git config --global user.email "youremail@example.com"
```

Next, generate an SSH key pair:

```
ssh-keygen -t ed25519 -C "your_email@example.com"
```

This creates two files in `~/.ssh/`:

- `id_ed25519` — your **private** key. Never share this.
- `id_ed25519.pub` — your **public** key. This is what you share with Git.

Copy your public key to your clipboard:

```
cat ~/.ssh/id_ed25519.pub
```

Then add it to GitHub:

1. Go to [GitHub](https://github.com/) → **Settings** → **SSH and GPG keys** → **New SSH key**.
2. Paste the key and save.

Test the connection:

```
ssh -T git@github.com
```

> If you see a message like *"The authenticity of host 'github.com (...)' can't be established"* — this is normal. Type `yes` and press **Enter**. You should then see a success message confirming your GitHub username.

---

## 2. Install a compiler toolchain

**WSL & Ubuntu/Debian**

```
sudo apt update && sudo apt upgrade -y
sudo apt install build-essential -y
gcc --version
```

**macOS**

```
brew install gcc
gcc --version
```

**Fedora / RHEL / CentOS / Rocky Linux**

```
sudo dnf groupinstall "Development Tools"
```

**Arch**

```
sudo pacman -Syu gcc
```

**openSUSE**

```
sudo zypper install --type pattern devel_basis
```

---

## 3. Install Python, pip, and virtual environments

F' requires **Python 3.10+**. These instructions install Python 3.12.

**Ubuntu / Linux & WSL**

```
sudo apt update && sudo apt upgrade -y
sudo apt install python3.12 python3.12-venv python3-pip -y
python3.12 --version
pip3 --version
```

**macOS**

```
brew install python@3.12
python3.12 --version
pip3 --version
```

### Other build architectures

If your build host architecture isn't x86_64 or aarch64, or you have an older pip version, you'll also need Java:

```
sudo apt install git cmake default-jre python3 python3-pip python3-venv
```

Ubuntu and Debian users should see the notes on [Python installation](https://fprime.jpl.nasa.gov/latest/docs/getting-started/installing-fprime/#ubuntu-debian-java-and-python-pip) in the official F' docs.

### Working with virtual environments (venv)

A virtual environment keeps your Python packages isolated per-project, so they don't conflict with your system Python or other projects.

You'll create your venv in the next step, right after cloning the repo — but here's how it works:

Create one:

```
python3.12 -m venv .venv
```

Activate it:

```
source .venv/bin/activate
```

Once active, you'll see `(.venv)` appear at the start of your terminal prompt — any packages you install now are contained to this project.

Deactivate it when you're done:

```
deactivate
```

---

## 4. Clone and set up the repository

Now that Git, a compiler, and Python are installed, put it all together:

1. **Clone the repository:**

   ```
   git clone git@github.com:SmallSatGasTeam/Workshops.git
   cd Workshops
   ```

2. **Create and activate your virtual environment** (inside the cloned repo):

   ```
   python3.12 -m venv .venv
   source .venv/bin/activate
   ```

3. **Fetch the F' submodule:**

   ```
   git submodule update --init --recursive
   ```

4. **Install F' dependencies.** Replace `<project>` below with the name of the specific workshop project directory you're working in (for example, `FSW`):

   ```
   pip install -r <project>/fprime/requirements.txt
   ```

You're all set! 🎉 From here, continue to `running-workshops.md` found in the same directory this file is in.
