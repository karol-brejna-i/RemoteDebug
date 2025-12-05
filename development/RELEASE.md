# Release Procedure

This document describes the steps to release a new version of RemoteDebug.

## Version Management

The version is defined in three places:
- **`src/RemoteDebugCfg.h`** - Source of truth (used at runtime)
- `library.json` - PlatformIO registry
- `library.properties` - Arduino Library Manager

## Release Steps

### 1. Update Version

Edit `src/RemoteDebugCfg.h` and update the version:

```cpp
#define VERSION "X.Y.Z"
```

### 2. Sync Version to Metadata Files

Run the sync script to update `library.json` and `library.properties`:

```bash
./development/sync_version.sh
```

### 3. Update Changelog

Add release notes to `CHANGELOG.md`:

```markdown
## [X.Y.Z] - YYYY-MM-DD

### Added
- New features

### Changed
- Changes to existing functionality

### Fixed
- Bug fixes
```

### 4. Commit Changes

```bash
git add src/RemoteDebugCfg.h library.json library.properties CHANGELOG.md
git commit -m "Release vX.Y.Z"
```

### 5. Create Tag

```bash
git tag -a vX.Y.Z -m "Release vX.Y.Z"
git push origin vX.Y.Z
```

### 6. Push to Main Branch

```bash
git push origin main
```

### 7. Create GitHub Release

1. Go to GitHub → Releases → "Create a new release"
2. Select the tag `vX.Y.Z`
3. Title: `vX.Y.Z`
4. Copy changelog notes to description
5. Publish release

## Version Numbering (Semantic Versioning)

- **MAJOR** (X): Breaking changes, incompatible API changes
- **MINOR** (Y): New features, backward compatible
- **PATCH** (Z): Bug fixes, backward compatible

## CI Verification

The CI automatically verifies that all three version files match. If they don't, the build will fail with:

```
::error::Version mismatch! All files must match src/RemoteDebugCfg.h
Run: ./development/sync_version.sh
```

## Quick Release Checklist

- [ ] Update version in `src/RemoteDebugCfg.h`
- [ ] Run `./development/sync_version.sh`
- [ ] Update `CHANGELOG.md`
- [ ] Commit all changes
- [ ] Create and push tag
- [ ] Create GitHub release
