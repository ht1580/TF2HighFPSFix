# Sources and licenses

## Ion / Northstar

This plugin derives its timing and input handling implementation from
[r2ion/IonLauncher](https://github.com/r2ion/IonLauncher), under the MIT license.
The original copyright notice is preserved in LICENSE and upstream/LICENSE.
This project is an independent transplant, not an official Ion release.

Reviewed commits, in order:

- 337d468e2af92414e9658ddf934f430835e181da
- 18114886fe7199e1e377dbcb23a10ec76794d583
- 0786036d9ceea428b8935b9b612bc0ab692bb957
- cdfccc734cb0b857f24e018aeefc190e9bc50afe

Patch snapshots and relevant source files are retained in upstream/ for review.

## MinHook

[TsudaKageyu/minhook](https://github.com/TsudaKageyu/minhook), v1.3.4,
commit c3fcafdc10146beb5919319d0683e44e3c30d537, is vendored in third_party/minhook.
Its BSD-style license and bundled disassembler notices are preserved in
third_party/minhook/LICENSE.txt and original source headers.

## Titanfall 2

No game modules, executables, assets, or credentials are distributed.
Version hashes and short function entry signatures identify the supported build.
Users must provide their own legitimate game installation and compatible loader.
