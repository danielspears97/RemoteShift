# Notices

RemoteShift is a modified version of **chiaki-ng** (https://github.com/streetpea/chiaki-ng),
which itself continues **Chiaki** by Florian Märkl and contributors.
Both are licensed under the GNU Affero General Public License v3.0 with the OpenSSL exception
(`LICENSES/AGPL-3.0-only-OpenSSL.txt`). RemoteShift is distributed under the same license; its
complete corresponding source code is available from the repository it is published in.

Changes made in RemoteShift (summary):
- `rsinput/`: new keyboard & mouse to DualSense engine, profile format and tests.
- `gui/`: input controller, Windows Raw Input, profile store, new QML pages, stream badge, update check,
  rebranding, portable mode, theme.
- Build and packaging: Windows CI workflow and Inno Setup installer for RemoteShift.

Third-party components added by RemoteShift:
- **nlohmann/json** 3.11.3, MIT License (`rsinput/third_party/nlohmann/LICENSE.MIT`).

Components inherited from chiaki-ng keep their own licenses (see `LICENSES/`, `third-party/`).

Trademarks: PlayStation, PS5 and DualSense are trademarks of Sony Interactive Entertainment Inc.
Grand Theft Auto is a trademark of Take-Two Interactive Software. RemoteShift is not affiliated with,
endorsed by, or sponsored by any of them, nor by OmniControl.
