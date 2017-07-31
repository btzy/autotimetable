# Autotimetable

Autotimetable is an automatic timetable generator library written in C++ for National University of Singapore (NUS) modules.  It uses an optimized non-probabilistic recursive backtracking algorithm to find the best timetable amongst all possible combinations, hence, it will always generate the *best* timetable possible (of course, this is limited to the evaluation criteria available to Autotimetable (see what it can do below)).

The library is in `autotimetable.cpp` (and the accompanying header file, `autotimetable.hpp`).

Under normal use, the speed of the generation engine is usually less than 5 ms, and extremely likely to be less than 50 ms (counting the time in the Autotimetable engine only, not the loading of modules from file).

To make usage more convenient, a `main.cpp` file is provided to allow the library to be compiled as a command-line program, and read module data in NUSMods format.

For now, the command-line interface is the only way to use this library, but I envision compiling the engine to a WebAssembly to be used with a web-based UI in the future.

Note: Autotimetable will **not** detect examination schedule clashes.  Please check the examination schedule from somewhere else (e.g. NUSMods).

## Command-line usage

`autotimetable --modulefile=<filename> --required=<comma-separated module list>`

### Example

`autotimetable --modulefile=modules.json --required=CS1010,MA1101R,CS1231,BN1101,GET1002`

Do not put any spaces in the module list!

### Required options

`--modulefile=<filename>` - Sets the JSON file to use to obtain the module data.  This file should follow the NUSMods API format.  This option is processed by `main.cpp` before invoking the Autotimetable engine.

`--required=<comma-separated module list>` - Selects the modules to pass to the Autotimetable engine, e.g. `CS1010,MA1101R,CS1231,BN1101,GET1002`.  There should be no spaces in the comma-separated module list.

### Other options

`--fixed=<comma-separated selection list>` - Selects the lessons to fix.  This option may be useful when certain modules are pre-allocated or you want a certain lesson at a fixed timeslot.  Each selection should be in the form `<module code>:<item kind>:<lesson number>`, e.g. `CS1010:Sectional_Teaching:2`.  If there are multiple selections, separate them with a single comma.  There should be no spaces in the comma-separated selection list; if the item kind contains spaces, they should be replaced by underscores or hyphens as in the example in this paragraph.  This option is processed by `main.cpp` before invoking the Autotimetable engine.

`--quiet` - Don't grumble about modules with lessons that cannot be interpreted (see below for what this means).  These modules will be ignored regardless of the presence of this option.  Autotimetable will still emit a warning if a module specified by `--required` is missing (or has been ignored as it was uninterpretable).  This option is processed by `main.cpp` before invoking the Autotimetable engine.

#### Scoring system

Each timetable is scored by a penalty system, and the best timetable is the one that has the lowest penalty of all valid timetables.

`--empty-slot=<unsigned int>` - Sets the penalty for every empty timetable slot that is in-between lesson slots (user needs to wait between lessons).  The default is `1`.

`--travel=<unsigned int>` - Sets the penalty for every day that has at least one lesson (user needs to travel to school).  The default is `8`.

`--no-lunch=<unsigned int>` - Sets the penalty for every day with no empty lunch-time slots (user cannot eat lunch).  The default is `6`.  If `<unsigned int>` is left empty, the penalty is set to `0`.

`--lunch-start=<time>`, `--lunch-end=<time>` - Sets the start and end times for lunch.  `<time>` must be a whole number of hours using the 24-hour clock, e.g. `1200` (12 noon) or `1500` (3 pm).  Non-zero minutes (e.g. `1430`) are not allowed.  The default `--lunch-start` is `1100` and the default `--lunch-end` is `1500`.  Both `--lunch-start` and `--lunch-end` must be specified to change the time period from the default.

Adjusting the relative values of the penalty settings allows Autotimetable to generate the ideal timetable for you :)

## What it can do

* Attempt to minimise time spent in school
* Attempt to leave full days free where possible
* Interpret lessons that are held on odd/even weeks only
* Schedule some time for lunch

## What it cannot do (yet)

* Prioritise modules when it is not possible to schedule all, or when the number of modules exceeds a user-defined maximum
* Prefer some time slots more than others (e.g. no early morning lessons)
* Attempt to minimise total travelling distance
* Attempt to avoid back-to-back modules that are a long distance from each other
* Interpret lessons that are held on custom weeks (i.e. not every week, odd week, or even week) (this is currently treated as a weekly lesson)

## Compilation

Autotimetable should compile in any C++14-compliant compiler.  Autotimetable has been tested to work with MSVC and GCC under Windows, but should work with Linux and Clang too.

For compilation as a command-line program with the included `main.cpp`, several files are included for convenience:

 * Visual Studio 2017 solution and project files (for MSVC/Windows)
 * `compile-gcc.sh` (for GCC/Linux)
 * `compile-gcc.bat` (for GCC/Windows)

Running the GCC compile script for your platform without any modifications should correctly compile the command-line version of Autotimetable.  Of course, your machine needs to have a recent version of GCC that is invokable from the command-line (i.e. added to your PATH).

## Notes

The input `modules.json` file uses the NUSMods API format.  You can download an updated copy of `modules.json` from there.
