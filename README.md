# Autotimetable

## Notes

The input `modules.json` file uses the NUSMods API format.  You can download an updated copy of `modules.json` from there.

Autotimetable will **not** detect examination schedule clashes.  Please check the examination schedule from somewhere else (e.g. NUSMods).

## Command-line usage

`autotimetable --modulefile=<filename> --required=<comma-separated module list>`

### Example

`autotimetable --modulefile=modules.json --required=CS1010,MA1101R,CS1231,BN1101,GET1002`

Do not put any spaces in the module list!

## What it can do

* Attempt to leave full days free where possible
* Attempt to minimise time spent in school
* Interpret lessons that are held on odd/even weeks only

## What it cannot do (yet)

* Prioritise modules when it is not possible to schedule all, or when the number of modules exceeds a user-defined maximum
* Prefer some time slots more than others (e.g. no early morning lessons)
* Schedule some time for meals
* Attempt to minimise total travelling distance
* Attempt to avoid back-to-back modules that are a long distance from each other
* Interpret Saturday/Sunday lessons
* Interpret lessons that are held on custom weeks (i.e. not every week, odd week, or even week)
