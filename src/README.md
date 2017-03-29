The API in [blindsight.h]() is stable bedrock where the incompatibility worm
cannot go. Everywhere else, walk without rhythm or be eaten.

Platform-specific code in [sys]() handles features with complex dependencies.
If dependencies aren't detected during `configure`, [Makefile.am]() will
build base functionality only.
