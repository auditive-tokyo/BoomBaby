# Compatibility

## Platform Requirements

| | |
| --- | --- |
| **OS** | macOS only (Windows / Linux not supported at this time) |
| **Architecture** | Apple Silicon (arm64) — Intel Macs are not supported |
| **Formats** | AUv2, VST3, Standalone |

> The distributed binary is arm64-only and is not a universal build.  
> Intel Macs cannot run arm64 binaries — there is no workaround.  
> Note: if you are on Apple Silicon but running an Intel-only DAW under Rosetta 2,
> the plugin must also be an Intel (x86_64) build, which is currently not provided.

---

Tested environments are listed below. To add your own, see [Contributing](#contributing).

## Tested Configurations

| DAW                  | OS                   | CPU          | Format     | Status      |
| -------------------- | -------------------- | ------------ | ---------- | ----------- |
| Ableton Live 12      | macOS 15.3.1 (24D70) | Apple M4 Pro | AUv2, VST3 | ✅ Works    |
| Logic Pro            | —                    | —            | AUv2       | —           |
| Cubase               | —                    | —            | VST3       | —           |
| Bitwig Studio        | —                    | —            | VST3       | —           |
| Reaper               | —                    | —            | AUv2, VST3 | —           |
| Studio One           | —                    | —            | AUv2, VST3 | —           |

## Contributing

If you have tested BoomBaby on a configuration not listed above, please share your results — either way works:

- **Quick report**: Post in [GitHub Discussions](https://github.com/auditive-tokyo/BoomBaby/discussions) — no fork needed, just describe your environment and whether it worked.
- **Add to the table**: Open a pull request following the steps below.

### Pull Request

To add a tested configuration:

1. Fork this repository.
2. Edit `docs/compatibility.md` and append your row to the table above.
3. Open a pull request with the title `compat: <OS> + <DAW>`.

Please include:

- **DAW**: name and version (e.g. `Ableton Live 12`, `Logic Pro 11.1`)
- **OS**: name, version, and build number (e.g. `macOS 15.3.1 (24D70)`)
- **CPU**: architecture and model (e.g. `Apple M4 Pro`, `Intel Core i9`)
- **Format**: `AUv2`, `VST3`, `Standalone`, or combinations (e.g. `AUv2, VST3`)
- **Status**: `✅ Works`, `⚠️ Partial` (with notes), or `❌ Broken` (with notes)
