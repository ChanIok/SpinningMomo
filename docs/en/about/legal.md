# Legal & Privacy

Last updated: 2026-03-23  
Effective date: 2026-03-23

By downloading, installing, or using this software, you acknowledge that you have read and accepted this notice.

### 1. Project Nature

- This software is an open-source third-party desktop tool. The source code is made available under the GPL 3.0 license.
- This software has no affiliation, agency, or endorsement relationship with *Infinity Nikki* or its developers and publishers.

### 2. Data Handling (Primarily Local)

This software processes data primarily on your local device, which may include:
- **Configuration data**: e.g., target window title, game directory, output directory, feature toggles, and UI preferences (such as `settings.json`).
- **Runtime data**: Log and crash files (e.g., the `logs/` directory).
- **Feature data**: Local indexes and metadata (e.g., `database.db`, used for gallery and similar features).

By default, this project does not include an account system, does not bundle advertising SDKs, and does not send feature-processing data to project-maintainer-provided network interfaces unless you enable a specific online feature.

### 3. Network Activity

- When you explicitly trigger "Check for Updates / Download Update", the software will access the update source.
- If you enable "Automatically check for updates", the software will access the update source at startup.
- "Infinity Nikki photo metadata extraction" is an optional online feature. You can skip it, and not enabling it does not affect other core features.
- The software only contacts the related service when you enable this feature or manually start an extraction. Requests may include the UID, embedded photo parameters, and basic request information required for parsing; the full image file itself is not uploaded.
- After you enable this feature, it may automatically run in the background when new related photos are detected.
- When accessing the update source or the related service above, your request may be logged by the respective service provider (e.g., IP address, timestamp, User-Agent).

### 4. Data Sharing & User Feedback

- No local data is actively uploaded to developer servers by default, except for optional online features that you choose to enable.
- If you voluntarily submit an Issue, log file, crash report, or screenshot on a public platform, you agree to make that content public on that platform.

### 5. Risks & Disclaimer

- This software is provided "as is" without warranty of any kind, and without guarantee of error-free, uninterrupted, or fully compatible operation in all environments.
- You assume all risks associated with its use, including but not limited to performance degradation, compatibility issues, data loss, crashes, or other unexpected behavior.
- To the extent permitted by applicable law, the project maintainers shall not be liable for any indirect, incidental, or consequential damages arising from the use of or inability to use this software.

### 6. Permitted Use

You may only use this software for lawful and legitimate purposes. Use for illegal, harmful, or malicious activities is strictly prohibited.

### 7. Changes & Support

- This notice may be updated as the project evolves. Updated versions will be published in the repository or on the documentation site.
- Continued use of the software constitutes your acceptance of the updated notice.
- This project does not offer one-on-one customer support or guaranteed response times. If the repository has Issues enabled, you may submit feedback there.
