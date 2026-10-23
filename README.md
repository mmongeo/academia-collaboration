<!--
  © 2026 Intel Corporation

  This software and the related documents are Intel copyrighted materials, and
  your use of them is governed by the express license under which they were
  provided to you ("License"). Unless the License provides otherwise, you may
  not use, modify, copy, publish, distribute, disclose or transmit this software
  or the related documents without Intel's prior written permission.

  This software and the related documents are provided as is, with no express or
  implied warranties, other than those that are expressly stated in the License.
-->

# RV32I cores

## Files that you will be using

`modules/rv32-atomic/` (atomic core):

- `rv32-atomic.c`
- `txt_loader.py`
- `rv32-helpers.h`
- `rv32-core.h`
- `rv32-core.c`

`modules/rv32-pipeline/` (pipelined core):

- `rv32-pipeline.c`
- `txt_loader.py`
- `rv32-helpers.h`
- `rv32-core.h`
- `rv32-core.c`

Programs in .txt format: `targets/rv32i/programs/`. Their format is described in
[CONVENTIONS.md](CONVENTIONS.md).

## Compile

Copy `modules/` and `targets/` into your Simics project directory. If you do
not have a project yet, create one with `project-setup` from the Simics
installation:

```
<simics-installation>\bin\project-setup.bat <project-directory>
```

Then, from the project directory:

```
bin\make.bat rv32-atomic
bin\make.bat rv32-pipeline
```

After any change to `txt_loader.py` or to a `.c` or `.h` file, run
`bin\make.bat` again for that module, and restart Simics. Simics uses the
compiled module, not the files you edit.

On Linux, use `project-setup`, `bin/make` and `/` instead of
`project-setup.bat`, `bin\make.bat` and `\`.

## Run

From the project directory:

```
simics.bat targets\rv32i\rv32-atomic.simics program=targets/rv32i/programs/all-instructions.txt
simics.bat targets\rv32i\rv32-pipeline.simics program=targets/rv32i/programs/all-instructions.txt
```

On Linux, use `./simics` and `/` instead of `simics.bat` and `\`.

`program=` is the `.txt` program to load; without it, `all-instructions.txt`
is loaded. Then, at the `simics>` prompt:

```
run
pregs
quit
```

`run` runs the program until its `ebreak`.

`pregs` shows the registers.
