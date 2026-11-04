# Transactional updater development

This describes development work after 1.2.0. The public 1.2.0 installer and
desktop build remain unchanged. The native Updates page still only checks
versions; it does not yet expose the preparation controls below.

The builder accepts `--stage-update` with the ordinary disc and installation
arguments. It completes the pinned disc validation, translation, native build,
graphics and language preparation, then places a complete candidate under
`.mkwii-update`. It leaves the installed game, metadata and UserData in place.
It does not create a desktop link in this mode. This is a complete native
rebuild, not an attempt to hot-swap Code.pul.

The candidate has an exact size/SHA-256 inventory of every file. Only GameData
and six owned root files are permitted. Activation rejects changed, missing or
extra files, duplicate JSON keys, unsafe Windows names and reparse points.
The package identity must agree with its native executable and install report.

The new portable launcher detects a prepared candidate on the next normal
open. It copies the installed builder to the local workspace and transfers an
exclusive installation-file handle to that helper. The helper waits for the
old launcher to exit, verifies the candidate through its embedded Python, and
publishes it before opening the updated game. The root launcher can therefore
be replaced after its old process has released the executable. Temporary
runtime files stay inside the installation's workspace.

Publication retains GameData.previous. An already existing previous version
is archived under Backups instead of failing at the end of installation.
Owned root files are backed up with hashes; a journal supports recovery after
either game-directory rename. A committed update is finalized rather than
rolled back after a cleanup interruption. Corrupt recovery backups stop the
operation. A rejected package is preserved under Backups; the previous game
can reopen only when its native hash still matches its install report.

A workspace lock excludes simultaneous builders. The native launch lock
excludes publication while that launcher still owns a running game. UserData
is excluded from package publication; the helper writes its diagnostic log
under UserData/Logs.

Unit coverage includes interruption at both directory renames, root-file
publication failure, retained earlier backups, changed/missing/extra data,
path escapes, external reparse destinations, corrupted recovery data and
concurrent workspace exclusion. Real Windows process tests also cover the
inherited-handle handoff and returning to the previous executable after a
damaged package. These small process fixtures contain no disc assets.

A complete local preparation was additionally run through the embedded builder
with the user's PAL disc while the original lab game stayed alive. All 86
checked persistent files stayed identical during preparation. Opening the
installation again applied the candidate and reached the original title in a
new native process. All 25 checked save/NAND files and the fifteen-language
bank stayed identical. The separate user installation was left running.

The builder's isolated-runtime smoke test now imports the actual worker. The
Windows progress smoke test also checks UTF-8 output containing accented text;
native consumers must not receive JSON in the system's legacy code page.

Pending integration: native preparation/cancellation/status controls and their
fifteen-language labels; automatic update preferences; a verified channel for
new builder versions; full release packaging and deployment. The current pack
remains pinned to 6.13.4. No future upstream-code version or CT 200cc support is
claimed by this work.
