# Autotimetable

Autotimetable is an automatic timetable generator library written in C++ for National University of Singapore (NUS) modules.  It uses a non-probabilistic recursive backtracking algorithm to find the best timetable amongst all possible combinations, hence, it will always generate the *best* timetable possible (of course, this is limited to the evaluation criteria available to Autotimetable (see what it can do below)).

The library is in `autotimetable.cpp` (and the accompanying header file, `autotimetable.hpp`).

Under normal use, speed of the generation engine is usually less than 50 ms and very likely less than 500 ms (counting the time in the Autotimetable algorithm only, not the loading of modules from file).

To make usage more convenient, a `main.cpp` file is provided to allow the library to be compiled as a command-line program, and read module data in NUSMods format.

Note: Autotimetable will **not** detect examination schedule clashes.  Please check the examination schedule from somewhere else (e.g. NUSMods).

## Command-line usage

`autotimetable --modulefile=<filename> --required=<comma-separated module list>`

### Example

`autotimetable --modulefile=modules.json --required=CS1010,MA1101R,CS1231,BN1101,GET1002`

Do not put any spaces in the module list!

## Other options

`--quiet` - Don't grumble about modules with lessons that cannot be interpreted (see below for what this means).  These modules will be ignored regardless of the presence of this option.  Autotimetable will still emit a warning if a module specified by `--required` is missing (or has been ignored as it was uninterpretable).

`--empty-slot=<unsigned int>` - Set the penalty for every empty timetable slot that is in-between lesson slots (user needs to wait between lessons).  The default is `1`.

`--travel=<unsigned int>` - Set the penalty for every day that has at least one lesson (user needs to travel to school).  The default is `10`.

Adjusting the relative values of the penalty settings allows Autotimetable to generate the ideal timetable for you :)

## What it can do

* Attempt to minimise time spent in school
* Attempt to leave full days free where possible
* Interpret lessons that are held on odd/even weeks only

## What it cannot do (yet)

* Prioritise modules when it is not possible to schedule all, or when the number of modules exceeds a user-defined maximum
* Prefer some time slots more than others (e.g. no early morning lessons)
* Schedule some time for meals
* Attempt to minimise total travelling distance
* Attempt to avoid back-to-back modules that are a long distance from each other
* Interpret lessons that are held on custom weeks (i.e. not every week, odd week, or even week) (this will be treated as a weekly lesson)

## Compilation

Autotimetable should compile in any C++14-compliant compiler.  Autotimetable has been tested to work with MSVC and GCC under Windows, but should work with Linux and Clang too.

## Notes

The input `modules.json` file uses the NUSMods API format.  You can download an updated copy of `modules.json` from there.
