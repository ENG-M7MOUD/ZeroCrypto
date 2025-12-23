# \# ZeroCrypto 🔐

# 

# ZeroCrypto is a secure, privacy-focused Windows application built on top of VeraCrypt,

# designed to manage encrypted vaults with enhanced usability, automation, and safety mechanisms.

# 

# It provides a modern GUI, strong operational security practices, and personal workflow automation

# for encrypted environments.

# 

# ---

# 

# \## ✨ Features

# 

# \- 🔒 \*\*VeraCrypt-based encrypted vault management\*\*

# \- 🧠 \*\*Automatic detection of mounted vaults\*\*

# \- ⚡ \*\*Async vault creation\*\* (no UI freezing, even for large containers)

# \- 🧨 \*\*Emergency Kill Switch\*\* (manual \& USB-triggered)

# \- 🔑 \*\*Secure password handling\*\* using in-memory secure buffers

# \- 📦 \*\*Vault Registry\*\* (persistent vault tracking)

# \- 📁 \*\*Autorun environment support\*\* after mount

# \- 📜 \*\*Encrypted system logs\*\*

# \- 🎨 \*\*Custom ImGui-based UI\*\*

# \- 🌐 \*\*Developer portfolio auto-open on mount/unmount\*\*

# 

# ---

# 

# \## 🖥️ Platform

# 

# \- OS: \*\*Windows 10 / 11\*\*

# \- Architecture: \*\*x64\*\*

# \- Dependencies:

# &nbsp; - VeraCrypt (bundled / portable)

# &nbsp; - DirectX 11

# &nbsp; - Win32 API

# 

# ---

# 

# \## 🧱 Architecture Overview

# 

# ZeroCrypto is built with a modular architecture to ensure:

# \- Maintainability

# \- Security isolation

# \- Future extensibility

# 

# Core logic is intentionally separated from UI rendering.

# 

# (See Architecture section below)

# 

# ---

# 

# \## 🔄 Vault Lifecycle

# 

# 1\. Vault is registered (manual selection or drag \& drop)

# 2\. User enters password (never stored as `std::string`)

# 3\. VeraCrypt is invoked securely

# 4\. Mount is detected via system drive scan

# 5\. Optional autorun environment starts

# 6\. Emergency logic monitors USB \& hotkeys

# 7\. On unmount, cleanup \& secure wipe (optional)

# 

# ---

# 

# \## 🧠 Security Design

# 

# \- Passwords stored only in `SecureBuffer`

# \- Memory wiped immediately after use

# \- No plaintext password persistence

# \- Panic mode can securely wipe:

# &nbsp; - config

# &nbsp; - vault registry

# &nbsp; - logs

# \- Uses OS-level process isolation

# 

# ---

# 

# \## ⏱️ Async Vault Creation (Large Containers)

# 

# For containers >20GB:

# \- VeraCrypt Format runs in a \*\*background thread\*\*

# \- UI remains responsive

# \- A modal indicates progress

# \- Final success/failure feedback is shown

# 

# This prevents the application from appearing frozen.

# 

# ---

# 

# \## 🔗 Developer

# 

# Portfolio opens automatically on:

# \- Successful mount

# \- Manual unmount

# 

# You can also open it via UI button.

# 

# 👉 https://eng-m7moud.github.io/protofolio/

# 

# ---

# 

# \## 🚀 Future Roadmap

# 

# \- Tray mode

# \- Multi-user profiles

# \- Linux support (experimental)

# \- Plugin system for autorun environments

# 

# ---

# 

# \## ⚠️ Disclaimer

# 

# ZeroCrypto is provided as-is.

# Use at your own risk.

# Always backup critical data.

# 

# ---

# 

# Made with ❤️ by Zero



