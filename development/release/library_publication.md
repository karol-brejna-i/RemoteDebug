# Publishing the library

This is a short guide (for the maintainers) on how to publish the library in the [PlatformIO Registry](https://registry.platformio.org/) and the Arduino Library Manager registry.

    You need to have permissions to publish the library in the PlatformIO Registry (be a member of the developers team of this project).

## PlatformIO Registry

[PlatformIO Registry](https://registry.platformio.org/) advertises itself as *The world's largest registry for embedded libraries*.

This library is structured according to the [PlatformIO Library Specification](https://docs.platformio.org/en/latest/librarymanager/creating.html) and published in the PlatformIO Registry as [**`"karol-brejna-i/RemoteDebug"`**](https://registry.platformio.org/libraries/karol-brejna-i/RemoteDebug).

For publishing the library in the PlatformIO Registry, perform the following steps:

- Test the library before releasing
- Update the library version (in the sources and in the manifest)
- Update CHANGELOG.md
- Create a GitHub release
- Publish the library in the PlatformIO Registry

### Test the library before releasing

TBD

### Update the library version

Right now, the library version number resides in these places:

- in the `library.json` file
- in the [RemoteDebugCfg.h](../src/RemoteDebugCfg.h) file
- in the [library.properties](../library.properties) file

Update the version number in those places.
(You can `update_version.sh` script to to this for you.)

### Update CHANGELOG.md

Add a new section to the [CHANGELOG.md](../CHANGELOG.md) file.
Use the [Keep a Changelog](https://keepachangelog.com/en/1.0.0/) format.

### Create a GitHub release

Tag the current commit with the version number and create a GitHub release.

If you are using the [GitHub CLI](https://cli.github.com/), you can do it like this:

```bash
VERSION=v4.0.4 # Please, note the "v" prefix. PlatformIO registry "likes it".

VERSION_NO=v${VERSION}
read -r -d '' RELEASE_NOTES << EOM
First Line Text
Second Line Text
Third Line Text
EOM

gh release create $VERSION --title "$VERSION" --notes "$RELEASE_NOTES"
```

### Publish the library in the PlatformIO Registry

Package the library with:

```bash
pio pkg pack
```

Publish the library with:

```bash
pio pkg publish
```

## Arduino Library Manager

The Arduino Library Manager index consumes Git tags and the `library.properties` manifest from this repository. Tags must match the version declared in `library.properties` (Arduino expects plain `X.Y.Z` without the `v` prefix; if you also need a `vX.Y.Z` tag for PlatformIO, push both tags pointing to the same commit).

For publishing a new version to the Arduino Library Manager, perform the following steps:

- Run tests for the library (see `development/RELEASE.md` for the general release workflow)
- Update the library version in `library.properties` (and sync it to the other manifests with `./development/sync_version.sh`)
- Commit the changes
- Create and push a tag named `X.Y.Z` (matching `library.properties`)
- Create a GitHub release using that tag and the changelog notes

### First-time submission (if the library is not yet listed)

- Verify `library.properties` meets Arduino requirements (name, version, author, maintainer, sentence, paragraph, category, url, architectures, depends)
- Create and push the `X.Y.Z` tag and GitHub release as above
- Open a pull request to the [`arduino/library-registry`](https://github.com/arduino/library-registry) repository using their "Add library" workflow, providing this repository URL and the release tag
- Wait for Arduino's CI to index the library; once accepted, future `X.Y.Z` tags will be pulled automatically
