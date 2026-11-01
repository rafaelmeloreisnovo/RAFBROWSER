# RAFBROWSER source lineage

- Snapshot source: `rafaelmeloreisnovo/BrowserRaf@585d1b2abd0622b886e12f306b27882ac6f07eb4`
- Import date: 2026-11-01
- Policy: byte-for-byte copy of runtime, test, workflow, tool, and license files; no runtime rewrites in this import.
- Independent destination: `rafaelmeloreisnovo/RAFBROWSER`
- Reproducibility: each row's Git blob SHA is the source file blob at the pinned source commit. Destination can be checked with `git hash-object <path>`.
- Update rule: future changes are explicit successor imports; the two repositories do not auto-sync.
- Product claim: implementation snapshot only. Trusted HTTPS, physical Android execution, and production readiness are not established.

| Source path | Source Git blob SHA |
|---|---|
| `.github/workflows/freestanding-core.yml` | `2385645fa3bff43d313fdb5123a0550d6975220c` |
| `.github/workflows/rafascan-offline.yml` | `78ed70d174a9a6392f41024162d4f8a0ad5dc0e7` |
| `LICENSE` | `f0433f024df053a0e11a5b8eac35a9d73da99b70` |
| `internal/Makefile` | `d97022b5557eb17a3417971cae627dbf1ad35330` |
| `internal/br_dns.h` | `d9275e0e6cfa37dabed557b2380b5077200ecead` |
| `internal/br_entropy.h` | `a2eeef90893aace96cdaf332207bd1fb5fb49839` |
| `internal/br_html.h` | `b8a52053a019d1116938058df4913f5ea62bc2e2` |
| `internal/br_http.h` | `e70db0f23e1b5dda2f0a77d58c8169d45399a2f8` |
| `internal/br_links.h` | `8e90105f01315b21cf81506b52dadd9acdd474f1` |
| `internal/br_main.c` | `306eeb2f70b60a102c144f67eda46bace60cf8e8` |
| `internal/br_pixel.h` | `e8f64b92a623b7da63a33ca866692dc61f8b23a9` |
| `internal/br_start.S` | `dee15ce56700b4dca8a4398ac7198ae96e848d4a` |
| `internal/br_sys.h` | `5c5153e9ecd3760eaf71d133e03db1e92ec76455` |
| `internal/br_tls.h` | `67b67b95349ef0e413e1c4d1e3fa06214592a17e` |
| `internal/br_types.h` | `317a5e9edde3eaa0aa55754f0424c3ad4b69f12a` |
| `tests/check_cross_abi_objects.sh` | `4b34c5263bbe6c6f857c1bf1ac47381fde636222` |
| `tests/check_host_freestanding.sh` | `f4f8bc8c38d89a532e7629744093bebb94681a1a` |
| `tests/check_negative_fixtures.sh` | `3c73d9e761ae856e69a489218f024ffb68c0ac50` |
| `tests/check_pixel_matrix.sh` | `e7e3cbaa07316c191bdb2ac6f8ed30f177741278` |
| `tests/check_pixel_vectors.sh` | `3acf4f66bfdbb52d96e354e830ee9debc0d05f7a` |
| `tests/check_qemu_abis.sh` | `6d47435a5de4e56997e914a0a986fd85f2a1f6d0` |
| `tests/check_termux_device.sh` | `fb8eb96f5c08a83404a1bdfdfdcdcf9c9ebaafa2` |
| `tests/loopback_http.py` | `bcf15744c9e9ca98e8d5ef8d1ef3ca61abae018f` |
| `tests/negative_fixtures.c` | `7727057b276b1d87f25f9fd92d695f957275e96b` |
| `tests/pixel_freestanding_probe.c` | `8cd49bf5dfcd98589f8141eef7dace4b8f563c15` |
| `tests/pixel_vectors.c` | `05cbda515f4510904efa4ee2281b15527a48e5f8` |
| `tests/test_division.c` | `8b48c1d5c233af33051552adfd553a742ddaefdd` |
| `tests/test_pixel_matrix.c` | `314d9e8335b30f1f6ff712c26dd05d654756f7ea` |
| `tests/test_rafascan_network.sh` | `8433bc28b2da70e559617d76d32cfb727bae69e7` |
| `tools/rafascan_network.sh` | `64cfc3badee29a310ae4c781a07e56c2488a86cd` |


## Destination-only build integration

The root `Makefile`, `.gitignore`, README, this lineage file, and `.github/workflows/makefile-smoke.yml` were authored for RAFBROWSER. The two imported upstream workflows and the 30 imported code/test/tool/license files remain byte-for-byte copies of the pinned source snapshot. The additional workflow validates `make` and the static ELF boundary.
