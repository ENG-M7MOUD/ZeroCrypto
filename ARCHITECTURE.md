┌──────────────────────────┐

│        WinMain           │

│  (Application Bootstrap) │

└────────────┬─────────────┘

&nbsp;            │

&nbsp;            ▼

┌──────────────────────────┐

│         UI Layer         │

│        (ImGui)           │

│                          │

│  - Vault List            │

│  - Password Input        │

│  - Kill Switch           │

│  - Logs Viewer           │

│  - Settings              │

└────────────┬─────────────┘

&nbsp;            │

&nbsp;            ▼

┌──────────────────────────┐

│     Application Logic    │

│                          │

│  - Mount / Unmount       │

│  - Async Vault Creation  │

│  - Autorun Handling      │

│  - Event Dispatch        │

└────────────┬─────────────┘

&nbsp;            │

&nbsp;            ▼

┌──────────────────────────┐

│     Core Modules         │

│                          │

│  VaultRegistry           │

│   - vaults.dat           │

│                          │

│  SecureBuffer            │

│   - password memory      │

│                          │

│  Crypto                  │

│   - encrypted logs       │

│                          │

│  SystemUtils             │

│   - process execution    │

│   - secure delete        │

└────────────┬─────────────┘

&nbsp;            │

&nbsp;            ▼

┌──────────────────────────┐

│     External Tools       │

│                          │

│  VeraCrypt.exe           │

│  VeraCrypt Format.exe    │

│                          │

│  Windows API             │

│  DirectX 11              │

└──────────────────────────┘



